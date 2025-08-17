#include <argtable3.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cgroups.h"
#include "utils.h"

static void print_version(void) {
  log_msg(LOG_NO_PREFIX, "plimit %s", PLIMIT_VERSION);
}

void free_limits(limits_t *lim) {
  if (lim->cgname)
    free(lim->cgname);
  if (lim->cpu_max_raw)
    free(lim->cpu_max_raw);
  if (lim->io_max) {
    for (char **p = lim->io_max; *p; ++p)
      free(*p);
    free((void *)lim->io_max);
  }
}

int main(int argc, char **argv) {
  if (!run_as_root()) {
    log_msg(LOG_ERROR, "must be run as root or with CAP_SYS_ADMIN: %s",
            strerror(errno));
    return PLIMIT_ERR_PERM;
  }

  struct arg_lit *help = arg_lit0("h", "help", "show this help");
  struct arg_lit *version = arg_lit0("v", "version", "show version");
  struct arg_int *pid = arg_int0(
      "p", "pid", "PID", "target PID (required unless --delete + --cgname)");
  struct arg_int *cpu_percent =
      arg_int0(NULL, "cpu-percent", "N", "limit CPU to N%% of a single CPU");
  struct arg_int *cpu_quota =
      arg_int0(NULL, "cpu-quota", "US", "quota in µs (requires --cpu-period)");
  struct arg_int *cpu_period =
      arg_int0(NULL, "cpu-period", "US", "period in µs (requires --cpu-quota)");
  struct arg_str *cpu_max =
      arg_str0(NULL, "cpu-max", "STR",
               "direct cpu.max string (e.g. \"max\" or \"50000 100000\")");
  struct arg_str *mem_max =
      arg_str0(NULL, "mem-max", "SIZE", "memory.max with K/M/G suffix");
  struct arg_str *io_max = arg_strn(NULL, "io-max", "STR", 0, 16,
                                    "io.max entries (MAJ:MIN rbps=... etc.)");
  struct arg_str *cgname =
      arg_str0(NULL, "cgname", "NAME", "cgroup name (default plimit/<pid>)");
  struct arg_lit *attach_only =
      arg_lit0(NULL, "attach-only", "move PID only, don't change limits");
  struct arg_lit *delete_cg =
      arg_lit0(NULL, "delete", "delete the cgroup (requires --cgname)");
  struct arg_lit *dry_run =
      arg_lit0(NULL, "dry-run", "print actions without making changes");
  struct arg_lit *force =
      arg_lit0(NULL, "force", "create parents and enable controllers");
  struct arg_lit *verbose = arg_lit0(NULL, "verbose", "extra logging");

  struct arg_end *end = arg_end(20);
  void *argtable[] = {help,      version,    pid,         cpu_percent,
                      cpu_quota, cpu_period, cpu_max,     mem_max,
                      io_max,    cgname,     attach_only, delete_cg,
                      dry_run,   force,      verbose,     end};

  int nerrors = arg_parse(argc, argv, argtable);
  if (help->count) {
    print_version();
    log_msg(LOG_NO_PREFIX, "Usage: plimit [options]\n\n");
    arg_print_glossary(stdout, argtable, "  %-25s %s\n");
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return PLIMIT_OK;
  }
  if (version->count) {
    print_version();
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return PLIMIT_OK;
  }
  if (nerrors > 0) {
    arg_print_errors(stdout, end, "plimit");
    log_msg(LOG_NO_PREFIX, "Try --help for more information.");
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return PLIMIT_ERR_ARG;
  }

  limits_t lim;
  memset(&lim, 0, sizeof(lim));
  lim.cpu_quota = -1;
  lim.cpu_period = -1;
  lim.mem_max = -1;
  lim.io_max = NULL;
  lim.attach_only = attach_only->count > 0;
  lim.delete_cg = delete_cg->count > 0;
  lim.opts.verbose = verbose->count > 0;
  lim.opts.dry_run = dry_run->count > 0;
  lim.opts.force = force->count > 0;

  if (lim.opts.verbose && lim.opts.dry_run) {
    log_msg(LOG_PREFIX, "--verbose and --dry-run cannot be used together");
    log_msg(LOG_NO_PREFIX, "Try --help for more information.");
    free_limits(&lim);
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return PLIMIT_ERR_ARG;
  }

  if (pid->count) {
    lim.pid = (pid_t)pid->ival[0];
  }
  if (cpu_percent->count) {
    lim.cpu_percent = cpu_percent->ival[0];
  }
  if (cpu_quota->count) {
    lim.cpu_quota = cpu_quota->ival[0];
  }
  if (cpu_period->count) {
    lim.cpu_period = cpu_period->ival[0];
  }
  if (cpu_max->count) {
    lim.cpu_max_raw = strdup(cpu_max->sval[0]);
  }
  if (mem_max->count) {
    lim.mem_max = parse_bytes(mem_max->sval[0]);
  }
  if (io_max->count) {
    size_t n = io_max->count;
    lim.io_max = (char **)calloc(n + 1, sizeof(char *));
    for (size_t i = 0; i < n; i++) {
      lim.io_max[i] = strdup(io_max->sval[i]);
    }
    lim.io_max[n] = NULL;
  }
  if (cgname->count) {
    lim.cgname = strdup(cgname->sval[0]);
  }

  if (!lim.cgname && lim.delete_cg) {
    log_msg(LOG_PREFIX, "--cgname is required with --delete");
    log_msg(LOG_NO_PREFIX, "Try --help for more information.");
    free_limits(&lim);
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return PLIMIT_ERR_ARG;
  }

  if ((!lim.delete_cg && !lim.cgname) && lim.pid <= 0) {
    log_msg(LOG_PREFIX, "--pid is required unless both --delete and "
                        "--cgname are not set");
    log_msg(LOG_NO_PREFIX, "Try --help for more information.");
    free_limits(&lim);
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return PLIMIT_ERR_ARG;
  }

  if (lim.cgname && lim.delete_cg) {
    int rc = delete_cgroup(lim.cgname, &lim.opts);
    free_limits(&lim);
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return rc;
  }

  if (!lim.cgname && lim.pid > 0) {
    if (asprintf(&lim.cgname, "%s/%d", CGROUPS_PLIMIT_DEFAULT_NAME, lim.pid) <
        0) {
      log_msg(LOG_ERROR, "failed to allocate memory for cgroup name (pid=%d)",
              lim.pid);
      free_limits(&lim);
      arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
      return PLIMIT_ERR_MEM;
    }
  }

  if ((lim.cpu_quota > 0 && lim.cpu_period <= 0) ||
      (lim.cpu_period > 0 && lim.cpu_quota <= 0)) {
    log_msg(LOG_PREFIX,
            "both --cpu-quota and --cpu-period are required together");
    log_msg(LOG_NO_PREFIX, "Try --help for more information.");
    free_limits(&lim);
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return PLIMIT_ERR_ARG;
  }

  if (!lim.cgname) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", lim.pid);
    lim.cgname = strdup(buf);
    if (!lim.cgname) {
      log_msg(LOG_ERROR, "failed to allocate memory for cgroup name (pid=%d)",
              lim.pid);
      return PLIMIT_ERR_MEM;
    }
    if (lim.opts.verbose) {
      log_msg(LOG_INFO, "--cgname not set, defaulting cgroup name to '%s'",
              lim.cgname);
    }
  }

  int rc = apply_limits(&lim);
  if (rc != PLIMIT_OK) {
    free_limits(&lim);
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return rc;
  }

  if (lim.opts.dry_run) {
    log_msg(LOG_DRY_RUN, "applied cgroup %s for PID %d", lim.cgname, lim.pid);
    free_limits(&lim);
    arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
    return PLIMIT_OK;
  }

  log_msg(LOG_NO_PREFIX, "applied cgroup %s for PID %d", lim.cgname, lim.pid);
  free_limits(&lim);
  arg_freetable((void **)argtable, sizeof(argtable) / sizeof(argtable[0]));
  return PLIMIT_OK;
}
