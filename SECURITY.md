# Security Policy

- This tool requires elevated privileges to write into `/sys/fs/cgroup`.
- Prefer running from a hardened admin context, not as part of an untrusted service.
- Review limits you set for correctness; misleading IO device tuples can affect unrelated workloads.
- Report vulnerabilities via issues with a "[SECURITY]" prefix; don't include sensitive logs publicly.
