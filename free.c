/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2015 Nexenta Systems, Inc. All rights reserved.
 */
/* Based on Brendan Gregg's swapinfo */

#include <stdio.h>
#include <sys/sysinfo.h>
#include <kstat.h>
#include <sys/swap.h>
#include <errno.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <locale.h>

static char *progname;
static kstat_ctl_t *kc = NULL;
static size_t pagesize;


/* nicenum taken from usr/src/lib/libzpool/common/util.c
 * should fix #640 and #5827 first and remove here
 * copyrights:
	 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 */

void
nicenum(uint64_t num, char *buf)
{
	uint64_t n = num;
	int index = 0;
	char u;

	while (n >= 1024) {
		n = (n + (1024 / 2)) / 1024; /* Round up or down */
		index++;
	}

	u = " KMGTPE"[index];

	if (index == 0) {
		(void) sprintf(buf, "%llu", (u_longlong_t)n);
	} else if (n < 10 && (num & (num - 1)) != 0) {
		(void) sprintf(buf, "%.2f%c",
		    (double)num / (1ULL << 10 * index), u);
	} else if (n < 100 && (num & (num - 1)) != 0) {
		(void) sprintf(buf, "%.1f%c",
		    (double)num / (1ULL << 10 * index), u);
	} else {
		(void) sprintf(buf, "%llu%c", (u_longlong_t)n, u);
	}
}


static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "Usage: %s [-p]\n"
	    "Display the amount of free and used system memory\n"),
	    progname);
	exit(1);
}

int main(int argc, char *argv[]) {
	int c;
	unsigned long long pages, total, freemem, avail, kernel,
	    pageslocked, arcsize, pp_kernel;
	unsigned long long locked, used;
	unsigned long long swaptotal, swapused, swapfree;
	char ram_total[6], ram_used[6], ram_free[6], ram_locked[6], ram_kernel[6];
	char ram_cached[6], swap_total[6], swap_used[6], swap_free[6];
	struct anoninfo ai;
	boolean_t parsable = B_FALSE;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	progname = basename(argv[0]);
	while ((c = getopt(argc, argv, "hp")) != -1) {
		switch (c) {
			case 'p':
				parsable = B_TRUE;
				break;
			case 'h': /* fallthrough */
			case '?': /* fallthrough */
			default:
				usage();
				exit(1);
		}
	}

	if ((kc = kstat_open()) == NULL) {
		fprintf(stderr, "kstat_open() failed\n");
		return (-1);
	}

	pagesize = getpagesize();
	pages = sysconf(_SC_PHYS_PAGES);
	total = pagesize * pages;

	kstat_t *ks = kstat_lookup(kc, "unix", 0, "system_pages");
	kstat_read(kc, ks, 0);

	kstat_named_t *knp = kstat_data_lookup(ks, "freemem");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	freemem = (unsigned long long)knp->value.ui64 * pagesize;

	knp = kstat_data_lookup(ks, "availrmem");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	avail = (unsigned long long)knp->value.ui64 * pagesize;

	knp = kstat_data_lookup(ks, "pp_kernel");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	pp_kernel = (unsigned long long)knp->value.ui64 * pagesize;

	knp = kstat_data_lookup(ks, "pageslocked");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	pageslocked = (unsigned long long)knp->value.ui64 * pagesize;

	ks = kstat_lookup(kc, "zfs", 0, "arcstats");
	kstat_read(kc, ks, 0);

	knp = kstat_data_lookup(ks, "size");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	arcsize = (unsigned long long)knp->value.ui64;

	kstat_close(kc);

	if (pp_kernel < pageslocked) {
		kernel = pp_kernel;
		locked = pageslocked - pp_kernel;
	} else {
		kernel = pageslocked;
		locked = 0;
	}

	used = avail - freemem;

	swapctl(SC_AINFO, &ai);
	swaptotal = ai.ani_max * pagesize;
	swapused  = (ai.ani_max - ai.ani_free + ai.ani_resv) * pagesize;
	swapfree  = (ai.ani_free - ai.ani_resv) * pagesize;

	if (parsable) {
		printf("type;total;used;free;locked;kernel;cached\n");
		printf("Mem;%llu;%llu;%llu;%llu;%llu;%llu\n", total,
			used, freemem, locked, kernel, arcsize);
		printf("Swap;%llu;%llu;%llu\n", swaptotal, swapused, swapfree);
	} else {
		nicenum(total, ram_total);
		nicenum(used, ram_used);
		nicenum(freemem, ram_free);
		nicenum(locked, ram_locked);
		nicenum(kernel, ram_kernel);
		nicenum(arcsize, ram_cached);
		nicenum(swaptotal, swap_total);
		nicenum(swapused, swap_used);
		nicenum(swapfree, swap_free);

		printf("%16s %10s %10s %10s %10s %10s\n",
			"total", "used", "free", "locked", "kernel", "cached");
		printf("Mem:  %10s %10s %10s %10s %10s %10s\n",
			ram_total, ram_used, ram_free, ram_locked, ram_kernel, ram_cached);
		printf("Swap: %10s %10s %10s\n", swap_total, swap_used, swap_free);
	}
	return (0);
}
