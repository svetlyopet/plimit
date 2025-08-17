#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static long long pow2(int e) {
  long long v = 1;
  while (e-- > 0) {
    v <<= 1;
  }
  return v;
}

void log_msg(log_type_t type, const char *fmt, ...) {
  FILE *stream = (type == LOG_ERROR) ? stderr : stdout;
  const char *type_prefix;
  switch (type) {
  case LOG_PREFIX:
    type_prefix = "plimit: ";
    break;
  case LOG_DRY_RUN:
    type_prefix = "dry-run: ";
    break;
  case LOG_INFO:
    type_prefix = "info: ";
    break;
  case LOG_DEBUG:
    type_prefix = "debug: ";
    break;
  case LOG_WARN:
    type_prefix = "warn: ";
    break;
  case LOG_ERROR:
    type_prefix = "error: ";
    break;
  default:
    type_prefix = "";
    break;
  }
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  fputs(type_prefix, stream);
  fputs(buf, stream);
  fputc('\n', stream);

  fflush(stream);
}

int write_file(bool dry_run, const file_write_args_t *args, bool verbose) {
  if (!args || !args->path) {
    return PLIMIT_ERR_ARG;
  }
  // we first check if the dry-run flag is set and do not perform the access
  // check since we expect that the parent directory if the file might not exist
  // yet
  if (dry_run) {
    if (strcmp(args->data, "") == 0) {
      log_msg(LOG_DRY_RUN, "truncate file %s", args->path);
    } else {
      log_msg(LOG_DRY_RUN, "write '%s' to file %s", args->data, args->path);
    }
    return PLIMIT_OK;
  }
  if (access(args->path, W_OK) != 0) {
    log_msg(LOG_ERROR, "cannot write to file '%s': %s", args->path,
            strerror(errno));
    return PLIMIT_ERR_IO;
  }
  int fd;
  if (strcmp(args->data, "") == 0) {
    // Open with O_WRONLY | O_TRUNC to truncate the file
    fd = open(args->path, O_WRONLY | O_TRUNC);
    if (fd < 0) {
      log_msg(LOG_ERROR, "cannot truncate file '%s': %s", args->path, strerror(errno));
      return PLIMIT_ERR_IO;
    }
    close(fd);
    if (verbose) {
      log_msg(LOG_INFO, "truncate file %s", args->path);
    }
    return PLIMIT_OK;
  }
  
  // Open with O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC to write data
  fd = open(args->path, O_WRONLY | O_CREAT | O_CLOEXEC | O_TRUNC, args->mode);
  if (fd < 0) {
    return PLIMIT_ERR_IO;
  }
  size_t len = strlen(args->data);
  ssize_t n = write(fd, args->data, len);
  close(fd);
  if (n < 0 || (size_t)n != len) {
    log_msg(LOG_ERROR, "failed to write to file '%s': %s", args->path,
            strerror(errno));
    return PLIMIT_ERR_IO;
  }
  if (verbose) {
    if (strcmp(args->data, "") == 0) {
      log_msg(LOG_INFO, "truncate file %s", args->path);
    } else {
      log_msg(LOG_INFO, "write '%s' to file %s", args->data, args->path);
    }
  }
  return PLIMIT_OK;
}

int create_directory(bool dry_run, const char *path, mode_t mode,
                     bool verbose) {
  struct stat st;
  if (stat(path, &st) == 0) {
    if (!S_ISDIR(st.st_mode)) {
      errno = ENOTDIR;
      return PLIMIT_ERR_IO;
    }
    return PLIMIT_OK;
  }
  char parent[PATH_MAX];
  strncpy(parent, path, PATH_MAX);
  parent[PATH_MAX - 1] = '\0';
  char *slash = strrchr(parent, '/');
  if (slash) {
    *slash = '\0';
    if (access(parent, W_OK) != 0) {
      log_msg(LOG_DRY_RUN,
              "cannot create directory '%s': parent '%s' is not writable (%s)",
              path, parent, strerror(errno));
      return PLIMIT_ERR_PERM;
    }
  }
  if (dry_run) {
    log_msg(LOG_DRY_RUN, "create directory %s", path);
    return PLIMIT_OK;
  }
  if (mkdir(path, mode) == 0) {
    if (verbose) {
      log_msg(LOG_INFO, "create directory %s", path);
    }
    return PLIMIT_OK;
  }
  return PLIMIT_ERR_IO;
}

long long parse_bytes(const char *s) {
  if (!s || !*s) {
    return PLIMIT_ERR_ARG;
  }
  char *end = NULL;
  errno = 0;
  long double v = strtold(s, &end);
  if (errno != 0) {
    return PLIMIT_ERR_PARSE;
  }
  long long mul = 1;
  if (*end) {
    if (strcasecmp(end, "K") == 0) {
      mul = 1024LL;
    } else if (strcasecmp(end, "M") == 0) {
      mul = 1024LL * 1024;
    } else if (strcasecmp(end, "G") == 0) {
      mul = pow2(30);
    } else if (strcasecmp(end, "T") == 0) {
      mul = pow2(40);
    } else if (strcasecmp(end, "P") == 0) {
      mul = pow2(50);
    } else if (strcasecmp(end, "E") == 0) {
      mul = pow2(60);
    } else if (*end == 0) {
      mul = 1;
    } else {
      return PLIMIT_ERR_PARSE;
    }
  }
  long double r = v * (long double)mul;
  if (r < 0) {
    return PLIMIT_ERR_PARSE;
  }
  if (r > (long double)LLONG_MAX) {
    return PLIMIT_ERR_PARSE;
  }
  return (long long)r;
}

long long parse_ll(const char *s, const char *name) {
  if (!s) {
    return PLIMIT_ERR_ARG;
  }
  char *end = NULL;
  errno = 0;
  long long v = strtoll(s, &end, 10);
  if (errno || *end) {
    log_msg(LOG_ERROR, "invalid value for %s: '%s'", name, s);
  }
  return v;
}

int run_as_root(void) { return geteuid() == 0; }
