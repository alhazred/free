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

static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "Usage: %s [-k|-m|-g]\n"
	    "Display the amount of free and used system memory\n"),
	    progname);
	exit(1);
}

int main(int argc, char *argv[]) {
	int c;
	unsigned long long pages, total, freemem, avail, kernel,
	    locked, physmem;
	unsigned long long ram_kernel, ram_locked, ram_used, ram_unusable;
	unsigned long long swapalloc, swapresv, swapavail, swaptotal;
	struct anoninfo ai;
	int div = 1;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	progname = basename(argv[0]);
	while ((c = getopt(argc, argv, "gkm")) != -1) {
		switch (c) {
			case 'k':
				div = 1;
				break;
			case 'g':
				div = 1024*1024;
				break;
			case 'm':
				div = 1024;
				break;
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
	total = (pagesize * pages) >> 10;

	kstat_t *ks = kstat_lookup(kc, "unix", 0, "system_pages");
	kstat_read(kc, ks, 0);

	kstat_named_t *knp = kstat_data_lookup(ks, "freemem");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	freemem = ((unsigned long long)knp->value.ui64 * pagesize) >> 10;

	knp = kstat_data_lookup(ks, "availrmem");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	avail = ((unsigned long long)knp->value.ui64 * pagesize) >> 10;

	knp = kstat_data_lookup(ks, "pp_kernel");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	kernel = ((unsigned long long)knp->value.ui64 * pagesize) >> 10;

	knp = kstat_data_lookup(ks, "pageslocked");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	locked = ((unsigned long long)knp->value.ui64 * pagesize) >> 10;

	knp = kstat_data_lookup(ks, "physmem");
	if (knp == NULL) {
		(void) kstat_close(kc);
		return (-1);
	}
	physmem = ((unsigned long long)knp->value.ui64 * pagesize) >> 10;

	kstat_close(kc);

	if (kernel < locked) {
		ram_kernel = kernel;
		ram_locked = locked - kernel;
	} else {
		ram_kernel = locked;
		ram_locked = 0;
	}

	ram_used = avail - freemem;
	ram_unusable = total - physmem;

	swapctl(SC_AINFO, &ai);
	swapalloc  = ai.ani_max - ai.ani_free;
	swapalloc = swapalloc * pagesize >> 10;
	swapresv   = ai.ani_resv + ai.ani_free - ai.ani_max;
	swapresv  = swapresv * pagesize >> 10;
	swapavail  = ai.ani_max - ai.ani_resv;
	swapavail = pagesize * swapavail >> 10;
	swaptotal = ((unsigned long long) ai.ani_max * pagesize) >> 10;

	printf("%16s %10s %10s %10s %10s %10s\n",
	    "total", "used", "free", "locked", "kernel", "unused");

	printf("Mem:  %10llu %10llu %10llu %10llu %10llu %10llu\n"
	    "Swap: %10llu %10llu %10llu\n", total/div, ram_used/div,
	    freemem/div, ram_locked/div, ram_kernel/div,
	    ram_unusable/div, swaptotal/div,
	    swapalloc/div + swapresv/div, swapavail/div);

	return (0);
}
