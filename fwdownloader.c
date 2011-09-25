/*
 * fwdownloader.c - download simulator based on a simply echo server/client.
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
 * compiles with: gcc -Wall -O2 -o fwdownloader fwdownloader.c
 *
 * This sampling idea and technique was originally introduced by procps tools.
 * this code is just an example which remains consistant to the original tools.
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <netdb.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG	1024
#define BUFSIZE 4096
#define FDOWNLD "/dev/urandom"

int bufsize = BUFSIZE;
char *file_download = FDOWNLD;
const char *prg_name;

static void oops(const char *msg)
{
	if (errno != 0) {
		fprintf(stderr, "Oops: %s: %s\n", strerror(errno), msg);
	} else
		fprintf(stderr, "Oops: %s\n", msg);
	exit(errno);
}

void print_usage(FILE *stream, int exit_code)
{
	fprintf(stream, "Usage: %s <options>\n", prg_name);
	fprintf(stream,
		"   -h  --help 		Display this usage information.\n"
		"   -s  --server 	Run in server mode.\n"
		"   -c  --client <srv>	Run in client mode.\n"
		"   -p  --port   <#> 	Set a port to server/client modes.\n"
		"   -d  --downld <file>	Set the file to use as download src.\n"
		"   -b  --bufsz  <#> 	Set the buffer size to write/read.\n");
	exit(exit_code);
}

static void chld_reaper(int sig)
{
	int saved_errno;

	saved_errno = errno;
	while (waitpid(-1, NULL, WNOHANG) > 0)
		continue;

	errno = saved_errno;
}

int deamonize(void)
{
	int fd, maxfd;
	switch (fork()) {
		case -1:
			return -1;
		case 0:
			break;
		default: 
			_exit(EXIT_SUCCESS);
	}

	if (setsid() == -1)
		return -1;

	switch (fork()) {
		case -1:
			return -1;
		case 0:
			break;
		default: 
			_exit(EXIT_SUCCESS);
	}

	maxfd = sysconf(_SC_OPEN_MAX);
	if (maxfd == -1)
		maxfd = 1024;

	for (fd=0; fd < maxfd; fd++)
		close(fd);

	open("/dev/null", O_RDWR);
	dup(0);
	dup(0);

	return 0;
}

ssize_t readn(int fd, void *buffer, size_t n)
{
	ssize_t reads;
	size_t total_reads;
	char *buf;

	buf = buffer;
	for (total_reads = 0; total_reads < n;) {
		reads = read(fd, buf, n - total_reads);
		if (reads == 0)
			return total_reads;

		if (reads == -1) {
			if (errno == EINTR)
				continue;
			else
				return -1;
		}

		total_reads += reads;
		buf += reads;
	}
	return total_reads;
}

ssize_t writen(int fd, const void *buffer, size_t n)
{
	ssize_t writes;
	size_t total_writes;
	const char *buf;

	buf = buffer;
	for (total_writes = 0; total_writes < n;) {
		writes = write(fd, buf, n - total_writes);
		if (writes <= 0) {
			if (writes == -1 && errno == EINTR)
				continue;
			else
				return -1;
		}

		total_writes += writes;
		buf += writes;
	}
	return total_writes;
}

static void handle_request(int sock_d, int fd)
{
	char buf[bufsize];
	ssize_t reads;
	struct timespec req = { .tv_sec = 0,
				.tv_nsec = 149200000 };
	struct timespec rem;
	int ret, secs, count = 0;
	socklen_t len_inet;
	struct sockaddr_in peer;
	char client_str[bufsize];

	len_inet = sizeof(peer);

	if (getpeername(sock_d, (struct sockaddr *)&peer, &len_inet) == -1) {
		snprintf(client_str, bufsize, "????????:????");
	} else {
		snprintf(client_str, bufsize, "%s:%u",
				inet_ntoa(peer.sin_addr),
						(unsigned)ntohs(peer.sin_port));
	}

	while ((reads = readn(fd, buf, bufsize)) > 0) {
		/* lets sleep a little time pretending we are doing something
		   important -- this is just 149,2 millisec of dummy latency */
		retry:
		ret = nanosleep(&req, &rem);
		if (ret) {
			if (errno == EINTR) {
				req.tv_sec = rem.tv_sec;
				req.tv_nsec = rem.tv_nsec;
				goto retry;
			}
			syslog(LOG_ERR, "nanosleep() failed: %s",
							strerror(errno));
		}

		/* after some 'processing' write back to client */
		if (writen(sock_d, buf, reads) != reads) {
			syslog(LOG_ERR, "write() failed: %s: %s",
						strerror(errno), client_str);
			exit(EXIT_FAILURE);
		}
		count++;

		/* insert some ramdom sleep */
		if (count > (9800 + (rand() %500))) {
			secs = 1 + (rand() % 20);
			sleep(secs);
			count=0;
		}

		if (reads == -1) {
			syslog(LOG_ERR, "read() failed: %s: %s",
						strerror(errno), client_str);
			exit(EXIT_FAILURE);
		}


	}
}

void start_server(int port)
{
	int svsock_d, clsock_d, fd;
	socklen_t len_inet;
	struct sigaction sa;
	struct sockaddr_in svaddr;
	struct sockaddr_in claddr;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = chld_reaper;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		syslog(LOG_ERR, "error from sigaction(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	svsock_d = socket(AF_INET, SOCK_STREAM, 0);
	if (svsock_d == -1) {
		syslog(LOG_ERR,	"error from socket(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	memset(&svaddr, 0, sizeof(svaddr));
	svaddr.sin_family = AF_INET;
	svaddr.sin_port = htons(port);
	svaddr.sin_addr.s_addr = INADDR_ANY;

	len_inet = sizeof(struct sockaddr_in);

	if (bind(svsock_d, (struct sockaddr *)&svaddr, len_inet) == -1) {
		syslog(LOG_ERR,	"error from bind(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (listen(svsock_d, BACKLOG) == -1) {
		syslog(LOG_ERR,	"error from listen(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (;;) {
		clsock_d = accept(svsock_d, (struct sockaddr *)&claddr,
								&len_inet);
		if (clsock_d == -1) {
			syslog(LOG_ERR,	"error from accept(): %s",
							strerror(errno));
			exit(EXIT_FAILURE);
		}

		switch(fork()) {
		case -1:
			syslog(LOG_ERR, "cannot create child (%s)",
							strerror(errno));
			close(clsock_d);
			break;
		case 0:
			close(svsock_d);
			fd = open(file_download, O_RDONLY);
			if (fd != -1) {
				handle_request(clsock_d, fd);
				close(fd);
				exit(EXIT_SUCCESS);
			}
			exit(EXIT_FAILURE);
		default:
			close(clsock_d);
			break;
		}
	}
}

void start_client(char *srv, int port)
{
	int socket_d;
	socklen_t len_inet;
	struct sigaction sa;
	struct sockaddr_in in_addr;
	char buf[bufsize];
	ssize_t reads;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = chld_reaper;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
		oops("error from sigaction()");

	socket_d = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_d == -1)
		oops("error from socket()");

	len_inet = sizeof(struct sockaddr_in);

	memset(&in_addr, 0, len_inet);
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons(port);
	in_addr.sin_addr.s_addr = inet_addr(srv);

	if (connect(socket_d, (struct sockaddr *)&in_addr, len_inet) == -1)
		oops("error from connect()");

	switch(fork()) {
	case -1:
		oops("error from fork()");

	case 0:  /* child -- read server's response */
		for (;;) {
			reads = readn(socket_d, buf, bufsize);
			if (reads <= 0)
				break;

			printf("%.*s", (int) reads, buf);
		}
		exit(EXIT_SUCCESS);

	default: /* parent -- write some stuff to socket every 600s*/
		for (;;) {

			writen(socket_d, "GET SOME", 8);
			sleep(600);
		}
		exit(EXIT_SUCCESS);
	}
}

int main(int argc, char *argv[])
{
	int next_opt, mode = 0;
	char *server = NULL;
	const char *short_opts = "hsc:p:d:b:";
	const struct option long_opts[] = {
		{"help", 0, NULL, 'h'},
		{"server", 0, NULL, 's'},
		{"client", 1, NULL, 'c'},
		{"port", 1, NULL, 'p'},
		{"downld", 1, NULL, 'd'},
		{"bufsz", 1, NULL, 'b'},
		{NULL, 0, NULL, 0} } ;

	int port_num = 9999;
	prg_name = argv[0];

	do {
		next_opt = getopt_long(argc, argv, short_opts, long_opts, NULL);
		switch (next_opt) {
			case 'h':
				print_usage(stdout, 0);

			case 's':
				mode += 1;
				break;

			case 'c':
				mode += 2;
				server = optarg;
				break;

			case 'd':
				file_download = optarg;
				break;

			case 'p':
				port_num = atoi(optarg);
				break;

			case 'b':
				bufsize = atoi(optarg);
				break;

			case '?':
				print_usage(stderr, 1);

			case -1:
				break;

			default:
				abort();
		}
	} while (next_opt != -1);

	if (argc == 1)
		print_usage(stderr, 2);

	if (mode == 0 || mode > 2)
		oops("Ok, I cannot realize what you want now...");

	switch (mode) {
		case 1: /* server mode */
			if (deamonize() == -1)
				oops("failed to become a deamon.");

			start_server(port_num);
			break;
		case 2: /* client mode */
			start_client(server, port_num);
			break;
	}

	return 0;
}
