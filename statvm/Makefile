# Default C compiler options.
CFLAGS 		= -O2 -g -Wall

# C source files.
SOURCES 	= statvm.c 

# Corresponding object files.
OBJECTS 	= $(SOURCES:.c=.o)

.PHONY: 	all clean

# Default target: build everything.
all: 	statvm

# Clean up build products.
clean:
	rm -f $(OBJECTS) statvm

statvm:	$(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ 

$(OBJECTS): 	statvm.c statvm.h

