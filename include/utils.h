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
  PLIMIT_OK = 0,       // Success
  PLIMIT_ERR_GENERIC,  // Generic error
  PLIMIT_ERR_ARG,      // Invalid argument
  PLIMIT_ERR_PERM,     // Permission denied
  PLIMIT_ERR_NOTFOUND, // File or resource not found
  PLIMIT_ERR_EXISTS,   // Already exists
  PLIMIT_ERR_IO,       // IO error
  PLIMIT_ERR_MEM,      // Memory allocation failure
  PLIMIT_ERR_PARSE,    // Parsing error
  PLIMIT_ERR_CGROUP,   // Cgroup operation error
  PLIMIT_ERR_SYS,      // System call failure
} plimit_err_t;

/**
 * @enum log_type_t
 * @brief Specifies the type of log message.
 * @var LOG_INFO  Informational message
 * @var LOG_DEBUG Debug message
 * @var LOG_WARN  Warning message
 * @var LOG_ERROR Error message
 */
typedef enum {
  LOG_NO_PREFIX = 0, // No prefix, used for generic log messages
  LOG_PREFIX,        // Log with prefix, not a message type
  LOG_DRY_RUN,       // Dry run message
  LOG_INFO,          // Informational message
  LOG_DEBUG,         // Debug message
  LOG_WARN,          // Warning message
  LOG_ERROR,         // Error message
} log_type_t;

/**
 * @brief Log a formatted message to stdout or stderr.
 * @param type Type of the log message
 * @param fmt Format string
 * @param ... Arguments for format string
 */
void log_msg(log_type_t type, const char *fmt, ...);

/**
 * @struct file_write_args_t
 * @brief Arguments for writing data to a file.
 * @var path Path to the file
 * @var data Data to write
 * @var mode File permissions
 */
typedef struct {
  const char *path;
  const char *data;
  mode_t mode;
} file_write_args_t;

/**
 * @brief Write data to a file, overwriting any existing content.
 * @param dry_run Only log the action without executing it
 * @param args File write arguments
 * @param verbose Log the action
 * @return PLIMIT_OK on success, error code on failure
 */
int write_file(bool dry_run, const file_write_args_t *args, bool verbose);

/**
 * @brief Creates a directory if not exists or set mode.
 * @param dry_run Only log the action without executing it
 * @param path Directory path
 * @param mode Directory permissions
 * @param verbose Log the action
 * @return PLIMIT_OK on success, error code on failure
 */
int create_directory(bool dry_run, const char *path, mode_t mode, bool verbose);

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
 * @brief Check if the program is being run as root.
 * @return 1 if root, 0 otherwise
 */
int run_as_root(void);

#endif
