#include "cgroups.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *CGROOT = "/sys/fs/cgroup";

static char *xstrdup(const char *s) {
  size_t n = strlen(s) + 1;
  char *p = malloc(n);
  if (!p) {
    sysdie("oom");
    free(p);
    return NULL;
  }
  memcpy(p, s, n);
  return p;
}

char *cg_full_path(const char *name) {
  char *p = NULL;
  if (asprintf(&p, "%s/%s", CGROOT, name) < 0) {
    sysdie("oom");
  }
  return p;
}

char *cg_parent(const char *name) {
  if (!name) {
    sysdie("cg_parent: name is NULL");
    return NULL;
  }
  const char *slash = strrchr(name, '/');
  if (!slash) {
    return xstrdup(CGROOT);
  }
  size_t len = slash - name;
  char *rel = strndup(name, len);
  if (!rel) {
    sysdie("oom");
  }
  char *p = NULL;
  if (asprintf(&p, "%s/%s", CGROOT, rel) < 0) {
    sysdie("oom");
  }
  free(rel);
  return p;
}

int enable_controllers(controllers_t controllers, bool dry_run, bool verbose) {
  char path[4096];
  snprintf(path, sizeof(path), "%s/cgroup.subtree_control", controllers.parent);
  // Example: list="+cpu +memory +io"
  if (dry_run) {
    vlogf(verbose, "[dry-run] echo '%s' > %s", controllers.list, path);
    return 0;
  }
  int rc = write_file(path, controllers.list, false, verbose);
  if (rc != 0) {
    return -1;
  }
  return 0;
}

static int write_controller(const char *cgpath, controller_opts_t ctrl_opts,
                            const run_opts_t *opts) {
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/%s", cgpath, ctrl_opts.file);
  vlogf(opts->verbose, "write %s = '%s'", path, ctrl_opts.value);
  if (write_file(path, ctrl_opts.value, opts->dry_run, opts->verbose) != 0) {
    write_stderr("error writing %s: %s\n", path, strerror(errno));
    return -1;
  }
  return 0;
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
  return 0;
}

static int apply_mem(const char *cgpath, const limits_t *lim) {
  if (lim->mem_max > 0) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", lim->mem_max);
    controller_opts_t ctrl_opts = {.file = "memory.max", .value = buf};
    return write_controller(cgpath, ctrl_opts, &lim->opts);
  }
  return 0;
}

static int apply_io(const char *cgpath, const limits_t *lim) {
  if (!lim->io_max) {
    return 0;
  }
  for (char **p = lim->io_max; *p; ++p) {
    controller_opts_t ctrl_opts = {.file = "io.max", .value = *p};
    if (write_controller(cgpath, ctrl_opts, &lim->opts) != 0) {
      return -1;
    }
  }
  return 0;
}

int apply_limits(const limits_t *lim) {
  if (!have_cgroupv2()) {
    die("cgroup v2 not detected at /sys/fs/cgroup");
  }
  if (!is_root()) {
    die("must be run as root or with CAP_SYS_ADMIN");
  }

  if (lim->delete_cg) {
    if (!lim->cgname) {
      die("--delete requires --cgname");
    }
    char *cgpath = cg_full_path(lim->cgname);
    if (!lim->opts.dry_run) {
      if (rmdir(cgpath) != 0) {
        sysdie("failed to delete %s", cgpath);
      }
    } else {
      vlogf(lim->opts.verbose, "[dry-run] rmdir %s", cgpath);
    }
    free(cgpath);
    return 0;
  }

  if (!lim->cgname) {
    die("missing --cgname (will default from PID in main)");
  }

  char *cgpath = cg_full_path(lim->cgname);
  char *parent = cg_parent(lim->cgname);

  if (lim->opts.force) {
    if (ensure_dir(parent, 0755, lim->opts.dry_run, lim->opts.verbose) != 0) {
      sysdie("mkdir %s", parent);
    }
    controllers_t controllers = {.parent = parent, .list = "+cpu +memory +io"};
    enable_controllers(controllers, lim->opts.dry_run, lim->opts.verbose);
  }

  if (ensure_dir(cgpath, 0755, lim->opts.dry_run, lim->opts.verbose) != 0) {
    sysdie("mkdir %s", cgpath);
  }

  if (!lim->attach_only) {
    if (apply_cpu(cgpath, lim) != 0) {
      die("failed to apply cpu limits");
    }
    if (apply_mem(cgpath, lim) != 0) {
      die("failed to apply memory limits");
    }
    if (apply_io(cgpath, lim) != 0) {
      die("failed to apply io limits");
    }
  }

  if (lim->pid > 0) {
    if (move_pid(cgpath, lim->pid, &lim->opts) != 0) {
      die("failed to move pid");
    }
  }

  free(cgpath);
  free(parent);
  return 0;
}
