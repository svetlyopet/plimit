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

static void log_f(FILE *stream, const char *fmt, va_list ap) {
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  fputs(buf, stream);
  fputc('\n', stream);
}

void slogf(log_dest_t dest, const char *fmt, ...) {
  FILE *stream = (dest == LOG_STDOUT) ? stdout : stderr;
  va_list ap;
  va_start(ap, fmt);
  log_f(stream, fmt, ap);
  va_end(ap);
}

void vlogf(bool enabled, const char *fmt, ...) {
  if (!enabled) {
    return;
  }
  FILE *stream = stdout;
  va_list ap;
  va_start(ap, fmt);
  log_f(stream, fmt, ap);
  va_end(ap);
}

void fatal_errorf(const char *fmt, ...) {
  slogf(LOG_STDERR, "Fatal error: ");
  slogf(LOG_STDERR, fmt);
  exit(EXIT_FAILURE);
}

void fatal_sys_errorf(const char *fmt, ...) {
  slogf(LOG_STDERR, "Fatal system error: ");
  slogf(LOG_STDERR, "%s: %s\n", fmt, strerror(errno));
  exit(EXIT_FAILURE);
}

int create_file(const char *path, mode_t mode, bool dry_run, bool verbose) {
  if (dry_run) {
    slogf(LOG_STDOUT, "[dry-run] ACTION: create file | PATH: '%s' | MODE: %o",
          path, mode);
    return PLIMIT_OK;
  }
  if (!path) {
    return PLIMIT_ERR_ARG;
  }
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
  if (fd < 0) {
    return PLIMIT_ERR_IO;
  }
  close(fd);
  vlogf(verbose, "[info] ACTION: create file | PATH: '%s' | MODE: %o", path,
        mode);
  return PLIMIT_OK;
}

int write_file(const char *path, const char *data, bool dry_run, bool verbose) {
  if (dry_run) {
    slogf(LOG_STDOUT, "[dry-run] ACTION: write file | PATH: '%s' | DATA: '%s'",
          path, data);
    return PLIMIT_OK;
  }
  int fd = open(path, O_WRONLY | O_CLOEXEC);
  if (fd < 0) {
    return PLIMIT_ERR_IO;
  }
  size_t len = strlen(data);
  ssize_t n = write(fd, data, len);
  close(fd);
  if (n < 0 || (size_t)n != len) {
    return PLIMIT_ERR_IO;
  }
  vlogf(verbose, "[info] ACTION: write file | PATH: '%s' | DATA: '%s'", path,
        data);
  return PLIMIT_OK;
}

int append_file(const char *path, const char *data, bool dry_run,
                bool verbose) {
  if (dry_run) {
    slogf(LOG_STDOUT, "[dry-run] ACTION: append file | PATH: '%s' | DATA: '%s'",
          path, data);
    return PLIMIT_OK;
  }
  int fd = open(path, O_WRONLY | O_APPEND | O_CLOEXEC);
  if (fd < 0) {
    return PLIMIT_ERR_IO;
  }
  size_t len = strlen(data);
  ssize_t n = write(fd, data, len);
  close(fd);
  if (n < 0 || (size_t)n != len) {
    return PLIMIT_ERR_IO;
  }
  vlogf(verbose, "[info] ACTION: append file | PATH: '%s' | DATA: '%s'", path,
        data);
  return PLIMIT_OK;
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
    fatal_errorf("parse_ll: invalid value for %s: '%s'", name, s);
  }
  return v;
}

int ensure_dir(const char *path, mode_t mode, bool dry_run, bool verbose) {
  if (dry_run) {
    slogf(LOG_STDOUT,
          "[dry-run] ACTION: ensure directory | PATH: '%s' | MODE: %o", path,
          mode);
    return PLIMIT_OK;
  }
  struct stat st;
  if (stat(path, &st) == 0) {
    if (!S_ISDIR(st.st_mode)) {
      errno = ENOTDIR;
      return PLIMIT_ERR_IO;
    }
    return PLIMIT_OK;
  }
  if (mkdir(path, mode) == 0) {
    vlogf(verbose, "[info] ACTION: ensure directory | PATH: '%s' | MODE: %o",
          path, mode);
    return PLIMIT_OK;
  }
  return PLIMIT_ERR_IO;
}

int is_root(void) { return geteuid() == 0; }
