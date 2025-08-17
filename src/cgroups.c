#include "cgroups.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const int CGFILE_PERM = 0644;

char *cg_full_path(const char *name) {
  char *p = NULL;
  if (!name) {
    log_msg(LOG_ERROR, "cg_full_path: name is NULL");
    return NULL;
  }
  if (!strchr(name, '/')) {
    // name does not contain '/', use CGROUPS_PLIMIT_DEFAULT_PATH as parent
    if (asprintf(&p, "%s/%s", CGROUPS_PLIMIT_DEFAULT_PATH, name) < 0) {
      log_msg(LOG_ERROR, "failed to allocate memory for cgroup path (name=%s)",
              name);
      free(p);
      return NULL;
    }
  } else {
    // name contains '/', use its path as parent
    if (asprintf(&p, "%s/%s", CGROUP_ROOT_PATH, name) < 0) {
      log_msg(LOG_ERROR, "failed to allocate memory for cgroup path (name=%s)",
              name);
      free(p);
      return NULL;
    }
  }
  return p;
}

char *cg_parent(const char *name) {
  char *p = NULL;
  if (!name || !*name) {
    log_msg(LOG_ERROR, "cgroup name is empty");
    free(p);
    return NULL;
  }
  const char *slash = strrchr(name, '/');
  size_t len = slash - name;
  char *rel = strndup(name, len);
  if (!rel) {
    log_msg(LOG_ERROR,
            "failed to allocate memory for parent substring (name=%s)", name);
    free(p);
    free(rel);
    return NULL;
  }
  if (asprintf(&p, "%s/%s", CGROUP_ROOT_PATH, rel) < 0) {
    log_msg(LOG_ERROR, "failed to allocate memory for parent path (rel=%s)",
            rel);
    free(p);
    free(rel);
    return NULL;
  }
  free(rel);
  return p;
}

int enable_controllers(controllers_t controllers, const run_opts_t *opts) {
  char path[4096];
  snprintf(path, sizeof(path), "%s/cgroup.subtree_control", controllers.parent);
  file_write_args_t file_args = {
      .path = path, .data = controllers.list, .mode = CGFILE_PERM};
  int rc = write_file(opts->dry_run, &file_args, opts->verbose);
  if (rc != PLIMIT_OK) {
    log_msg(LOG_ERROR, "failed to enable controllers '%s' in file %s: %s",
            controllers.list, path, strerror(errno));
    return PLIMIT_ERR_IO;
  }
  return PLIMIT_OK;
}

static int write_controller(const char *cgpath, controller_opts_t ctrl_opts,
                            const run_opts_t *opts) {
  int rc;
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/%s", cgpath, ctrl_opts.file);
  file_write_args_t file_args = {
      .path = path, .data = ctrl_opts.value, .mode = CGFILE_PERM};
  rc = write_file(opts->dry_run, &file_args, opts->verbose);
  if (rc != PLIMIT_OK) {
    return rc;
  }
  return PLIMIT_OK;
}

int add_proc_cgroup(const char *cgpath, pid_t pid, const run_opts_t *opts) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%d", pid);
  controller_opts_t ctrl_opts = {.file = "cgroup.procs", .value = buf};
  return write_controller(cgpath, ctrl_opts, opts);
}

int remove_procs_cgroup(const char *cgname, const run_opts_t *opts) {
  char *cgpath = cg_full_path(cgname);
  char *procsfilepath = NULL;
  if (asprintf(&procsfilepath, "%s/cgroup.procs", cgpath) < 0) {
    log_msg(LOG_ERROR, "failed to allocate memory for cgroup procs file (cgname=%s)",
            cgname);
    return PLIMIT_ERR_MEM;
  }

  file_write_args_t file_args = {
    .path = procsfilepath,
    .data = "",
    .mode = CGFILE_PERM
  };
  return write_file(opts->dry_run, &file_args, opts->verbose);
}

char **get_procs_cgroup(int *rc, const char *cgname) {
  *rc = PLIMIT_OK;
  char *cgpath = cg_full_path(cgname);
  if (!cgpath) {
    log_msg(LOG_ERROR, "cgpath is NULL");
    *rc = PLIMIT_ERR_MEM;
    return NULL;
  }

  char *procsfilepath = NULL;
  if (asprintf(&procsfilepath, "%s/cgroup.procs", cgpath) < 0) {
    log_msg(LOG_ERROR, "failed to allocate memory for cgroup procs file (cgname=%s)",
            cgname);
    *rc = PLIMIT_ERR_MEM;
    return NULL;
  }

  FILE *fp = fopen(procsfilepath, "r");
  if (!fp) {
    log_msg(LOG_ERROR, "failed to open file '%s': %s", procsfilepath, strerror(errno));
    *rc = PLIMIT_ERR_IO;
    return NULL;
  }

  size_t procs_alloc = 8;
  size_t procs_count = 0;
  char **procs = (char **)malloc(procs_alloc * sizeof(char *));
  if (!procs) {
    log_msg(LOG_ERROR, "memory allocation failed");
    fclose(fp);
    *rc = PLIMIT_ERR_MEM;
    return NULL;
  }

  char buf[256];
  while (fgets(buf, sizeof(buf), fp)) {
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    if (procs_count == procs_alloc) {
      procs_alloc *= 2;
      char **tmp = (char **)realloc((void *)procs, procs_alloc * sizeof(char *));
      if (!tmp) {
        log_msg(LOG_ERROR, "memory allocation failed");
        for (size_t i = 0; i < procs_count; ++i) free(procs[i]);
        free((void *)procs);
        fclose(fp);
        *rc = PLIMIT_ERR_MEM;
        return NULL;
      }
      procs = tmp;
    }
    procs[procs_count] = strdup(buf);
    if (!procs[procs_count]) {
      log_msg(LOG_ERROR, "memory allocation failed");
      for (size_t i = 0; i < procs_count; ++i) free(procs[i]);
      free((void *)procs);
      fclose(fp);
      *rc = PLIMIT_ERR_MEM;
      return NULL;
    }
    procs_count++;
  }
  fclose(fp);

  if (procs_count == procs_alloc) {
    char **tmp = (char **)realloc((void *)procs, (procs_alloc + 1) * sizeof(char *));
    if (tmp) procs = tmp;
  }
  procs[procs_count] = NULL;

  *rc = PLIMIT_OK;
  return procs;
}

int delete_cgroup(const char *cgname, const run_opts_t *opts) {
  char *cgpath = cg_full_path(cgname);
  if (opts->dry_run) {
    log_msg(LOG_DRY_RUN, "delete cgroup directory %s", cgpath);
    return PLIMIT_OK;
  }
  if (rmdir(cgpath) != 0) {
    log_msg(LOG_ERROR, "failed to delete cgroup %s: %s", cgpath,
            strerror(errno));
    return PLIMIT_ERR_IO;
  }
  log_msg(LOG_INFO, "deleted cgroup directory %s", cgpath);
  return PLIMIT_OK;
}

static int apply_cpu(const char *cgpath, const limits_t *lim) {
  if (lim->cpu_max_raw) {
    controller_opts_t ctrl_opts = {.file = "cpu.max",
                                   .value = lim->cpu_max_raw};
    return write_controller(cgpath, ctrl_opts, &lim->opts);
  }
  if (lim->cpu_percent > 0) {
    long long period = 100000;
    long long quota = (period * lim->cpu_percent) / 100;
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld %lld", quota, period);
    controller_opts_t ctrl_opts = {.file = "cpu.max", .value = buf};
    return write_controller(cgpath, ctrl_opts, &lim->opts);
  }
  if (lim->cpu_quota > 0 && lim->cpu_period > 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld %lld", lim->cpu_quota, lim->cpu_period);
    controller_opts_t ctrl_opts = {.file = "cpu.max", .value = buf};
    return write_controller(cgpath, ctrl_opts, &lim->opts);
  }
  return PLIMIT_OK;
}

static int apply_mem(const char *cgpath, const limits_t *lim) {
  if (lim->mem_max > 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", lim->mem_max);
    controller_opts_t ctrl_opts = {.file = "memory.max", .value = buf};
    return write_controller(cgpath, ctrl_opts, &lim->opts);
  }
  return PLIMIT_OK;
}

static int apply_io(const char *cgpath, const limits_t *lim) {
  if (!lim->io_max) {
    return PLIMIT_OK;
  }
  for (char **p = lim->io_max; *p; ++p) {
    controller_opts_t ctrl_opts = {.file = "io.max", .value = *p};
    if (write_controller(cgpath, ctrl_opts, &lim->opts) != PLIMIT_OK) {
      return PLIMIT_ERR_IO;
    }
  }
  return PLIMIT_OK;
}

int apply_limits(const limits_t *lim) {
  if (!have_cgroupv2()) {
    log_msg(LOG_ERROR, "cgroup v2 not detected at /sys/fs/cgroup: %s",
            strerror(errno));
    return PLIMIT_ERR_NOTFOUND;
  }

  char *cgpath = cg_full_path(lim->cgname);
  char *parent = cg_parent(lim->cgname);

  if (lim->opts.force) {
    if (create_directory(lim->opts.dry_run, parent, 0755, lim->opts.verbose) !=
        PLIMIT_OK) {
      log_msg(LOG_ERROR, "failed to create parent directory '%s': %s", parent,
              strerror(errno));
      free(cgpath);
      free(parent);
      return PLIMIT_ERR_IO;
    }
    controllers_t controllers = {.parent = parent,
                                 .list = "+cpu +memory +io +pids"};
    int rc = enable_controllers(controllers, &lim->opts);
    if (rc != PLIMIT_OK) {
      log_msg(LOG_ERROR, "failed to enable controllers for parent '%s': %s",
              parent, strerror(errno));
      free(cgpath);
      free(parent);
      return rc;
    }
  }

  if (create_directory(lim->opts.dry_run, cgpath, 0755, lim->opts.verbose) !=
      PLIMIT_OK) {
    log_msg(LOG_ERROR, "failed to create cgroup directory '%s': %s", cgpath,
            strerror(errno));
    free(cgpath);
    free(parent);
    return PLIMIT_ERR_IO;
  }

  if (lim->pid > 0) {
    if (add_proc_cgroup(cgpath, lim->pid, &lim->opts) != PLIMIT_OK) {
      log_msg(LOG_ERROR, "failed to add pid to cgroup: %s", strerror(errno));
      free(cgpath);
      free(parent);
      return PLIMIT_ERR_IO;
    }
  }

  if (!lim->attach_only) {
    int rc;
    rc = apply_cpu(cgpath, lim);
    if (rc != PLIMIT_OK) {
      log_msg(LOG_ERROR, "failed to apply cpu limits");
      free(cgpath);
      free(parent);
      return PLIMIT_ERR_CGROUP;
    }
    rc = apply_mem(cgpath, lim);
    if (rc != PLIMIT_OK) {
      log_msg(LOG_ERROR, "failed to apply memory limits");
      free(cgpath);
      free(parent);
      return PLIMIT_ERR_CGROUP;
    }
    rc = apply_io(cgpath, lim);
    if (rc != PLIMIT_OK) {
      log_msg(LOG_ERROR, "failed to apply io limits");
      free(cgpath);
      free(parent);
      return rc;
    }
  }

  free(cgpath);
  free(parent);
  return PLIMIT_OK;
}

int have_cgroupv2(void) {
  struct stat st;
  return stat(CGROUPS_DEFAULT_CONTROLLERS_PATH, &st) == 0;
}
