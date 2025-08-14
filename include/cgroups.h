#ifndef CGROUPS_H
#define CGROUPS_H

#include "utils.h"
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
  pid_t pid;
  char *cgname;         // relative to /sys/fs/cgroup
  int cpu_percent;      // 1..100, 0=unset
  long long cpu_quota;  // us, -1 unset
  long long cpu_period; // us, -1 unset
  char *cpu_max_raw;    // if set, write directly
  long long mem_max;    // bytes, -1 unset
  char **io_max; // array of strings "MAJ:MIN key=val ...", NULL-terminated
  bool attach_only;
  bool delete_cg;
  run_opts_t opts;
} limits_t;

typedef struct {
  const char *parent;
  const char *list;
} controllers_t;

typedef struct {
  const char *file;
  const char *value;
} controller_opts_t;

int apply_limits(const limits_t *lim);
int move_pid(const char *cgpath, pid_t pid, const run_opts_t *opts);
char *cg_full_path(const char *name);
char *cg_parent(const char *name);

#endif
