# plimit — cgroup v2 limiter for a running process

`plimit` is a tool that creates or reuses cgroups 
to set CPU, memory, and block I/O limits.

> Requires Linux with the unified cgroup v2 hierarchy mounted 
at `/sys/fs/cgroup`, and to be executed with root priviledges.

## Features
- Create a dedicated cgroup for an existing process
- Apply CPU quota/percent, memory max, and io rules
- Auto-enable controllers in the parent cgroup (cpu, memory, io)
- Move the PID into the new cgroup
- Optional attach-only mode and clean deletion
- Dry-runs and verbose logging

## Quick start

Build from source.
```bash
make
make install
```

Example command to limit CPU, Memory and IO
```bash
sudo plimit --pid 1234 --cpu-percent 50 --mem-max 2G --io-max "8:0 rbps=1048576 wbps=1048576"
```

## Usage

See [docs/USAGE.md](docs/USAGE.md) for full details and examples.
