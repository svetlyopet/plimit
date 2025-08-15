#include "cgroups.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *CGROOT = "/sys/fs/cgroup";

char *cg_full_path(const char *name) {
  char *p = NULL;
  if (asprintf(&p, "%s/%s", CGROOT, name) < 0) {
    fatal_sys_errorf(
        "cg_full_path: failed to allocate memory for cgroup path (name=%s)",
        name);
  }
  return p;
}

char *cg_parent(const char *name) {
  if (!name) {
    fatal_sys_errorf("cg_parent: name is NULL");
    return NULL;
  }
  const char *slash = strrchr(name, '/');
  if (!slash) {
    char *p = NULL;
    if (asprintf(&p, "%s/plimit", CGROOT) < 0) {
      fatal_sys_errorf(
          "cg_parent: failed to allocate memory for parent path (name=%s)",
          name);
    }
    return p;
  }
  size_t len = slash - name;
  char *rel = strndup(name, len);
  if (!rel) {
    fatal_sys_errorf(
        "cg_parent: failed to allocate memory for parent substring (name=%s)",
        name);
  }
  char *p = NULL;
  if (asprintf(&p, "%s/%s", CGROOT, rel) < 0) {
    fatal_sys_errorf(
        "cg_parent: failed to allocate memory for parent path (rel=%s)", rel);
  }
  free(rel);
  return p;
}

int enable_controllers(controllers_t controllers, bool dry_run, bool verbose) {
  char path[4096];
  snprintf(path, sizeof(path), "%s/cgroup.subtree_control", controllers.parent);
  // Example: list="+cpu +memory +io"
  if (dry_run) {
    slogf(
        LOG_STDOUT,
        "[dry-run] ACTION: Enable controllers | PATH: '%s' | CONTROLLERS: '%s'",
        path, controllers.list);
    return PLIMIT_OK;
  }
  int rc = write_file(path, controllers.list, false, verbose);
  if (rc != PLIMIT_OK) {
    return PLIMIT_ERR_IO;
  }
  vlogf(verbose,
        "[info] ACTION: Controllers enabled | PATH: '%s' | CONTROLLERS: '%s'",
        path, controllers.list);
  return PLIMIT_OK;
}

static int write_controller(const char *cgpath, controller_opts_t ctrl_opts,
                            const run_opts_t *opts) {
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/%s", cgpath, ctrl_opts.file);
  vlogf(
      opts->verbose,
      "[info] ACTION: Write controller | PATH: '%s' | FILE: '%s' | VALUE: '%s'",
      cgpath, ctrl_opts.file, ctrl_opts.value);
  if (create_file(path, 0644, opts->dry_run, opts->verbose) != PLIMIT_OK) {
    slogf(LOG_STDERR,
          "[error] ACTION: Create file failed | PATH: '%s' | ERRNO: %s", path,
          strerror(errno));
    return PLIMIT_ERR_IO;
  }
  if (write_file(path, ctrl_opts.value, opts->dry_run, opts->verbose) !=
      PLIMIT_OK) {
    slogf(LOG_STDERR,
          "[error] ACTION: Write file failed | PATH: '%s' | ERRNO: %s", path,
          strerror(errno));
    return PLIMIT_ERR_IO;
  }
  return PLIMIT_OK;
}

int move_pid(const char *cgpath, pid_t pid, const run_opts_t *opts) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%d", pid);
  controller_opts_t ctrl_opts = {.file = "cgroup.procs", .value = buf};
  return write_controller(cgpath, ctrl_opts, opts);
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
    fatal_sys_errorf("apply_limits: cgroup v2 not detected at /sys/fs/cgroup");
  }
  if (!is_root()) {
    fatal_sys_errorf("apply_limits: must be run as root or with CAP_SYS_ADMIN");
  }

  if (lim->delete_cg) {
    if (!lim->cgname) {
      fatal_errorf("apply_limits: --delete requires --cgname");
    }
    char *cgpath = cg_full_path(lim->cgname);
    if (!lim->opts.dry_run) {
      if (rmdir(cgpath) != 0) {
        fatal_sys_errorf("apply_limits: failed to delete gcroup directory %s",
                         cgpath);
      }
    } else {
      vlogf(lim->opts.verbose,
            "[dry-run] ACTION: Delete cgroup directory | PATH: '%s'", cgpath);
    }
    free(cgpath);
    return PLIMIT_OK;
  }

  if (!lim->cgname) {
    vlogf(lim->opts.verbose, "[info] missing --cgname (will default to PID)");
  }

  char *cgpath = cg_full_path(lim->cgname);
  char *parent = cg_parent(lim->cgname);

  if (lim->opts.force) {
    if (ensure_dir(parent, 0755, lim->opts.dry_run, lim->opts.verbose) !=
        PLIMIT_OK) {
      fatal_sys_errorf("apply_limits: failed to create parent directory '%s'", parent);
    }
    controllers_t controllers = {.parent = parent, .list = "+cpu +memory +io"};
    enable_controllers(controllers, lim->opts.dry_run, lim->opts.verbose);
  }

  if (ensure_dir(cgpath, 0755, lim->opts.dry_run, lim->opts.verbose) !=
      PLIMIT_OK) {
    fatal_sys_errorf("apply_limits: failed to ensure directory '%s'", cgpath);
  }

  if (!lim->attach_only) {
    if (apply_cpu(cgpath, lim) != PLIMIT_OK) {
      fatal_sys_errorf("apply_limits: failed to apply cpu limits");
    }
    if (apply_mem(cgpath, lim) != PLIMIT_OK) {
      fatal_sys_errorf("apply_limits: failed to apply memory limits");
    }
    if (apply_io(cgpath, lim) != PLIMIT_OK) {
      fatal_sys_errorf("apply_limits: failed to apply io limits");
    }
  }

  if (lim->pid > 0) {
    if (move_pid(cgpath, lim->pid, &lim->opts) != PLIMIT_OK) {
      fatal_sys_errorf("apply_limits: failed to move pid");
    }
    vlogf(lim->opts.verbose, "[info] ACTION: Move PID | PATH: '%s' | PID: %d",
          cgpath, lim->pid);
  }

  free(cgpath);
  free(parent);
  return PLIMIT_OK;
}

int have_cgroupv2(void) {
  struct stat st;
  return stat(DEFAULT_CGROUP_CONTROLLERS_PATH, &st) == 0;
}
