#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MB (1 << 20)

int main(int argc, char *argv[])
{
	void *memblock;

	while (1) {
		memblock = malloc(MB);
		if (!memblock) {
			perror("malloc");
			break;
		}
		memset(memblock, 1, MB);
	}

	return errno;
}
