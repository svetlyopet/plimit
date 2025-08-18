## Usage

```text
plimit [options]

Options:
  --pid PID                 PID to move into the cgroup (requried unless --delete with --cgname).
  --cgname NAME             Cgroup name (default: "plimit/<PID>").
  --attach-only             Do not change limits, only move PID into an existing cgroup.
  --delete                  Delete the target cgroup (requires --cgname). PID not required.
  --dry-run                 Print actions without making changes.
  --force                   Create parent and enable controllers as needed.
  --verbose                 Extra logging.
  --version                 Show version.
  --help                    Show help.

CPU limit options (cgroup v2: cpu.max):
  --cpu-percent N           Limit CPU to N%% of a single CPU (1-100). Uses 100000µs period.
  --cpu-quota US            Set hard quota (µs) for each period (requires --cpu-period).
  --cpu-period US           Set period (µs) for quota (requires --cpu-quota).
  --cpu-max VALUE           Write VALUE directly to cpu.max (e.g., "max" or "50000 100000").

Memory limit options (cgroup v2: memory.max):
  --mem-max SIZE            Absolute memory limit, accepts suffixes: K, M, G, T, P, E (binary: KiB/MiB etc.).

IO limit options (cgroup v2: io.max):
  --io-max STRING           Direct string for io.max, e.g. "8:0 rbps=1048576 wbps=1048576".
                            Repeat the flag to set multiple devices.
```

## Examples

```bash
# Limit PID 4321 to 1 CPU @ 60% (quota 60000/100000) and 1 GiB RAM
sudo plimit --pid 4321 --cpu-percent 60 --mem-max 1G

# Explicit quota/period and IO rbps cap for /dev/sda (major:minor 8:0)
sudo plimit --pid 4321 --cpu-quota 50000 --cpu-period 100000 --io-max "8:0 rbps=1048576"

# Use direct cpu.max, memory.max and attach only
sudo plimit --pid 4321 --cpu-max "75000 100000" --mem-max 2G --attach-only

# Delete a cgroup (no PID required)
sudo plimit --delete --cgname plimit-g1/app1
```
