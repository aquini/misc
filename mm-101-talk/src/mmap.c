/*
 * mmap.c - simple program for {anon,file}-mapped demand paging examples
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
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/resource.h>

#define KB (1UL << 10)
#define MB (1UL << 20)
#define PG_FAULT_NONE	0x0
#define PG_FAULT_READ	0x1
#define PG_FAULT_WRITE	0x2
#define PG_FAULT_RWLOOP	0x3
#define MMAP_MODE_NONE	0x0
#define MMAP_MODE_ANON	0x1
#define MMAP_MODE_FILE	0x2

const char *prg_name;
const char *short_opts = "ha:f:p:ws";
const struct option long_opts[] = {
		{"help", 0, NULL, 'h'},
		{"anon", 1, NULL, 'a'},
		{"file", 1, NULL, 'f'},
		{"pg-fault", 2, NULL, 'p'},
		{"wait", 0, NULL, 'w'},
		{"stats", 0, NULL, 's'},
		{NULL, 0, NULL, 0} } ;

void print_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "Usage: %s <options>\n", prg_name);
	fprintf(stream,
		"   -h  --help     (Display this usage information)\n"
		"   -a  --anon     <size-in-MB>\n"
		"   -f  --file     <file-name>\n"
		"   -p  --pg-fault [read|write|loop|none]\n"
		"   -s  --stats\n"
		"   -w  --wait \n");
	exit(exit_code);
}

static inline void read_bytes(unsigned long length, void *addr)
{
	unsigned long i;
	unsigned char tmp;
	for (i = 0; i < length; i++)
		tmp += *((unsigned char *)(addr + i));
}

static inline void write_bytes(unsigned long length, void *addr)
{
	unsigned long i;
	for (i = 0; i < length; i++)
		*((unsigned char *)(addr + i)) = 0xff;
}

static inline void *do_mmap(int fd, size_t len)
{
	int map_flags = (fd >= 0) ? MAP_SHARED : MAP_ANONYMOUS | MAP_PRIVATE;
	if (fd >= 0)
		posix_fadvise(fd, 0, len, POSIX_FADV_RANDOM);

	return mmap(NULL, len, PROT_READ | PROT_WRITE, map_flags, fd, 0);
}

int infinite_loop_stop = 0;
static void signal_handler(int signo)
{
	switch (signo) {
	case SIGINT:
	case SIGTERM:
		infinite_loop_stop = 1;
		break;
	default:
		exit(EXIT_FAILURE);
	}
}

static inline void fault_rwloop(unsigned long length, void *addr)
{
	while (!infinite_loop_stop) {
		write_bytes(length, addr);
		sleep(5);
		read_bytes(length, addr);
	}
}

static inline void wait_loop(void)
{
	fprintf(stderr, "%s's PID: %d\n", prg_name, getpid());
	while (!infinite_loop_stop)
		sleep(15);
}

void do_page_fault(void *addr, size_t len, int fault_mode)
{
	switch (fault_mode) {
	case PG_FAULT_READ:
		read_bytes(len, addr);
		break;
	case PG_FAULT_WRITE:
		write_bytes(len, addr);
		break;
	case PG_FAULT_RWLOOP:
		fault_rwloop(len, addr);
		break;
	case PG_FAULT_NONE:
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
	void *addr;
	size_t length;
	struct stat sb;
	struct tms tms;
	struct rusage r1, r2;
	int fd, next_opt;
	long hz, tvali, tvalf;
	double elapsed, us_time, sy_time;
	int fault_mode = PG_FAULT_NONE, mmap_mode = MMAP_MODE_NONE;
	int exit_wait = 0;
	int print_stats = 0;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	prg_name = argv[0];
	do {
		next_opt = getopt_long(argc, argv, short_opts, long_opts, NULL);
		switch (next_opt) {
		case 'p':
			fault_mode = PG_FAULT_NONE;
			if (optarg) {
			        if (strcmp(optarg, "write") == 0)
					fault_mode = PG_FAULT_WRITE;

			        if (strcmp(optarg, "read") == 0)
					fault_mode = PG_FAULT_READ;

			        if (strcmp(optarg, "loop") == 0)
					fault_mode = PG_FAULT_RWLOOP;
			}
			break;

		case 'a':
			mmap_mode = MMAP_MODE_ANON;
			length = atol(optarg) * MB;
			fd = -1;
			break;

		case 'f':
			mmap_mode = MMAP_MODE_FILE;
			fd = open (optarg, O_RDWR);
			if (fd == -1) {
				perror ("open");
				return 1;
			}

			if (fstat (fd, &sb) == -1) {
				perror ("fstat");
				return 1;
			}

			length = sb.st_size;
			break;

		case 's':
			print_stats = 1;
			break;

		case 'w':
			exit_wait = 1;
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

	if (argc == 1 || mmap_mode == MMAP_MODE_NONE)
		print_usage(stderr, 2);

	addr = do_mmap(fd, length);
	if (mmap_mode == MMAP_MODE_FILE)
		close(fd);

	if (addr == MAP_FAILED) {
		perror("mmap");
		return 1;
	}

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

	do_page_fault(addr, length, fault_mode);

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
			length / KB / elapsed);
	}

	if (exit_wait)
		wait_loop();

	return 0;
}
