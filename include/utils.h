#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PLIMIT_VERSION
#define PLIMIT_VERSION "0.0.0"
#endif

typedef struct {
  bool verbose;
  bool dry_run;
  bool force;
} run_opts_t;

void vlogf(bool enabled, const char *fmt, ...);
void die(const char *fmt, ...);
void sysdie(const char *fmt, ...);
void write_stderr(const char *fmt, ...);
void write_stdout(const char *fmt, ...);

int write_file(const char *path, const char *data, bool dry_run, bool verbose);
int append_file(const char *path, const char *data, bool dry_run, bool verbose);

long long parse_bytes(const char *s); // supports K, M, G, T, P, E (binary)
long long parse_ll(const char *s, const char *name);

int ensure_dir(const char *path, mode_t mode, bool dry_run, bool verbose);

int have_cgroupv2(void);
int is_root(void);

#endif
