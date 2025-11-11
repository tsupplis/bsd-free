# Free - BSD Memory Display Utility

A loosely compatible cross-platform BSD implementation of the Linux memory `free` command that displays memory and swap usage.

Supports: FreeBSD, NetBSD, OpenBSD, DragonFly BSD, and macOS (Darwin).

## Reason for this development

1. Missing terribly this little command
2. Play with Claude Sonnet 4.5 and refine iterative process with AI

## Building

```sh
make
```

## Installation

```sh
doas make install
```

## Usage

```
Usage: free [options]
Display amount of free and used memory in the system

Options:
  -b, --bytes    Display the amount of memory in bytes
  -k, --kilo     Display the amount of memory in kilobytes (default)
  -m, --mega     Display the amount of memory in megabytes
  -g, --giga     Display the amount of memory in gigabytes
  -h, --human    Show human-readable output
  -V, --version  Show version information
      --help     Print this help
```

## Example Output

```
               total         used         free   buff/cache    available
Mem:           16384        12957           78         2294         3426
Swap:           6144         5257          886
```

## How It Works

The program uses FreeBSD's `sysctl` interface to gather memory statistics:

- **total**: Total physical memory (`vm.stats.vm.v_page_count`)
- **used**: Calculated as `total - available`
- **free**: Free memory (`vm.stats.vm.v_free_count`)
- **shared**: Memory used by tmpfs (not easily available on FreeBSD, shown as 0)
- **buff/cache**: Buffer and cache memory (`vfs.bufspace` + `vm.stats.vm.v_cache_count`)
- **available**: Estimate of memory available for new applications (free + inactive + cache)
- **swap**: Swap space information from `vm.swap_info`

## Platform-Specific Details

### FreeBSD
- Uses `vm.stats.vm.v_*` individual sysctls for page counts
- Swap info from `vm.swap_info` array
- Buffer memory from `vfs.bufspace`
- Available = free + inactive + cache

### NetBSD
- Uses `struct uvmexp_sysctl` via `VM_UVMEXP2`
- Has separate execpages/filepages for executable and file cache
- Buffer memory from `vm.bufmem`
- Available = free + inactive + cache

### OpenBSD
- Uses `struct uvmexp` via `VM_UVMEXP`
- Total memory from `hw.physmem64`
- Has vnodepages/vtextpages for vnode and vtext cache
- Buffer memory not easily accessible (shown as 0)

### DragonFly BSD
- Uses `vm.stats.vm.v_*` individual sysctls (like FreeBSD)
- Simplified swap info via `vm.swap_size` and `vm.swap_free`
- Has `v_cache_count` for cached pages
- Buffer memory not easily accessible (shown as 0)

### macOS (Darwin)
- Uses Mach `host_statistics64()` API for VM statistics
- Total memory from `hw.memsize`, swap from `vm.swapusage`
- Page size: 16KB on Apple Silicon (M1/M2/M3), 4KB on Intel
- Has memory compression (compressed pages counted as used)
- File-backed pages (external_page_count) shown as buffers
- Cache = speculative + purgeable pages
- Aggressively uses memory for caching/compression

## Differences from Linux

This is a simplified implementation compared to Linux's `free` command:

### Not Implemented
- **shared** column - Always shown as 0 (shared memory not easily accessible on most BSDs)
- **-w, --wide** - Wide mode (separate buffers and cache columns)
- **-t, --total** - Display total line
- **-s, --seconds** - Continuous updates
- **-c, --count** - Number of updates
- **-l, --lohi** - Show detailed low/high memory stats
- **--si** - Use powers of 1000 instead of 1024
- **--committed** - Show committed memory

### Behavioral Differences
- **Memory calculations** - BSD and Linux use different VM subsystems:
  - Linux reads from `/proc/meminfo`
  - BSDs use `sysctl` or Mach APIs
  - "Available" memory calculation differs due to different page management
- **Output format** - Matches Linux format but values reflect BSD memory management
- **Shared memory** - Not tracked the same way across BSDs

## Development

This project was developed with assistance from GitHub Copilot (Claude 3.5 Sonnet)

### Sources and References

The implementation was based on studying and referencing the following sources:

1. **BSD Manual Pages**:
   - FreeBSD: `sysctl(3)`, `sysctl(8)`, `vmstat(8)`
   - NetBSD: `sysctl(3)`, `sysctl(7)`, UVM documentation
   - OpenBSD: `sysctl(2)`, `sysctl(3)`, `vmstat(8)`
   - DragonFly BSD: `sysctl(3)`, `sysctl(8)`, `vmstat(8)`

2. **BSD System Headers**:
   - FreeBSD: `/usr/include/vm/vm_param.h`
   - NetBSD: `/usr/include/uvm/uvm_extern.h`
   - OpenBSD: `/usr/include/uvm/uvmexp.h`
   - DragonFly BSD: `/usr/include/sys/vmmeter.h`

3. **Testing and Verification**:
   - Compared output against native `vmstat -s` on all platforms
   - Verified against existing `free` implementations where available
   - Cross-referenced with `sysctl` direct queries

4. **Platform Documentation**:
   - FreeBSD Handbook and Developer's Handbook
   - NetBSD Guide and internals documentation
   - OpenBSD FAQ and man pages
   - DragonFly BSD documentation

## License

BSD 2-Clause License
