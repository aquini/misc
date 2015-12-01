CC = gcc
CFLAGS = -Wall
TESTS = mmap oom-teaser shmem stdio

all: $(TESTS)

$(TESTS): %: src/%.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	@rm -f $(TESTS)

clean-dat:
	@rm -f *.dat

clean-all: clean-dat clean

bigfile:
	@fsz1=`awk '/MemTotal/ {print $$2*0.8}' /proc/meminfo`; \
	fsz2=`awk '/MemTotal/ {print $$2*0.5}' /proc/meminfo`; \
	dd if=/dev/zero bs=1K count=$$fsz1 of=bigfile1.dat; \
	dd if=/dev/zero bs=1K count=$$fsz2 of=bigfile2.dat
