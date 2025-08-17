#ifndef CGROUPS_H
#define CGROUPS_H

#include "utils.h"
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef CGROUP_ROOT_PATH
#define CGROUP_ROOT_PATH "/sys/fs/cgroup"
#endif

#ifndef CGROUPS_PLIMIT_DEFAULT_NAME
#define CGROUPS_PLIMIT_DEFAULT_NAME "plimit"
#endif

#ifndef CGROUPS_PLIMIT_DEFAULT_PATH
#define CGROUPS_PLIMIT_DEFAULT_PATH "/sys/fs/cgroup/plimit"
#endif

#ifndef CGROUPS_DEFAULT_CONTROLLERS_PATH
#define CGROUPS_DEFAULT_CONTROLLERS_PATH "/sys/fs/cgroup/cgroup.controllers"
#endif

/**
 * @struct run_opts_t
 * @brief Options controlling program execution and logging.
 * @var verbose Enable verbose output
 * @var dry_run Simulate actions without making changes
 * @var force   Force actions, ignoring warnings
 */
typedef struct {
  bool verbose;
  bool dry_run;
  bool force;
} run_opts_t;

/**
 * @struct limits_t
 * @brief Describes resource limits and cgroup options for a process.
 * @var pid         Target process ID for cgroup operations.
 * @var cgname      Relative cgroup name (to /sys/fs/cgroup).
 * @var cpu_percent CPU usage limit as a percentage (1..100, 0=unset).
 * @var cpu_quota   CPU quota in microseconds (-1 if unset).
 * @var cpu_period  CPU period in microseconds (-1 if unset).
 * @var cpu_max_raw Raw string for CPU max value (if set, write directly).
 * @var mem_max     Memory limit in bytes (-1 if unset).
 * @var io_max      Array of strings for IO limits ("MAJ:MIN key=val ...",
 * NULL-terminated).
 * @var attach_only If true, only attach to cgroup without setting limits.
 * @var delete_cg   If true, delete the specified cgroup.
 * @var opts        Additional runtime options (verbose, dry-run, force).
 */
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

/**
 * @struct controllers_t
 * @brief Specifies cgroup controllers to enable for a parent cgroup.
 * @var parent Parent cgroup path.
 * @var list   Space-separated list of controllers to enable (e.g., "+cpu
 * +memory +io").
 */
typedef struct {
  const char *parent;
  const char *list;
} controllers_t;

/**
 * @struct controller_opts_t
 * @brief Options for writing a value to a cgroup controller file.
 * @var file  Controller file name (e.g., "cpu.max").
 * @var value Value to write to the controller file.
 */
typedef struct {
  const char *file;
  const char *value;
} controller_opts_t;

/**
 * @brief Apply resource limits and cgroup operations as specified in limits_t.
 * @param lim Pointer to limits_t structure with desired settings.
 * @return PLIMIT_OK on success, error code on failure.
 */
int apply_limits(const limits_t *lim);

/**
 * @brief Add a process into the specified cgroup.
 * @param cgpath Full path to the cgroup.
 * @param pid    Process ID to move.
 * @param opts   Runtime options (verbose, dry-run, etc.).
 * @return PLIMIT_OK on success, error code on failure.
 */
int add_proc_cgroup(const char *cgpath, pid_t pid, const run_opts_t *opts);

/**
 * @brief Remove all processes from a cgroup.
 * @param cgname Cgroup name.
 * @param opts   Runtime options (verbose, dry-run, etc.).
 * @return PLIMIT_OK on success, error code on failure.
 */
int remove_procs_cgroup(const char *cgname, const run_opts_t *opts);

/**
 * @brief Get all processes in a cgroup.
 * @param rc     Pointer to store the return code (PLIMIT_OK or error).
 * @param cgname Cgroup name.
 * @return NULL-terminated array of process IDs, or NULL on error.
 */
char **get_procs_cgroup(int *rc, const char *cgname);

/**
 * @brief Delete a cgroup.
 * @param cgname Cgroup name.
 * @param opts   Runtime options (verbose, dry-run, etc.).
 * @return PLIMIT_OK on success, error code on failure.
 */
int delete_cgroup(const char *cgname, const run_opts_t *opts);

/**
 * @brief Get the full path to a cgroup given its relative name.
 * @param name Relative cgroup name.
 * @return Newly allocated string with full cgroup path (caller must free).
 */
char *cg_full_path(const char *name);

/**
 * @brief Get the parent cgroup path for a given cgroup name.
 * @param name Relative cgroup name.
 * @return Newly allocated string with parent cgroup path (caller must free).
 */
char *cg_parent(const char *name);

/**
 * @brief Check if the system is using cgroup v2.
 * @return 1 if cgroup v2 is present, 0 otherwise
 */
int have_cgroupv2(void);

#endif
