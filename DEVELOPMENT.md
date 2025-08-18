## Building and Developing `plimit`

This document describes how to build, lint, format, install, and clean the `plimit` project using the provided Makefile.

---

## Prerequisites

- **Compiler:** GCC (or compatible C compiler)
- **Tools:** `make`, `clang-tidy`, `clang-format`

---

## Directory Structure

- `src/` — Source files
- `include/` — Header files
- `lib/` — Third-party libraries
- `bin/` — Compiled binaries
- `build/` — Object files
- `tests/` — Test files

---

## Build

Compile the project and generate the binary:

```sh
make
```

- Uses compiler flags for warnings, pedantic checks, and optimization.
- Output: `bin/plimit`

## Install

Install the binary to `/usr/local/bin` (default):

```sh
sudo make install
```

- Requires root privileges.
- Installs `bin/plimit` to `/usr/local/bin/plimit` (or the path set by `PREFIX`).

## Lint

Run `clang-tidy` linter on all source and header files:

```sh
make lint
```

- Uses `.clang-tidy` configuration.
- Checks `src/` and `include/` directories.

## Format

Run `clang-format` on all source and header files:

```sh
make format
```

- Uses project `.clang-format` style.
- Formats files in `src/` and `include/`.

## Clean

Remove build artifacts and binaries:

```sh
make clean
```

- Deletes `build/` and `bin/` directories.

## Notes

- The build version is determined by `git describe` if available, otherwise defaults to `0.0.0`.
- All compiler and linker flags are set in the Makefile for strict warning and error checking.
- The argtable3 library is built from source in `lib/argtable`.

---

## Customization

- Change installation prefix by setting `PREFIX`:
  ```sh
  make install PREFIX=/usr/bin
  ```
- Specify a build version by setting `PLIMIT_VERSION`:
  ```sh
  make PLIMIT_VERSION=1.0.0
  ```
- Adjust compiler flags in the Makefile as needed.
