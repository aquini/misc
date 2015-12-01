/*
 * shmem.c - simple program for sysv-shm demand paging examples
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY 0xbadbeef
#define KB 	(1UL << 10)
#define MB	(1UL << 20)
#define OBJ_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)
#define SHMGET_FAILED     (-1)
#define SHMAT_FAILED      ((void *) -1)
#define OP_MODE_NONE    0x00u
#define OP_MODE_ATTACH  0x01u
#define OP_MODE_CREATE  0x02u
#define OP_MODE_MASK	(OP_MODE_ATTACH|OP_MODE_CREATE)
#define OP_DELETE	0x04u
#define OP_WAIT		0x08u
#define OP_STAT		0x10u
#define PG_FAULT_NONE	0x0
#define PG_FAULT_READ	0x1
#define PG_FAULT_WRITE	0x2
#define PG_FAULT_RWLOOP	0x3

const char *prg_name;
const char *short_opts = "ab:cdf:hi:wstz:";
const struct option long_opts[] = {
		{"attach", 0, NULL, 'a'},
		{"create", 0, NULL, 'c'},
		{"bloat", 1, NULL, 'b'},
		{"delete", 0, NULL, 'd'},
		{"fault", 2, NULL, 'f'},
		{"help", 0, NULL, 'h'},
		{"id", 1, NULL, 'i'},
		{"wait", 0, NULL, 'w'},
		{"shmsz", 1, NULL, 'z'},
		{"stats", 0, NULL, 's'},
		{"tlbhuge", 0, NULL, 't'},
		{NULL, 0, NULL, 0} } ;

void print_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "Usage: %s <options>\n", prg_name);
	fprintf(stream,
		"   -h  --help     (Display this usage information)\n"
		"   -a  --attach\n"
		"   -c  --create\n"
		"   -d  --delete\n"
		"   -f  --fault    [read|write|loop|none]\n"
		"   -k  --key      <shm_key>\n"
		"   -z  --shmsz    <MB>\n"
		"   -s  --stats\n"
		"   -t  --tlbhuge\n"
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

int main(int argc, char *argv[])
{
	int shmid, shm_key, shm_flg = IPC_CREAT | IPC_EXCL | OBJ_PERMS;
	struct shmid_ds shm_buf;
        unsigned long shm_size;
	void *addr, *block;
	size_t bloat;
	int errno, next_opt;
	struct tms tms;
	struct rusage r1, r2;
	long hz, tvali, tvalf;
	double elapsed, us_time, sy_time;
	int mode = OP_MODE_NONE;
	int fault_mode = PG_FAULT_NONE;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	shm_size = 0;
	shm_key = SHM_KEY;
	prg_name = argv[0];
	do {
		next_opt = getopt_long(argc, argv, short_opts, long_opts, NULL);
		switch (next_opt) {
		case 'a':
			mode = OP_MODE_ATTACH;
			break;

		case 'b':
			bloat = atoi(optarg) * MB;
			if ((block = malloc(bloat)))
				memset(block, 0, bloat);
			break;

		case 'c':
			mode = OP_MODE_CREATE;
			break;

		case 'd':
			mode |= OP_DELETE;
			break;

		case 'f':
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

		case 'i':
			shm_key = atoi(optarg);
			break;

		case 'w':
			mode |= OP_WAIT;
			break;

		case 's':
			mode |= OP_STAT;
			break;

		case 't':
			shm_flg |= SHM_HUGETLB;
			break;

		case 'z':
			shm_size = atoi(optarg) * MB;
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

	if (argc == 1 || mode == OP_MODE_NONE ||
	    (mode == OP_MODE_CREATE && shm_size == 0))
		print_usage(stderr, 2);

	switch (mode & OP_MODE_MASK) {
	case OP_MODE_CREATE:
	        shmid = shmget(shm_key, shm_size, shm_flg);
		break;
	case OP_MODE_ATTACH:
		shmid = shmget(shm_key, 0, 0);
		break;
	default:
		print_usage(stderr, 3);
	}

        if (shmid == SHMGET_FAILED) {
                perror("shmget");
                return 1;
        }

	addr = shmat(shmid, NULL, 0);
        if (addr == SHMAT_FAILED) {
                perror("shmat");
                return 1;
        }

	if (mode & OP_STAT) {
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

	switch (fault_mode) {
	case PG_FAULT_READ:
		read_bytes(shm_size, addr);
		break;
	case PG_FAULT_WRITE:
		write_bytes(shm_size, addr);
	case PG_FAULT_RWLOOP:
		fault_rwloop(shm_size, addr);
	default:
		break;
	}

	if (mode & OP_STAT) {
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
			shm_size/ KB / elapsed);
	}

	if (mode & OP_WAIT)
		wait_loop();


	if (mode & OP_DELETE)
		shmctl(shmid, IPC_RMID, &shm_buf);

	return 0;
}
