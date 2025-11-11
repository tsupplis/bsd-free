# free - BSD Memory Display Utility

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
sudo make install
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
               total        used        free      shared  buff/cache   available
Mem:         8388608     2145678     4567890           0     1675040     6242930
Swap:        2097152           0     2097152
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

- BSD memory management differs from Linux, so exact values may not match conceptually
- The "shared" field is not easily available on most BSDs and is displayed as 0
- The calculation of "available" memory uses BSD's inactive pages, which is similar but not identical to Linux's calculation

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

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
