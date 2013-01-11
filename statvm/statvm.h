/*
 * statvm.h - collection of helper routines and definitions to statvm.c
 * Copyright (C) 2011 Rafael Aquini <aquini@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This file is based and adapted from page-types.c tool found at
 * <linux>/Documentation/vm/, which is licensed under the same
 * GPL terms and shows the following copyright notice:
 * * Copyright (C) 2009 Intel corporation
 * * Authors: Wu Fengguang <fengguang.wu@intel.com>
 */
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <assert.h>
#include <memory.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/statfs.h>

/* Description of long options for getopt_long. */
static const struct option l_opts[] = {
   { "pid", 1, NULL, 'p' },
   { "maps", 0, NULL, 'm' },
   { "list", 0, NULL, 'l' },
   { "help", 0, NULL, 'h' },
};

/* Description of short options for getopt_long. */
static const char* const s_opts = "p:mlh";


/* */
static const char* const usage_template =
   "Usage: %s [ options ]\n"
   "   -h, --help : Print this information screen.\n"
   "   -l, --list : Dump info for the task list, similarly to 'ps' (default)\n"
   "   -m, --maps : Dump info for the task list, similarly to 'pmap'\n"
   "   -p, --pid <pid-#> : Restrict the dump out info for <pid-#> only,"
   " instead of doing it for all process list (as in default mode)\n\n";

#define	INFO_LIST  0
#define	INFO_MAPS  1

#define BATCH			128
#define KPAGEFLAGS_BATCH	(BATCH << 10)
#define PAGEMAP_BATCH		(BATCH << 10)
#define BUFFER_SIZE		(BATCH << 4)

#ifndef MAX_PATH
# define MAX_PATH 256
#endif

#ifndef STR
# define _STR(x) #x
# define STR(x) _STR(x)
#endif

/*
 * pagemap kernel ABI bits
 */
#define PM_ENTRY_BYTES      sizeof(uint64_t)
#define PM_STATUS_BITS      3
#define PM_STATUS_OFFSET    (64 - PM_STATUS_BITS)
#define PM_STATUS_MASK      (((1LL << PM_STATUS_BITS) - 1) << PM_STATUS_OFFSET)
#define PM_STATUS(nr)       (((nr) << PM_STATUS_OFFSET) & PM_STATUS_MASK)
#define PM_PSHIFT_BITS      6
#define PM_PSHIFT_OFFSET    (PM_STATUS_OFFSET - PM_PSHIFT_BITS)
#define PM_PSHIFT_MASK      (((1LL << PM_PSHIFT_BITS) - 1) << PM_PSHIFT_OFFSET)
#define PM_PSHIFT(x)        (((u64) (x) << PM_PSHIFT_OFFSET) & PM_PSHIFT_MASK)
#define PM_PFRAME_MASK      ((1LL << PM_PSHIFT_OFFSET) - 1)
#define PM_PFRAME(x)        ((x) & PM_PFRAME_MASK)

#define PM_PRESENT          PM_STATUS(4LL)
#define PM_SWAP             PM_STATUS(2LL)


/*
 * kernel page flags
 */
#define KPF_BYTES		8
#define PROC_KPAGEFLAGS		"/proc/kpageflags"

/* copied from kpageflags_read() */
#define KPF_LOCKED		0
#define KPF_ERROR		1
#define KPF_REFERENCED		2
#define KPF_UPTODATE		3
#define KPF_DIRTY		4
#define KPF_LRU			5
#define KPF_ACTIVE		6
#define KPF_SLAB		7
#define KPF_WRITEBACK		8
#define KPF_RECLAIM		9
#define KPF_BUDDY		10

/* [11-20] new additions in 2.6.31 */
#define KPF_MMAP		11
#define KPF_ANON		12
#define KPF_SWAPCACHE		13
#define KPF_SWAPBACKED		14
#define KPF_COMPOUND_HEAD	15
#define KPF_COMPOUND_TAIL	16
#define KPF_HUGE		17
#define KPF_UNEVICTABLE		18
#define KPF_HWPOISON		19
#define KPF_NOPAGE		20
#define KPF_KSM			21

/* [32-] kernel hacking assistances */
#define KPF_RESERVED		32
#define KPF_MLOCKED		33
#define KPF_MAPPEDTODISK	34
#define KPF_PRIVATE		35
#define KPF_PRIVATE_2		36
#define KPF_OWNER_PRIVATE	37
#define KPF_ARCH		38
#define KPF_UNCACHED		39

/* [48-] take some arbitrary free slots for expanding overloaded flags
 * not part of kernel API
 */
#define KPF_READAHEAD		48
#define KPF_SLOB_FREE		49
#define KPF_SLUB_FROZEN		50
#define KPF_SLUB_DEBUG		51

#define KPF_ALL_BITS		((uint64_t)~0ULL)
#define KPF_HACKERS_BITS	(0xffffULL << 32)
#define KPF_OVERLOADED_BITS	(0xffffULL << 48)
#define BIT(name)		(1ULL << KPF_##name)
#define BITS_COMPOUND		(BIT(COMPOUND_HEAD) | BIT(COMPOUND_TAIL))

#define MAX_BIT_FILTERS 64
static int              nr_bit_filters;
static uint64_t         opt_mask[MAX_BIT_FILTERS];
static uint64_t         opt_bits[MAX_BIT_FILTERS];

static int		page_size;
static int		pagemap_fd;
static int		kpageflags_fd;

#define HASH_SHIFT	13
#define HASH_SIZE	(1 << HASH_SHIFT)
#define HASH_MASK	(HASH_SIZE - 1)
#define HASH_KEY(flags)	(flags & HASH_MASK)


/*
 * helper functions
 */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1 : __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1 : __max2; })

static void fatal(const char *x, ...)
{
	va_list ap;

	va_start(ap, x);
	vfprintf(stderr, x, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

static int checked_open(const char *pathname, int flags)
{
	int fd = open(pathname, flags);

	if (fd < 0) {
		perror(pathname);
		exit(EXIT_FAILURE);
	}

	return fd;
}

static unsigned long do_u64_read(int fd, char *name,
				 uint64_t *buf,
				 unsigned long index,
				 unsigned long count)
{
	long bytes;

	if (index > ULONG_MAX / 8)
		fatal("index overflow: %lu\n", index);

	if (lseek(fd, index * 8, SEEK_SET) < 0) {
		perror(name);
		exit(EXIT_FAILURE);
	}

	bytes = read(fd, buf, count * 8);
	if (bytes < 0) {
		perror(name);
		exit(EXIT_FAILURE);
	}
	if (bytes % 8)
		fatal("partial read: %lu bytes\n", bytes);

	return bytes / 8;
}

static unsigned long kpageflags_read(uint64_t *buf,
				     unsigned long index,
				     unsigned long pages)
{
	return do_u64_read(kpageflags_fd, PROC_KPAGEFLAGS, buf, index, pages);
}

static unsigned long pagemap_read(uint64_t *buf,
				  unsigned long index,
				  unsigned long pages)
{
	return do_u64_read(pagemap_fd, "/proc/pid/pagemap", buf, index, pages);
}

static unsigned long pagemap_pfn(uint64_t val)
{
	unsigned long pfn;

	if (val & PM_PRESENT)
		pfn = PM_PFRAME(val);
	else
		pfn = 0;

	return pfn;
}

static int bit_mask_ok(uint64_t flags)
{
	int i;

	for (i = 0; i < nr_bit_filters; i++) {
		if (opt_bits[i] == KPF_ALL_BITS) {
			if ((flags & opt_mask[i]) == 0)
				return 0;
		} else {
			if ((flags & opt_mask[i]) != opt_bits[i])
				return 0;
		}
	}

	return 1;
}

static uint64_t expand_overloaded_flags(uint64_t flags)
{
	/* SLOB/SLUB overload several page flags */
	if (flags & BIT(SLAB)) {
		if (flags & BIT(PRIVATE))
			flags ^= BIT(PRIVATE) | BIT(SLOB_FREE);
		if (flags & BIT(ACTIVE))
			flags ^= BIT(ACTIVE) | BIT(SLUB_FROZEN);
		if (flags & BIT(ERROR))
			flags ^= BIT(ERROR) | BIT(SLUB_DEBUG);
	}

	/* PG_reclaim is overloaded as PG_readahead in the read path */
	if ((flags & (BIT(RECLAIM) | BIT(WRITEBACK))) == BIT(RECLAIM))
		flags ^= BIT(RECLAIM) | BIT(READAHEAD);

	return flags;
}

static uint64_t well_known_flags(uint64_t flags)
{
	/* hide flags intended only for kernel hacker */
	flags &= ~KPF_HACKERS_BITS;

	/* hide non-hugeTLB compound pages */
	if ((flags & BITS_COMPOUND) && !(flags & BIT(HUGE)))
		flags &= ~BITS_COMPOUND;

	return flags;
}

static uint64_t kpageflags_flags(uint64_t flags)
{
	flags = expand_overloaded_flags(flags);
	flags = well_known_flags(flags);

	return flags;
}

