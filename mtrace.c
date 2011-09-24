/*
 * mtrace.c - enables the use of the malloc() tracer facility by binary apps
 *	      through wrapping mtrace() call via shlib.
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
 * Compile with: gcc mtrace.c -Wall -fPIC -shared -o mtrace.so
 * Run with: MALLOC_TRACE=mtrace.txt LD_PRELOAD=./mtrace.so /bin/echo 42
 */
#include <stdio.h>
#include <stdlib.h>
#include <mcheck.h>
#include <unistd.h>
#include <sys/types.h>

void __mtrace_on(void) __attribute__((constructor));
void __mtrace_off(void) __attribute__((destructor));

void __mtrace_on(void)
{
	char *ptr_env = getenv("MALLOC_TRACE");
	char mtrace_buf[512];
	if(!ptr_env)
		ptr_env = "mtrace";

	sprintf(mtrace_buf, "%s.%d", ptr_env, getpid());
	setenv("MALLOC_TRACE", mtrace_buf, 1);
	atexit(&__mtrace_off);
	mtrace();
}

void __mtrace_off(void)
{
	muntrace();
}
