/*
 * stdio.c - simple program for ordinary stdio file caching examples
 * Copyright (C) 2014 Rafael Aquini <aquini@redhat.com>
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/resource.h>

#define BUFSZ 1024
#define KB (1UL << 10)
#define MB (1UL << 20)
#define STDIO_MODE_NONE		0x0
#define STDIO_MODE_READ		0x1
#define STDIO_MODE_WRITE	0x2

const char *prg_name;
const char *short_opts = "hr:w:s";
const struct option long_opts[] = {
		{"help", 0, NULL, 'h'},
		{"read", 1, NULL, 'r'},
		{"write", 1, NULL, 'w'},
		{"stats", 0, NULL, 's'},
		{NULL, 0, NULL, 0} } ;

void print_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "Usage: %s <options>\n", prg_name);
	fprintf(stream,
		"   -h  --help     (Display this usage information)\n"
		"   -r  --file     <file-name>\n"
		"   -w  --file     <file-name>\n"
		"   -s  --stats\n");
	exit(exit_code);
}

int main(int argc, char *argv[])
{
	size_t ret;
	char buf[BUFSZ];
	int errno, next_opt;
	struct stat sb;
	struct tms tms;
	struct rusage r1, r2;
	long hz, tvali, tvalf;
	double elapsed, us_time, sy_time;
	int mode = STDIO_MODE_NONE;
	int print_stats = 0;
	int fd = -1;

	prg_name = argv[0];
	do {
		next_opt = getopt_long(argc, argv, short_opts, long_opts, NULL);
		switch (next_opt) {
		case 'r':
			mode = STDIO_MODE_READ;
			fd = open(optarg, O_RDONLY);
			break;

		case 'w':
			mode = STDIO_MODE_WRITE;
			fd = open(optarg, O_RDWR);
			break;

		case 's':
			print_stats = 1;
			break;

		case 'h':
			print_usage(stdout, 0);

		case '?':
			print_usage(stderr, 1);

		case -1:
			break;

		default:
			abort();
		}
	} while (next_opt != -1);

	if (argc == 1 || mode == STDIO_MODE_NONE)
		print_usage(stderr, 2);


	if (print_stats) {
		hz = sysconf(_SC_CLK_TCK);
		if (hz == -1) {
			perror("sysconf");
			return 1;
		}

		tvali = times(&tms);

		if (getrusage(RUSAGE_SELF, &r1)) {
			perror("getrusage");
			return 1;
		}
	}

	if (fd != -1) {
		if (fstat(fd, &sb) == -1) {
			perror ("fstat");
			return -1;
		}

		posix_fadvise(fd, 0, sb.st_size, POSIX_FADV_RANDOM);
		if (mode == STDIO_MODE_READ) {
			while ((ret = read(fd, buf, BUFSZ)) > 0)
				if (ret == -1 && errno != EINTR) {
					perror("read");
					break;
				}
		} else if (mode == STDIO_MODE_WRITE) {
			while ((ret = write(fd, buf, BUFSZ)) > 0)
				if (ret == -1 && errno != EINTR) {
					perror("write");
					break;
				}
		}

		close(fd);
	}

	if (print_stats) {
		if (getrusage(RUSAGE_SELF, &r2)) {
			perror("getrusage");
			return 1;
		}

		tvalf = times(&tms);

		elapsed = (double) (tvalf - tvali) / hz;
		us_time = (double) tms.tms_utime / hz;
		sy_time = (double) tms.tms_stime / hz;

		fprintf(stderr, "page faults: %lu minor; %lu major\n",
			r2.ru_minflt - r1.ru_minflt,
			r2.ru_majflt - r1.ru_majflt);

		fprintf(stderr, "time (secs): %0.3lf wall; "
			"%0.3lf user; %0.3lf system\n",
			elapsed, us_time, sy_time);

		fprintf(stderr, "throughput : %0.3lf kB/s \n",
			sb.st_size/ KB / elapsed);
	}

	return ret;
}
