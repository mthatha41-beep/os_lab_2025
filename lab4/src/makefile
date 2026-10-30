CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

TARGETS = parallel_min_max process_memory psum

all: $(TARGETS)

#1
parallel_min_max: parallel_min_max.c find_min_max.c utils.c parallel_min_max.h find_min_max.h utils.h
	$(CC) $(CFLAGS) -o $@ $^

#3
process_memory: process_memory.c
	$(CC) $(CFLAGS) -o $@ $^

# 5
psum: parallel_sum.c sum_lib.c utils.c sum_lib.h utils.h
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGETS)
	rm -f *.o


.PHONY: all clean