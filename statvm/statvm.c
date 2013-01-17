/*
 * statvm.c - collects memory statistics for processes in the system.
 * Copyright (C) 2011  Rafael Aquini <aquini@redhat.com>
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
 * This program utilizes the new set of kernel interfaces (> 2.6.25)
 * that allow userspace programs to examine the page tables and
 * related information by reading files in /proc -- kernel pagemap.
 *
 * compiles with: gcc -Wall -o statvm statvm.c
 */
#include "statvm.h"

typedef struct vma_info vma_info_t;
struct vma_info {
	vma_info_t		 *next;
	char			 *map_name;
	long unsigned int	 vm_start;
	long unsigned int	 vm_end;
	unsigned char		 vm_perms[4];
	short unsigned int	 devnode[2];
	long unsigned int	 pg_offset;
	long unsigned int	 inode;
	long unsigned int	 pres_pages;
	long unsigned int	 swap_pages;
	long unsigned int	 active_pgs;
	long unsigned int	 inactive_pgs;
};

typedef struct task_info task_info_t;
struct task_info {
	task_info_t		 *next;
	long unsigned int	 pid;
	char			 *cmdline;
	vma_info_t		 *mm;
	long unsigned int	 pres_pages;
	long unsigned int	 swap_pages;
	long unsigned int	 active_pgs;
	long unsigned int	 inactive_pgs;
};

static void append_vma(vma_info_t **headref, vma_info_t vma_node)
{
	vma_info_t *current = *headref;
	vma_info_t *new;

	new = malloc(sizeof(vma_info_t));
	if (new) {
		*new = vma_node;
		new->next = NULL;
		new->pres_pages = 0;
		new->swap_pages = 0;
		new->active_pgs = 0;
		new->inactive_pgs = 0;

		if (current == NULL) {
			*headref = new;
		} else {
			while (current->next != NULL)
				current = current->next;

			current->next = new;
		}
	}
}

static void append_task(task_info_t **headref, task_info_t task_node)
{
	task_info_t *current = *headref;
	task_info_t *new;

	new = malloc(sizeof(task_info_t));
	if (new) {
		*new = task_node;
		new->next = NULL;

		if (current == NULL) {
			*headref = new;
		} else {
			while (current->next != NULL)
				current = current->next;

			current->next = new;
		}
	}
}

char *get_mapping_name(const char *mapbuf, vma_info_t * vma)
{
	char *cp, *mapname = NULL;
	int len;

	cp = strchr(mapbuf, '/');
	if (cp) {
		if (strstr(mapbuf, "/SYSV"))
			asprintf(&mapname, "[shmid: %lu]", vma->inode);
		else {
			len = strlen(cp);
			mapname = malloc(len);
			if (mapname)
				snprintf(mapname, len, "%s", cp);
		}
		goto out;
	}

	cp = strrchr(mapbuf, '[');
	if (cp) {
		len = strlen(cp);
		mapname = malloc(len);
		if (mapname)
			snprintf(mapname, len, "%s", cp);
		goto out;
	}

	asprintf(&mapname, "[anon]");
out:
	return mapname;
}

vma_info_t *get_vma_info(task_info_t *task)
{
	int n;
	FILE *fd;
	char buffer[BUFFER_SIZE];
	int pid = task->pid;
	vma_info_t *vma_head = NULL;
	vma_info_t vma;

	sprintf(buffer, "/proc/%d/maps", pid);
	fd = fopen(buffer, "r");
	if (fd <= 0) {
		perror(buffer);
		exit(1);
	}

	while (fgets(buffer, BUFFER_SIZE, fd) != NULL) {
		n = sscanf(buffer, "%lx-%lx %c%c%c%c %lx %hu:%hu %lu",
			   &vma.vm_start, &vma.vm_end, &vma.vm_perms[0],
			   &vma.vm_perms[1], &vma.vm_perms[2], &vma.vm_perms[3],
			   &vma.pg_offset, &vma.devnode[0], &vma.devnode[1],
			   &vma.inode);
		vma.next = NULL;

		if (n > 1) {
			vma.map_name = get_mapping_name(buffer, &vma);
			append_vma(&vma_head, vma);
		}
	}

	fclose(fd);
	return vma_head;
}

task_info_t get_task_info(int pid)
{
	task_info_t task;
	int fd, len;
	char buffer[BUFFER_SIZE], *cmd;

	snprintf(buffer, sizeof(buffer), "/proc/%d/comm", pid);
	fd = open(buffer, O_RDONLY);
	if (fd < 0) {
		perror(buffer);
		exit(1);
	}

	len = read(fd, buffer, BUFFER_SIZE);
	close(fd);
	buffer[len] = '\0';

	len = strlen(buffer);
	cmd = malloc(len);
	if (cmd)
		snprintf(cmd, len, "%s", buffer);

	task.next = NULL;
	task.pid = pid;
	task.cmdline = cmd;
	task.mm = get_vma_info(&task);

	return task;
}

static void task_account_pages(task_info_t *task)
{
	unsigned long pres = 0;
	unsigned long swap = 0;
	unsigned long active = 0;
	unsigned long inactive = 0;
	vma_info_t *vma = task->mm;

	while(vma != NULL) {
		pres += vma->pres_pages;
		swap += vma->swap_pages;
		active += vma->active_pgs;
		inactive += vma->inactive_pgs;
		vma = vma->next;
	}
	task->pres_pages = pres;
	task->swap_pages = swap;
	task->active_pgs = active;
	task->inactive_pgs = inactive;
}

static void add_page(unsigned long voffset, unsigned long offset,
		     uint64_t flags, vma_info_t *vma)
{
	flags = kpageflags_flags(flags);
	if (!bit_mask_ok(flags))
		return;

	if (flags & KPF_ACTIVE)
		vma->active_pgs++;
	else
		vma->inactive_pgs++;

	vma->pres_pages++;
}

static void walk_pfn(unsigned long voffset, unsigned long index,
		     unsigned long count, vma_info_t *vma)
{
	uint64_t buffer[KPAGEFLAGS_BATCH];
	unsigned long batch;
	unsigned long pages;
	unsigned long i;

	while (count) {
		batch = min_t(unsigned long, count, KPAGEFLAGS_BATCH);
		pages = kpageflags_read(buffer, index, batch);
		if (pages == 0)
			break;

		for (i = 0; i < pages; i++)
			add_page(voffset + i, index + i, buffer[i], vma);

		index += pages;
		count -= pages;
	}
}

static void walk_vma(unsigned long index, unsigned long count, vma_info_t *vma)
{
	uint64_t buffer[PAGEMAP_BATCH];
	unsigned long batch;
	unsigned long pages;
	unsigned long pfn;
	unsigned long i;

	while (count) {
		batch = min_t(unsigned long, count, PAGEMAP_BATCH);
		pages = pagemap_read(buffer, index, batch);
		if (pages == 0)
			break;

		for(i = 0; i < pages; i++) {
			pfn = buffer[i];
			if (pfn & PM_PRESENT)
				walk_pfn(index + i, PM_PFRAME(pfn), 1, vma);

			if (pfn & PM_SWAP)
				vma->swap_pages++;
		}

		index += pages;
		count -= pages;
	}
}

static void walk_task(task_info_t *task)
{
	char buffer[BUFFER_SIZE];
	const unsigned long end = ULONG_MAX;
	unsigned long start = 0;
	unsigned long index = 0;
	vma_info_t *vma = task->mm;

        sprintf(buffer, "/proc/%ld/pagemap", task->pid);
        pagemap_fd = checked_open(buffer, O_RDONLY);

	while (vma != NULL) {
		if (vma->vm_start >= end)
			return;

		start = max_t(unsigned long, (vma->vm_start/page_size), index);
		index = min_t(unsigned long, (vma->vm_end / page_size), end);

		assert(start < index);
		walk_vma(start, (index - start), vma);
		vma = vma->next;
	}
	close(pagemap_fd);
}

static void print_vma_info(vma_info_t *vmas)
{
	unsigned long virt_kb, virt_total = 0;
	unsigned long pres_kb, pres_total = 0;
	unsigned long swap_kb, swap_total = 0;
	unsigned long active_kb, active_total = 0;
	unsigned long inactive_kb, inactive_total = 0;

        while(vmas != NULL) {
		virt_kb = ((vmas->vm_end - vmas->vm_start) >> 10);
		virt_total += virt_kb;
		pres_kb = ((vmas->pres_pages * page_size) >> 10);
		pres_total += pres_kb;
		swap_kb = ((vmas->swap_pages * page_size) >> 10);
		swap_total += swap_kb;
		active_kb = ((vmas->active_pgs * page_size) >> 10);
		active_total += active_kb;
		inactive_kb = ((vmas->inactive_pgs * page_size) >> 10);
		inactive_total += inactive_kb;

		printf("0x%016lx %8lu %8lu %6lu %6lu %8lu %c%c%c%c %s\n",
			vmas->vm_start,	virt_kb, pres_kb,
			active_kb, inactive_kb, swap_kb,
			vmas->vm_perms[0], vmas->vm_perms[1],
			vmas->vm_perms[2], vmas->vm_perms[3], vmas->map_name);

		vmas = vmas->next;
        }
	printf("%18s %8lu %8lu %6lu %6lu %8lu\n", "Total:",
		 virt_total, pres_total,
		 active_total, inactive_total, swap_total);
}

static void print_info(task_info_t *task, int mode)
{
	kpageflags_fd = checked_open(PROC_KPAGEFLAGS, O_RDONLY);

	if (mode == INFO_LIST)
		printf("%7s %8s %8s %8s %8s %s\n",
		       "PID", "RSS", "ACTIVE", "INACTIVE", "SWAP", "COMM");

	while(task != NULL) {
		walk_task(task);
		task_account_pages(task);
		switch (mode) {
		case INFO_LIST:
			if (task->pres_pages > 0)
				printf("%7ld %8lu %8lu %8lu %8lu %s\n",
				       task->pid,
				       ((task->pres_pages * page_size) >> 10),
				       ((task->active_pgs * page_size) >> 10),
				       ((task->inactive_pgs * page_size) >> 10),
				       ((task->swap_pages * page_size) >> 10),
				       task->cmdline);
			break;
		case INFO_MAPS:
			if (task->pres_pages > 0) {
				printf("PID: %ld COMM: %s\n",
					task->pid, task->cmdline);
				printf("%18s %8s %8s %6s %6s %8s %4s %s\n",
					"Address", "VirtSz", "Rss", "Active",
					"Inactv", "Swap", "mode", "Mapping");

				print_vma_info(task->mm);
				printf("=================================\n\n");
			}
			break;
		}
		task = task->next;
	}
	close(kpageflags_fd);
}

void release_memory(task_info_t *head)
{
	vma_info_t *vma, *oldvma;
	task_info_t *pid, *oldpid;

	pid = head;
	while(pid != NULL) {
		vma = pid->mm;
		while(vma != NULL) {
			oldvma = vma;
			vma = vma->next;
			free(oldvma->map_name);
			free(oldvma);
		}
		oldpid = pid;
		pid = pid->next;
		free(oldpid->cmdline);
		free(oldpid);
	}
}


static void print_usage(const char *program_name)
{
	fprintf(stderr, usage_template, program_name);
	return;
}

int main(int argc, char *argv[])
{
	task_info_t *list_head = NULL;
	task_info_t task;
	page_size = getpagesize();
	uid_t uid = getuid();
	int opt, mode, pid;
	DIR *d;
	struct dirent *de;

	mode = pid = 0;

	if (uid) {
		printf("WARN: Insufficient user privileges for uid: %d\n", uid);
		print_usage(argv[0]);
		return -1;
	}

	while ((opt=getopt_long(argc, argv, s_opts, l_opts, NULL)) != -1) {
		switch (opt) {
		case 'l':
		case 'm':
			/* selects the printout mode */
			mode = opt;
			break;
		case 'p':
			pid = atoi(optarg);
			task = get_task_info(pid);
			append_task(&list_head, task);
			break;
		case 'h':
			print_usage(argv[0]);
			return -1;
		case '?':
		case -1:
			/* done with options */
			break;
		}
	}

	if (!pid) {
		d = opendir("/proc");
		while ((de = readdir(d)))
			if (de->d_name[0] >= '0' && de->d_name[0] <= '9') {
				task = get_task_info(atoi(de->d_name));
				append_task(&list_head, task);
			}
		closedir(d);
	}

	switch (mode) {
	case 'l':
	case 0:
		print_info(list_head, INFO_LIST);
		break;
	case 'm':
		print_info(list_head, INFO_MAPS);
		break;
	}

	release_memory(list_head);

	return 0;
}
