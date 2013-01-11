/*
 * mm-alloc-2.c - virtual memory allocator
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
 * It allocates 6GB of VMEM and tries to touch some of the allocated memory.
 * This creates page faults, and demands physical pages, so RSS increases.
 *
 * compiles with: gcc -Wall -o mm-alloc-2 mm-alloc-2.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MB 1024*1024

int main(int argc, char *argv[])
{
	void *addr, *memblock = NULL;
	int shmid, counter = 0;
	char cmd[64];
	while (1) {
		memblock = (void *)malloc(MB);
		/*
		 * exit if either we've failed to get a memchunk,
		 * or if we've reached 6GB of allocated virtual memory.
		 */
		if(!memblock || counter == 6144)
			break;

		/* touch some allocated memory blocks (page_faults)*/
		if (counter % 6 == 0)
			memset(memblock, 1, MB);

		counter++;
	}

	shmid = shmget(IPC_PRIVATE, 42, IPC_CREAT | 0666);
	if(shmid==-1)
		return -1;

	addr = shmat(shmid, NULL, SHM_RDONLY);

	sprintf(cmd, "grep -E \"Vm(Size|RSS|PTE)|Name|Pid\" /proc/%d/status",
		 getpid());
	system(cmd);
	getc(stdin);
	shmctl(shmid, IPC_RMID, NULL);
	return 0;
}
