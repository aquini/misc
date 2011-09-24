/*
 * idletime.c - demonstrates how to sample /proc/stat file.
 * Copyright (C) 2011 Rafael Aquini <aquini@redhat.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License, either version 2 of the License, or
 * (at your option) any later version
 *
 * compiles with: gcc -Wall -O2 -o idletime idletime.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFSIZE (64*1024)
typedef unsigned long long jiff;

void getstat(jiff *cusr, jiff *cnic, jiff *csys, jiff *cidl,
	     jiff *ciow, jiff *chwi, jiff *cswi, jiff *cstl)
{
	int fd;
	char buff[BUFFSIZE];
	const char *b;

	fd = open("/proc/stat", O_RDONLY, 0);
	if (fd == -1)
		exit(1);

	read(fd, buff, BUFFSIZE-1);

	*cusr = 0;
	*cnic = 0;
	*csys = 0;
	*cidl = 0;
	*ciow = 0;
	*chwi = 0;
	*cswi = 0;
	*cstl = 0;

	b = strstr(buff, "cpu ");
	if (b)
		sscanf(b, "cpu %Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
			cusr, cnic, csys, cidl, ciow, chwi, cswi, cstl);

	close(fd);
}

int main(int argc, char *argv[])
{
	unsigned int sleep_time = 1;
	int tog = 0;
	int debt = 0;
	int count = 0;
	jiff c_usr[2], c_nic[2], c_sys[2], c_idl[2];
	jiff c_iow[2], c_hwi[2], c_swi[2], c_stl[2];
	jiff usr, sys, idl, iow, stl, total_jiffies, divo2;

	if (argc > 1)
		sleep_time = atoi(argv[1]);

	getstat(c_usr, c_nic, c_sys, c_idl, c_iow, c_hwi, c_swi, c_stl);

	while (1) {
		tog = !tog;
		sleep(sleep_time);
		getstat(c_usr+tog, c_nic+tog, c_sys+tog, c_idl+tog,
			c_iow+tog, c_hwi+tog, c_swi+tog, c_stl+tog);

		usr = ((c_usr[tog] - c_usr[!tog]) +
			(c_nic[tog] - c_nic[!tog]));
		sys = ((c_sys[tog] - c_sys[!tog]) +
			(c_hwi[tog] - c_hwi[!tog]) +
			(c_swi[tog] - c_swi[!tog]));
		idl = (c_idl[tog] - c_idl[!tog]);
		iow = (c_iow[tog] - c_iow[!tog]);
		stl = (c_stl[tog] - c_stl[!tog]);

		if ((int )idl < 0) {
			debt = (int )idl;
			idl = 0;
		}

		if (debt) {
			idl = (int )idl + debt;
			debt = 0;
		}

		/* total jiffies per tuple */
		total_jiffies = usr + sys + idl + iow + stl;

		/* as we are doing integer arithmetic,
		 * this is used to round decimal parts up
		 */
		divo2 = total_jiffies/2UL;

		if ((count % 20) == 0)
			printf("%%user\t%%sys\t%%idle\t%%iowait\t%%steal" \
				"\tDjiffies\n");

		printf("%3u\t%3u\t%3u\t%3u\t%3u\t%Lu\n",
			(unsigned )((100 * usr + divo2) / total_jiffies),
			(unsigned )((100 * sys + divo2) / total_jiffies),
			(unsigned )((100 * idl + divo2) / total_jiffies),
			(unsigned )((100 * iow + divo2) / total_jiffies),
			(unsigned )((100 * stl + divo2) / total_jiffies),
			total_jiffies);

		count++;
	}

	return 0;
}
