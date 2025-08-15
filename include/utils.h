#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PLIMIT_VERSION
#define PLIMIT_VERSION "0.0.0"
#endif

/**
 * @enum plimit_err_t
 * @brief Error codes for plimit utilities.
 */
typedef enum {
  PLIMIT_OK = 0,       ///< Success
  PLIMIT_ERR_GENERIC,  ///< Generic error
  PLIMIT_ERR_ARG,      ///< Invalid argument
  PLIMIT_ERR_PERM,     ///< Permission denied
  PLIMIT_ERR_NOTFOUND, ///< File or resource not found
  PLIMIT_ERR_EXISTS,   ///< Already exists
  PLIMIT_ERR_IO,       ///< IO error
  PLIMIT_ERR_MEM,      ///< Memory allocation failure
  PLIMIT_ERR_PARSE,    ///< Parsing error
  PLIMIT_ERR_CGROUP,   ///< Cgroup operation error
  PLIMIT_ERR_SYS,      ///< System call failure
} plimit_err_t;

/**
 * @enum log_dest_t
 * @brief Specifies the destination for logging output.
 */
typedef enum { LOG_STDOUT = 1, LOG_STDERR } log_dest_t;

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
 * @brief Log a formatted message to stdout or stderr.
 * @param dest Destination for the log (LOG_STDOUT or LOG_STDERR)
 * @param fmt Format string
 * @param ... Arguments for format string
 */
void slogf(log_dest_t dest, const char *fmt, ...);

/**
 * @brief Conditionally log a formatted message to stdout if enabled.
 * @param enabled If true, log the message
 * @param fmt Format string
 * @param ... Arguments for format string
 */
void vlogf(bool enabled, const char *fmt, ...);

/**
 * @brief Print a fatal error message and exit the program.
 * @param fmt Format string
 * @param ... Arguments for format string
 */
void fatal_errorf(const char *fmt, ...);

/**
 * @brief Print a fatal error message including system error and exit.
 * @param fmt Format string
 * @param ... Arguments for format string
 */
void fatal_sys_errorf(const char *fmt, ...);

/**
 * @brief Create an empty file at the specified path.
 * @param path File path
 * @param mode File mode/permissions
 * @param dry_run If true, simulate the action
 * @param verbose If true, log the action
 * @return PLIMIT_OK on success, error code on failure
 */
int create_file(const char *path, mode_t mode, bool dry_run, bool verbose);

/**
 * @brief Write data to a file, overwriting any existing content.
 * @param path File path
 * @param data Data to write
 * @param dry_run If true, simulate the action
 * @param verbose If true, log the action
 * @return PLIMIT_OK on success, error code on failure
 */
int write_file(const char *path, const char *data, bool dry_run, bool verbose);

/**
 * @brief Append data to a file.
 * @param path File path
 * @param data Data to append
 * @param dry_run If true, simulate the action
 * @param verbose If true, log the action
 * @return PLIMIT_OK on success, error code on failure
 */
int append_file(const char *path, const char *data, bool dry_run, bool verbose);

/**
 * @brief Parse a string representing a size (K, M, G, T, P, E) into bytes.
 * @param s Input string
 * @return Parsed value in bytes, or error code on failure
 */
long long parse_bytes(const char *s);

/**
 * @brief Parse a string as a long long integer.
 * @param s Input string
 * @param name Name for error reporting
 * @return Parsed value, or error code on failure
 */
long long parse_ll(const char *s, const char *name);

/**
 * @brief Ensure a directory exists, creating it if necessary.
 * @param path Directory path
 * @param mode Directory permissions
 * @param dry_run If true, simulate the action
 * @param verbose If true, log the action
 * @return PLIMIT_OK on success, error code on failure
 */
int ensure_dir(const char *path, mode_t mode, bool dry_run, bool verbose);

/**
 * @brief Check if the current user is root.
 * @return 1 if root, 0 otherwise
 */
int is_root(void);

#endif
