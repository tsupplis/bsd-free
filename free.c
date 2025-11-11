/*
 * free - Cross-Platform Memory Display Utility
 * 
 * A cross-platform implementation of the Linux free command.
 * Supports: FreeBSD, NetBSD, OpenBSD, DragonFly BSD, macOS,
 *           illumos/Solaris, and Haiku OS.
 * 
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

/* Haiku doesn't have err.h */
#ifdef __HAIKU__
#include <errno.h>
#define err(code, fmt, ...) do { \
    fprintf(stderr, fmt ": %s\n", ##__VA_ARGS__, strerror(errno)); \
    exit(code); \
} while(0)
#define errx(code, fmt, ...) do { \
    fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
    exit(code); \
} while(0)
#define warnx(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else
#include <err.h>
#endif

/* sysctl not available on illumos/Solaris or Haiku */
#if !defined(__sun) && !defined(__illumos__) && !defined(__HAIKU__)
#include <sys/sysctl.h>
#endif

#ifdef __FreeBSD__
#include <vm/vm_param.h>
#endif

#ifdef __NetBSD__
#include <uvm/uvm_extern.h>
#endif

#ifdef __OpenBSD__
#include <uvm/uvmexp.h>
#endif

#ifdef __DragonFly__
#include <sys/vmmeter.h>
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <mach/mach_host.h>
#endif

#if defined(__sun) || defined(__illumos__)
#include <kstat.h>
#include <sys/stat.h>
#include <sys/swap.h>
#include <fcntl.h>
#endif

#ifdef __HAIKU__
#include <OS.h>
#endif

#define VERSION "1.0.4"

#define KILOBYTE 1024
#define MEGABYTE (1024 * 1024)
#define GIGABYTE (1024 * 1024 * 1024)
#define TERABYTE ((unsigned long long)1024 * 1024 * 1024 * 1024)
#define PETABYTE ((unsigned long long)1024 * 1024 * 1024 * 1024 * 1024)

typedef enum {
    UNIT_BYTES,
    UNIT_KILO,
    UNIT_MEGA,
    UNIT_GIGA,
    UNIT_HUMAN
} unit_t;

typedef struct {
    uint64_t mem_total;
    uint64_t mem_free;
    uint64_t mem_active;
    uint64_t mem_inactive;
    uint64_t mem_wired;
    uint64_t mem_cache;
    uint64_t mem_buffers;
    uint64_t swap_total;
    uint64_t swap_used;
    int has_swap_info;  /* 1 if swap info available, 0 otherwise */
} mem_stats_t;

void print_version(void) {
    printf("free version %s\n", VERSION);
}

void print_help(void) {
    printf("Usage: free [options]\n");
    printf("Display amount of free and used memory in the system\n\n");
    printf("Options:\n");
    printf("  -b, --bytes    Display the amount of memory in bytes\n");
    printf("  -k, --kilo     Display the amount of memory in kilobytes (default)\n");
    printf("  -m, --mega     Display the amount of memory in megabytes\n");
    printf("  -g, --giga     Display the amount of memory in gigabytes\n");
    printf("  -h, --human    Show human-readable output\n");
    printf("  -V, --version  Show version information\n");
    printf("      --help     Print this help\n");
}

void format_value(uint64_t value, unit_t unit, char *buf, size_t bufsize) {
    switch (unit) {
        case UNIT_BYTES:
            snprintf(buf, bufsize, "%llu", (unsigned long long)value);
            break;
        case UNIT_KILO:
            snprintf(buf, bufsize, "%llu", (unsigned long long)(value / KILOBYTE));
            break;
        case UNIT_MEGA:
            snprintf(buf, bufsize, "%llu", (unsigned long long)(value / MEGABYTE));
            break;
        case UNIT_GIGA:
            snprintf(buf, bufsize, "%llu", (unsigned long long)(value / GIGABYTE));
            break;
        case UNIT_HUMAN:
            if (value >= PETABYTE) {
                snprintf(buf, bufsize, "%.1fP", (double)value / PETABYTE);
            } else if (value >= TERABYTE) {
                snprintf(buf, bufsize, "%.1fT", (double)value / TERABYTE);
            } else if (value >= GIGABYTE) {
                snprintf(buf, bufsize, "%.1fG", (double)value / GIGABYTE);
            } else if (value >= MEGABYTE) {
                snprintf(buf, bufsize, "%.1fM", (double)value / MEGABYTE);
            } else if (value >= KILOBYTE) {
                snprintf(buf, bufsize, "%.1fK", (double)value / KILOBYTE);
            } else {
                snprintf(buf, bufsize, "%lluB", (unsigned long long)value);
            }
            break;
    }
}

#ifdef __FreeBSD__
/*
 * FreeBSD Memory Statistics Retrieval
 * 
 * FreeBSD uses the vm.stats.vm.v_* sysctl hierarchy to expose virtual memory
 * statistics. Each statistic is retrieved separately by name using sysctlbyname().
 * 
 * Key differences from NetBSD/OpenBSD:
 * - No unified uvmexp structure; each stat is a separate sysctl
 * - Uses xswdev structure for swap device information
 * - Supports v_cache_count (file cache) on some versions
 * - Buffer memory available via vfs.bufspace
 * - Total memory calculated from managed pages (v_page_count)
 */
int retrieve_mem_stats(mem_stats_t *stats) {
    unsigned int page_size;
    unsigned int page_count;
    size_t len;
    
    /* Get page size */
    len = sizeof(page_size);
    if (sysctlbyname("vm.stats.vm.v_page_size", &page_size, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_page_size");
    }
    
    /* Get memory statistics */
    len = sizeof(page_count);
    if (sysctlbyname("vm.stats.vm.v_page_count", &page_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_page_count");
    }
    stats->mem_total = (uint64_t)page_count * page_size;
    
    len = sizeof(page_count);
    if (sysctlbyname("vm.stats.vm.v_free_count", &page_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_free_count");
    }
    stats->mem_free = (uint64_t)page_count * page_size;
    
    len = sizeof(page_count);
    if (sysctlbyname("vm.stats.vm.v_active_count", &page_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_active_count");
    }
    stats->mem_active = (uint64_t)page_count * page_size;
    
    len = sizeof(page_count);
    if (sysctlbyname("vm.stats.vm.v_inactive_count", &page_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_inactive_count");
    }
    stats->mem_inactive = (uint64_t)page_count * page_size;
    
    len = sizeof(page_count);
    if (sysctlbyname("vm.stats.vm.v_wire_count", &page_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_wire_count");
    }
    stats->mem_wired = (uint64_t)page_count * page_size;
    
    /*
     * Try to get ZFS ARC cache size first
     * On FreeBSD systems with ZFS, the ARC is the primary cache
     * and can use significant memory (often gigabytes)
     * If ZFS is not available, fall back to v_cache_count
     */
    uint64_t arc_size = 0;
    len = sizeof(arc_size);
    if (sysctlbyname("kstat.zfs.misc.arcstats.size", &arc_size, &len, NULL, 0) == 0 && arc_size > 0) {
        /* ZFS ARC is available, use it as the primary cache metric */
        stats->mem_cache = arc_size;
        /* On ZFS systems, bufspace is typically small and included in ARC */
        stats->mem_buffers = 0;
    } else {
        /* No ZFS, use traditional cache count */
        len = sizeof(page_count);
        if (sysctlbyname("vm.stats.vm.v_cache_count", &page_count, &len, NULL, 0) == -1) {
            stats->mem_cache = 0;
        } else {
            stats->mem_cache = (uint64_t)page_count * page_size;
        }
        
        /* Get buffer memory */
        len = sizeof(page_count);
        if (sysctlbyname("vfs.bufspace", &page_count, &len, NULL, 0) == -1) {
            stats->mem_buffers = 0;
        } else {
            stats->mem_buffers = page_count;
        }
    }
    
    /*
     * Get swap information
     * FreeBSD can have multiple swap devices, so we iterate through
     * vm.swap_info using xswdev structure to sum up all swap space.
     */
    struct xswdev xsw;
    size_t miblen = 16;
    int mib[16];
    
    miblen = sizeof(mib) / sizeof(mib[0]);
    if (sysctlnametomib("vm.swap_info", mib, &miblen) == -1) {
        stats->swap_total = 0;
        stats->swap_used = 0;
    } else {
        stats->swap_total = 0;
        stats->swap_used = 0;
        
        for (int i = 0; ; i++) {
            mib[miblen] = i;
            len = sizeof(xsw);
            if (sysctl(mib, miblen + 1, &xsw, &len, NULL, 0) == -1) {
                break;
            }
            stats->swap_total += (uint64_t)xsw.xsw_nblks * page_size;
            stats->swap_used += (uint64_t)xsw.xsw_used * page_size;
        }
    }
    
    stats->has_swap_info = 1;
    return 0;
}
#endif

#ifdef __NetBSD__
/*
 * NetBSD Memory Statistics Retrieval
 * 
 * NetBSD uses UVM (Unified Virtual Memory) and provides memory statistics
 * through the uvmexp_sysctl structure accessed via VM_UVMEXP2 sysctl.
 * 
 * Key differences from FreeBSD/OpenBSD:
 * - Uses uvmexp_sysctl (VM_UVMEXP2) not uvmexp (VM_UVMEXP)
 * - uvmexp_sysctl has int64_t fields vs. int in regular uvmexp
 * - Includes separate execpages and filepages for cache calculation
 * - Has vm.bufmem sysctl for buffer memory
 * - Total memory from npages (managed pages), not hw.physmem
 * - This matches NetBSD's own /usr/pkg/bin/free behavior
 * 
 * Note: VM_UVMEXP provides struct uvmexp which lacks active/inactive fields,
 * while VM_UVMEXP2 provides struct uvmexp_sysctl with all needed counters.
 */
int retrieve_mem_stats(mem_stats_t *stats) {
    struct uvmexp_sysctl uvmexp;
    size_t len;
    int mib[2];
    uint64_t page_size;
    uint64_t bufmem;
    
    /* Get UVM statistics using uvmexp_sysctl structure */
    mib[0] = CTL_VM;
    mib[1] = VM_UVMEXP2;
    len = sizeof(uvmexp);
    if (sysctl(mib, 2, &uvmexp, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.uvmexp2");
    }
    
    /* uvmexp_sysctl has pagesize as int64_t */
    page_size = (uint64_t)uvmexp.pagesize;
    
    /*
     * Use pages managed (npages) for total, not hw.physmem
     * This matches NetBSD's /usr/pkg/bin/free which parses vmstat output
     * Pages managed = total pages the kernel manages (excludes kernel reserved)
     */
    stats->mem_total = (uint64_t)uvmexp.npages * page_size;
    
    /*
     * NetBSD UVM page categories:
     * - free: immediately available pages
     * - active: recently accessed, likely to be used again
     * - inactive: not recently used, candidates for reclamation
     * - wired: locked in memory, cannot be paged out
     * - execpages: executable code pages (cached)
     * - filepages: file data pages (cached)
     */
    stats->mem_free = (uint64_t)uvmexp.free * page_size;
    stats->mem_active = (uint64_t)uvmexp.active * page_size;
    stats->mem_inactive = (uint64_t)uvmexp.inactive * page_size;
    stats->mem_wired = (uint64_t)uvmexp.wired * page_size;
    
    /* File cache = executable pages + file data pages */
    stats->mem_cache = (uint64_t)(uvmexp.execpages + uvmexp.filepages) * page_size;
    
    /* Get buffer memory (file system buffers) */
    len = sizeof(bufmem);
    if (sysctlbyname("vm.bufmem", &bufmem, &len, NULL, 0) == -1) {
        stats->mem_buffers = 0;
    } else {
        stats->mem_buffers = (uint64_t)bufmem;
    }
    
    /* Swap statistics directly available in uvmexp */
    stats->swap_total = (uint64_t)uvmexp.swpages * page_size;
    stats->swap_used = (uint64_t)uvmexp.swpginuse * page_size;
    
    stats->has_swap_info = 1;
    return 0;
}
#endif

#ifdef __OpenBSD__
/*
 * OpenBSD Memory Statistics Retrieval
 * 
 * OpenBSD uses UVM like NetBSD but with some key differences in how
 * memory statistics are exposed and calculated.
 * 
 * Key differences from FreeBSD/NetBSD:
 * - Uses hw.physmem64 for total memory (actual physical RAM)
 * - Uses struct uvmexp (VM_UVMEXP), not uvmexp_sysctl
 * - uvmexp has int fields (not int64_t like NetBSD's uvmexp_sysctl)
 * - Has vnodepages/vtextpages for vnode and vtext cache
 * - No easily accessible buffer memory sysctl (unlike NetBSD's vm.bufmem)
 * - Swap information directly in uvmexp structure
 * 
 * Memory calculation approach:
 * - Total = hw.physmem64 (matches /usr/local/bin/free behavior)
 *   This differs from using npages which would give "managed" pages
 * - Used = total - free (simple calculation)
 * - Available = free + inactive + cache (reclaimable memory)
 * 
 * Note: vmstat shows "pages managed" which is less than hw.physmem
 * because some memory is reserved for kernel use at boot.
 */
int retrieve_mem_stats(mem_stats_t *stats) {
    struct uvmexp uvmexp;
    size_t len;
    int mib[2];
    uint64_t page_size;
    uint64_t physmem;
    
    /*
     * Get physical memory from hw.physmem64
     * This is the actual installed RAM and matches /usr/local/bin/free
     * Alternative would be to use uvmexp.npages * pagesize for "managed" pages
     */
    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM64;
    len = sizeof(physmem);
    if (sysctl(mib, 2, &physmem, &len, NULL, 0) == -1) {
        err(1, "sysctl hw.physmem64");
    }
    stats->mem_total = physmem;
    
    /* Get UVM statistics via VM_UVMEXP (struct uvmexp) */
    mib[0] = CTL_VM;
    mib[1] = VM_UVMEXP;
    len = sizeof(uvmexp);
    if (sysctl(mib, 2, &uvmexp, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.uvmexp");
    }
    
    /* OpenBSD's uvmexp has pagesize as int (not int64_t) */
    page_size = (uint64_t)uvmexp.pagesize;
    
    /*
     * OpenBSD UVM page categories (similar to NetBSD):
     * - free: immediately available pages
     * - active: recently accessed, likely to be used again
     * - inactive: not recently used, candidates for reclamation
     * - wired: locked in memory, cannot be paged out
     * - cache: buffer cache and other cached pages
     */
    stats->mem_free = (uint64_t)uvmexp.free * page_size;
    stats->mem_active = (uint64_t)uvmexp.active * page_size;
    stats->mem_inactive = (uint64_t)uvmexp.inactive * page_size;
    stats->mem_wired = (uint64_t)uvmexp.wired * page_size;
    
    /*
     * Calculate cache as remaining pages not accounted for
     * OpenBSD's top uses: npages - free - active - inactive - wired
     * This includes buffer cache, per-CPU caches, and other cached pages
     * Note: vnodepages and vtextpages fields exist but are often 0
     */
    int64_t cache_pages = uvmexp.npages - uvmexp.free - uvmexp.active - 
                          uvmexp.inactive - uvmexp.wired;
    if (cache_pages < 0) cache_pages = 0;
    stats->mem_cache = (uint64_t)cache_pages * page_size;
    
    /*
     * Buffer memory not separately tracked on OpenBSD
     * It's included in the cache calculation above
     */
    stats->mem_buffers = 0;
    
    /* Swap statistics directly available in uvmexp */
    stats->swap_total = (uint64_t)uvmexp.swpages * page_size;
    stats->swap_used = (uint64_t)uvmexp.swpginuse * page_size;
    
    stats->has_swap_info = 1;
    return 0;
}
#endif

#ifdef __DragonFly__
/*
 * DragonFly BSD Memory Statistics Retrieval
 * 
 * DragonFly BSD forked from FreeBSD 4.x but has evolved its own
 * virtual memory system with significant differences from FreeBSD.
 * 
 * Key differences from other BSDs:
 * - Uses individual vm.stats.vm.v_* sysctls (like FreeBSD) NOT struct vmmeter
 * - Has v_cache_count for cached pages (similar to FreeBSD's v_cache_count)
 * - Swap information via vm.swap_size and vm.swap_free (simpler than FreeBSD)
 * - Physical memory from hw.physmem (returns unsigned long)
 * - Page counts are in pages, need to multiply by pagesize
 * 
 * Memory calculation approach:
 * - Total = hw.physmem (actual physical RAM)
 * - Used = total - free (simple calculation like NetBSD/OpenBSD)
 * - Available = free + inactive + cache (reclaimable memory)
 * 
 * Note: DragonFly's VM system includes some FreeBSD heritage but with
 * its own DFLY VM improvements for multi-threading and NUMA support.
 * The sysctl names are similar to FreeBSD but swap handling is simplified.
 */
int retrieve_mem_stats(mem_stats_t *stats) {
    size_t len;
    unsigned long physmem;
    u_int page_size;
    u_int v_free_count, v_active_count, v_inactive_count;
    u_int v_wire_count, v_cache_count;
    u_int swap_size, swap_free;
    
    /*
     * Get physical memory from hw.physmem
     * Returns unsigned long on DragonFly (actual installed RAM)
     */
    len = sizeof(physmem);
    if (sysctlbyname("hw.physmem", &physmem, &len, NULL, 0) == -1) {
        err(1, "sysctl hw.physmem");
    }
    stats->mem_total = (uint64_t)physmem;
    
    /* Get page size (returns u_int on DragonFly) */
    len = sizeof(page_size);
    if (sysctlbyname("hw.pagesize", &page_size, &len, NULL, 0) == -1) {
        err(1, "sysctl hw.pagesize");
    }
    
    /*
     * Get VM page counts from individual sysctls
     * DragonFly uses vm.stats.vm.v_* like FreeBSD (not struct vmmeter)
     * 
     * Page categories in DragonFly:
     * - v_free_count: immediately available pages
     * - v_active_count: recently accessed, hot pages
     * - v_inactive_count: not recently used, can be reclaimed
     * - v_wire_count: wired (locked) in memory, cannot be paged
     * - v_cache_count: cached pages (quickly reclaimable)
     */
    len = sizeof(v_free_count);
    if (sysctlbyname("vm.stats.vm.v_free_count", &v_free_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_free_count");
    }
    
    len = sizeof(v_active_count);
    if (sysctlbyname("vm.stats.vm.v_active_count", &v_active_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_active_count");
    }
    
    len = sizeof(v_inactive_count);
    if (sysctlbyname("vm.stats.vm.v_inactive_count", &v_inactive_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_inactive_count");
    }
    
    len = sizeof(v_wire_count);
    if (sysctlbyname("vm.stats.vm.v_wire_count", &v_wire_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_wire_count");
    }
    
    len = sizeof(v_cache_count);
    if (sysctlbyname("vm.stats.vm.v_cache_count", &v_cache_count, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.stats.vm.v_cache_count");
    }
    
    /* Convert page counts to bytes (cast to uint64_t to avoid overflow) */
    stats->mem_free = (uint64_t)v_free_count * page_size;
    stats->mem_active = (uint64_t)v_active_count * page_size;
    stats->mem_inactive = (uint64_t)v_inactive_count * page_size;
    stats->mem_wired = (uint64_t)v_wire_count * page_size;
    stats->mem_cache = (uint64_t)v_cache_count * page_size;
    
    /* Buffer memory not directly accessible on DragonFly */
    stats->mem_buffers = 0;
    
    /*
     * Get swap information from vm.swap_size and vm.swap_free
     * DragonFly provides simpler swap sysctls than FreeBSD's vm.swap_info
     * Both values are in pages and need to be converted to bytes
     */
    len = sizeof(swap_size);
    if (sysctlbyname("vm.swap_size", &swap_size, &len, NULL, 0) == -1) {
        /* Swap might not be configured */
        stats->swap_total = 0;
        stats->swap_used = 0;
        return 0;
    }
    
    len = sizeof(swap_free);
    if (sysctlbyname("vm.swap_free", &swap_free, &len, NULL, 0) == -1) {
        err(1, "sysctl vm.swap_free");
    }
    
    stats->swap_total = (uint64_t)swap_size * page_size;
    stats->swap_used = (uint64_t)(swap_size - swap_free) * page_size;
    
    stats->has_swap_info = 1;
    return 0;
}
#endif

#ifdef __APPLE__
/*
 * macOS (Darwin) Memory Statistics Retrieval
 * 
 * macOS is based on Darwin which combines the Mach microkernel with
 * FreeBSD userland components. Memory statistics come from different
 * sources than traditional BSD systems.
 * 
 * Key differences from other BSDs:
 * - Uses Mach host_statistics64() API for VM statistics
 * - Uses sysctl for total memory (hw.memsize) and swap (vm.swapusage)
 * - Page size typically 16KB on Apple Silicon, 4KB on Intel
 * - Has memory compression (compressed pages don't count as used swap)
 * - Has file-backed pages (app memory) and anonymous pages (data)
 * - Speculative pages are like cache (pre-fetched for performance)
 * 
 * Memory calculation approach:
 * - Total = hw.memsize (actual physical RAM)
 * - Free = free_count (immediately available)
 * - Active = active_count (recently accessed)
 * - Inactive = inactive_count (not recently used)
 * - Wired = wire_count (locked in memory, kernel use)
 * - Cache = speculative_count + purgeable_count (reclaimable)
 * - Compressed pages are counted as "used" but don't consume swap
 * 
 * Note: macOS aggressively uses memory for caching and compression,
 * so "used" memory doesn't mean unavailable memory.
 */
int retrieve_mem_stats(mem_stats_t *stats) {
    size_t len;
    uint64_t memsize;
    uint64_t pagesize;
    mach_msg_type_number_t count;
    vm_statistics64_data_t vm_stats;
    kern_return_t kr;
    struct xsw_usage swapusage;
    
    /*
     * Get physical memory from hw.memsize
     * Returns uint64_t on macOS (actual installed RAM)
     */
    len = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &len, NULL, 0) == -1) {
        err(1, "sysctl hw.memsize");
    }
    stats->mem_total = memsize;
    
    /*
     * Get page size
     * Typically 16KB on Apple Silicon (M1/M2/M3), 4KB on Intel
     */
    len = sizeof(pagesize);
    if (sysctlbyname("hw.pagesize", &pagesize, &len, NULL, 0) == -1) {
        err(1, "sysctl hw.pagesize");
    }
    
    /*
     * Get VM statistics using Mach host_statistics64() API
     * This is the Darwin/Mach way of getting memory statistics
     * Unlike BSD sysctls, this uses Mach IPC
     */
    count = HOST_VM_INFO64_COUNT;
    kr = host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          (host_info64_t)&vm_stats, &count);
    if (kr != KERN_SUCCESS) {
        errx(1, "host_statistics64 failed: %s", mach_error_string(kr));
    }
    
    /*
     * macOS page categories (from vm_statistics64):
     * - free_count: immediately available pages
     * - active_count: pages currently in use or recently used
     * - inactive_count: pages not recently used, candidates for reclamation
     * - wire_count: wired (locked) pages, cannot be paged out (kernel use)
     * - speculative_count: pre-fetched pages (like cache)
     * - purgeable_count: purgeable memory (can be freed instantly)
     * - compressor_page_count: pages held by memory compressor
     * - external_page_count: file-backed pages (app code/mapped files)
     * - internal_page_count: anonymous pages (app data/heap/stack)
     */
    stats->mem_free = (uint64_t)vm_stats.free_count * pagesize;
    stats->mem_active = (uint64_t)vm_stats.active_count * pagesize;
    stats->mem_inactive = (uint64_t)vm_stats.inactive_count * pagesize;
    stats->mem_wired = (uint64_t)vm_stats.wire_count * pagesize;
    
    /*
     * Cache = speculative + purgeable pages
     * These are reclaimable without needing to page out to disk
     */
    stats->mem_cache = (uint64_t)(vm_stats.speculative_count + 
                                   vm_stats.purgeable_count) * pagesize;
    
    /*
     * File-backed pages (external) could be considered as "buffers"
     * These are pages backed by files on disk (app code, mapped files)
     */
    stats->mem_buffers = (uint64_t)vm_stats.external_page_count * pagesize;
    
    /*
     * Get swap usage from vm.swapusage sysctl
     * macOS provides a structured xsw_usage with total/used/free in bytes
     * Note: Compressed memory doesn't necessarily use swap space
     */
    len = sizeof(swapusage);
    if (sysctlbyname("vm.swapusage", &swapusage, &len, NULL, 0) == -1) {
        /* Swap might not be configured */
        stats->swap_total = 0;
        stats->swap_used = 0;
        stats->has_swap_info = 0;
        return 0;
    }
    
    stats->swap_total = swapusage.xsu_total;
    stats->swap_used = swapusage.xsu_used;
    stats->has_swap_info = 1;
    
    return 0;
}
#endif

#if defined(__sun) || defined(__illumos__)
/*
 * illumos/Solaris Memory Statistics Retrieval
 * 
 * illumos is an open-source fork of OpenSolaris, and Solaris is the
 * original Sun/Oracle operating system. Both share similar APIs.
 * 
 * Key differences from BSD systems:
 * - Uses kstat (kernel statistics) library for memory info
 * - Memory statistics from unix:0:system_pages kstat module
 * - Swap information from swapctl() system call
 * - Page size from sysconf(_SC_PAGESIZE)
 * - No sysctl interface (different from BSD)
 * 
 * Memory calculation approach:
 * - Total = physmem (total physical pages)
 * - Free = freemem (immediately available pages)
 * - Uses ZFS ARC as cache on illumos/Solaris
 * - Kernel memory is "locked" (similar to wired on BSD)
 * 
 * Note: illumos/Solaris have sophisticated memory management with
 * ZFS ARC (Adaptive Replacement Cache) which can use significant RAM.
 */
int retrieve_mem_stats(mem_stats_t *stats) {
    kstat_ctl_t *kc;
    kstat_t *ksp;
    kstat_named_t *knp;
    long page_size;
    uint64_t physmem = 0, freemem = 0, pp_kernel = 0;
    struct swaptable *swt;
    struct swapent *ste;
    char path[1024];
    int i, n;
    
    /*
     * Get page size from sysconf
     * Typically 4KB on x86, 8KB on SPARC
     */
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        err(1, "sysconf _SC_PAGESIZE");
    }
    
    /*
     * Open kstat library to access kernel statistics
     * kstat is the Solaris/illumos way to get kernel metrics
     */
    kc = kstat_open();
    if (kc == NULL) {
        err(1, "kstat_open");
    }
    
    /*
     * Read memory statistics from unix:0:system_pages kstat
     * This module contains system-wide page statistics
     */
    ksp = kstat_lookup(kc, "unix", 0, "system_pages");
    if (ksp == NULL) {
        kstat_close(kc);
        errx(1, "kstat_lookup system_pages failed");
    }
    
    if (kstat_read(kc, ksp, NULL) == -1) {
        kstat_close(kc);
        errx(1, "kstat_read failed");
    }
    
    /*
     * Extract memory page counts from kstat
     * - physmem: total physical memory pages
     * - freemem: free memory pages
     * - pp_kernel: pages used by kernel (locked)
     */
    knp = kstat_data_lookup(ksp, "physmem");
    if (knp) physmem = knp->value.ul;
    
    knp = kstat_data_lookup(ksp, "freemem");
    if (knp) freemem = knp->value.ul;
    
    knp = kstat_data_lookup(ksp, "pp_kernel");
    if (knp) pp_kernel = knp->value.ul;
    
    /*
     * Get ZFS ARC statistics if available
     * The ARC (Adaptive Replacement Cache) is ZFS's main cache
     * and can consume a large portion of available memory
     */
    uint64_t arc_size = 0;
    ksp = kstat_lookup(kc, "zfs", 0, "arcstats");
    if (ksp != NULL && kstat_read(kc, ksp, NULL) != -1) {
        knp = kstat_data_lookup(ksp, "size");
        if (knp) arc_size = knp->value.ui64;
    }
    
    kstat_close(kc);
    
    /*
     * Calculate memory statistics
     * On illumos, ZFS ARC is the primary cache mechanism
     */
    stats->mem_total = physmem * page_size;
    stats->mem_free = freemem * page_size;
    stats->mem_wired = pp_kernel * page_size;
    
    /* Simplified: active/inactive not easily available */
    stats->mem_active = 0;
    stats->mem_inactive = 0;
    
    /* ZFS ARC cache size */
    stats->mem_cache = arc_size;
    stats->mem_buffers = 0;
    
    /*
     * Get swap information using swapctl()
     * This is the illumos/Solaris way to query swap space
     */
    n = swapctl(SC_GETNSWP, NULL);
    if (n <= 0) {
        /* No swap configured */
        stats->swap_total = 0;
        stats->swap_used = 0;
        return 0;
    }
    
    /* Allocate space for swap table */
    swt = malloc(sizeof(int) + n * sizeof(struct swapent));
    if (swt == NULL) {
        err(1, "malloc");
    }
    
    swt->swt_n = n;
    ste = &(swt->swt_ent[0]);
    for (i = 0; i < n; i++, ste++) {
        ste->ste_path = path;
    }
    
    /* Get swap table entries */
    if (swapctl(SC_LIST, swt) == -1) {
        free(swt);
        stats->swap_total = 0;
        stats->swap_used = 0;
        return 0;
    }
    
    /* Sum up all swap devices */
    stats->swap_total = 0;
    stats->swap_used = 0;
    ste = &(swt->swt_ent[0]);
    for (i = 0; i < n; i++, ste++) {
        stats->swap_total += (uint64_t)ste->ste_pages * page_size;
        stats->swap_used += (uint64_t)(ste->ste_pages - ste->ste_free) * page_size;
    }
    
    free(swt);
    stats->has_swap_info = 1;
    return 0;
}
#endif

#ifdef __HAIKU__
/*
 * Haiku OS Memory Statistics Retrieval
 * 
 * Haiku is an open-source recreation of BeOS, with its own unique APIs.
 * 
 * Key differences from other systems:
 * - Uses BeOS-style get_system_info() API
 * - Returns system_info structure with memory statistics
 * - Page size from B_PAGE_SIZE constant (typically 4KB)
 * - Much simpler API than BSD sysctl or Solaris kstat
 * - No traditional swap file (uses virtual memory differently)
 * 
 * Memory calculation approach:
 * - Uses system_info structure from get_system_info()
 * - max_pages: total physical memory pages
 * - used_pages: pages currently in use
 * - cached_pages: pages used for cache
 * - page_faults: not used for memory stats
 * 
 * Note: Haiku's memory management is simpler and more BeOS-like
 * than traditional Unix systems.
 */
int retrieve_mem_stats(mem_stats_t *stats) {
    system_info sysinfo;
    
    /*
     * Get system information using Haiku's native API
     * This is much simpler than BSD sysctl or Solaris kstat
     */
    if (get_system_info(&sysinfo) != B_OK) {
        errx(1, "get_system_info failed");
    }
    
    /*
     * Haiku system_info provides:
     * - max_pages: total physical memory pages
     * - used_pages: total virtual pages (can exceed physical!)
     * - cached_pages: pages used for cache
     * - page_faults: not used for memory stats
     * - ignored_pages: system reserved pages
     * 
     * For accurate physical memory stats, we calculate:
     * total = max_pages, free = max_pages - physical usage
     */
    stats->mem_total = (uint64_t)sysinfo.max_pages * B_PAGE_SIZE;
    stats->mem_cache = (uint64_t)sysinfo.cached_pages * B_PAGE_SIZE;
    
    /* 
     * Simple approach: assume cached + some used pages <= max_pages
     * Free = total - cached (treating cache as "used" for now)
     */
    if (sysinfo.cached_pages < sysinfo.max_pages) {
        stats->mem_free = (uint64_t)(sysinfo.max_pages - sysinfo.cached_pages) * B_PAGE_SIZE;
    } else {
        stats->mem_free = 0;
    }
    
    /* Haiku doesn't expose these separately */
    stats->mem_active = 0;
    stats->mem_inactive = 0;
    stats->mem_wired = 0;
    stats->mem_buffers = 0;
    
    /*
     * Haiku doesn't use traditional swap in the same way
     * It has virtual memory but not exposed the same way
     */
    stats->swap_total = 0;
    stats->swap_used = 0;
    stats->has_swap_info = 0;  /* Don't display swap line on Haiku */
    
    return 0;
}
#endif

int main(int argc, char *argv[]) {
    unit_t unit = UNIT_KILO;
    mem_stats_t stats;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bytes") == 0) {
            unit = UNIT_BYTES;
        } else if (strcmp(argv[i], "-k") == 0 || strcmp(argv[i], "--kilo") == 0) {
            unit = UNIT_KILO;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mega") == 0) {
            unit = UNIT_MEGA;
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--giga") == 0) {
            unit = UNIT_GIGA;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--human") == 0) {
            unit = UNIT_HUMAN;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_help();
            return 1;
        }
    }
    
    /* Retrieve memory statistics */
    if (retrieve_mem_stats(&stats) != 0) {
        return 1;
    }
    
    /* Calculate metrics */
    uint64_t buff_cache = stats.mem_cache + stats.mem_buffers;
    uint64_t used, available;
    
#if defined(__NetBSD__) || defined(__OpenBSD__)
    /* NetBSD/OpenBSD: simpler calculation like their free command */
    /* used = total - free (all non-free pages are considered "used") */
    used = stats.mem_total - stats.mem_free;
    /* available = free + inactive + cache (reclaimable memory) */
    available = stats.mem_free + stats.mem_inactive + stats.mem_cache;
#else
    /* FreeBSD/Linux: used = total - available */
    available = stats.mem_free + stats.mem_inactive + stats.mem_cache;
    used = stats.mem_total - available;
#endif
    
    uint64_t swap_free = stats.swap_total - stats.swap_used;
    
    /* Print header */
    printf("%-7s %12s %12s %12s %12s %12s\n",
           "", "total", "used", "free", "buff/cache", "available");
    
    /* Print memory line */
    char buf_total[32], buf_used[32], buf_free[32], buf_buffcache[32], buf_available[32];
    format_value(stats.mem_total, unit, buf_total, sizeof(buf_total));
    format_value(used, unit, buf_used, sizeof(buf_used));
    format_value(stats.mem_free, unit, buf_free, sizeof(buf_free));
    format_value(buff_cache, unit, buf_buffcache, sizeof(buf_buffcache));
    format_value(available, unit, buf_available, sizeof(buf_available));
    
    printf("%-7s %12s %12s %12s %12s %12s\n",
           "Mem:", buf_total, buf_used, buf_free, buf_buffcache, buf_available);
    
    /* Print swap line only if platform provides swap info */
    if (stats.has_swap_info) {
        char buf_swap_total[32], buf_swap_used[32], buf_swap_free[32];
        format_value(stats.swap_total, unit, buf_swap_total, sizeof(buf_swap_total));
        format_value(stats.swap_used, unit, buf_swap_used, sizeof(buf_swap_used));
        format_value(swap_free, unit, buf_swap_free, sizeof(buf_swap_free));
        
        printf("%-7s %12s %12s %12s\n",
               "Swap:", buf_swap_total, buf_swap_used, buf_swap_free);
    }
    
    return 0;
}
