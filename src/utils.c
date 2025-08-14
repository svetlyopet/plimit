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

void vlogf(bool enabled, const char *fmt, ...) {
  if (!enabled) {
    return;
  }
  va_list ap;
  va_start(ap, fmt);
  write_stdout(fmt, ap);
  write_stdout("\n");
  va_end(ap);
}

void die(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  write_stderr(fmt, ap);
  write_stderr("\n");
  va_end(ap);
  exit(EXIT_FAILURE);
}

void sysdie(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  write_stderr(fmt, ap);
  write_stderr(": %s\n", strerror(errno));
  va_end(ap);
  exit(EXIT_FAILURE);
}

int write_file(const char *path, const char *data, bool dry_run, bool verbose) {
  if (dry_run) {
    vlogf(verbose, "[dry-run] echo '%s' > %s", data, path);
    return 0;
  }
  int fd = open(path, O_WRONLY | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }
  size_t len = strlen(data);
  ssize_t n = write(fd, data, len);
  close(fd);
  if (n < 0 || (size_t)n != len) {
    return -1;
  }
  return 0;
}

int append_file(const char *path, const char *data, bool dry_run,
                bool verbose) {
  if (dry_run) {
    vlogf(verbose, "[dry-run] echo '%s' >> %s", data, path);
    return 0;
  }
  int fd = open(path, O_WRONLY | O_APPEND | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }
  size_t len = strlen(data);
  ssize_t n = write(fd, data, len);
  close(fd);
  if (n < 0 || (size_t)n != len) {
    return -1;
  }
  return 0;
}

long long parse_bytes(const char *s) {
  if (!s || !*s) {
    return -1;
  }
  char *end = NULL;
  errno = 0;
  long double v = strtold(s, &end);
  if (errno != 0) {
    return -1;
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
      return -1;
    }
  }
  long double r = v * (long double)mul;
  if (r < 0) {
    return -1;
  }
  if (r > (long double)LLONG_MAX) {
    return -1;
  }
  return (long long)r;
}

long long parse_ll(const char *s, const char *name) {
  if (!s) {
    return -1;
  }
  char *end = NULL;
  errno = 0;
  long long v = strtoll(s, &end, 10);
  if (errno || *end) {
    die("invalid %s: %s", name, s);
  }
  return v;
}

int ensure_dir(const char *path, mode_t mode, bool dry_run, bool verbose) {
  if (dry_run) {
    vlogf(verbose, "[dry-run] mkdir -p %s", path);
    return 0;
  }
  struct stat st;
  if (stat(path, &st) == 0) {
    if (!S_ISDIR(st.st_mode)) {
      errno = ENOTDIR;
      return -1;
    }
    return 0;
  }
  if (mkdir(path, mode) == 0) {
    return 0;
  }
  return -1;
}

int have_cgroupv2(void) {
  struct stat st;
  return stat("/sys/fs/cgroup/cgroup.controllers", &st) == 0;
}

int is_root(void) { return geteuid() == 0; }

void write_stderr(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  fputs(buf, stderr);
}

void write_stdout(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  fputs(buf, stdout);
}
