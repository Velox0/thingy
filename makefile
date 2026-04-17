CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

all: thingy

thingy: main.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f thingy

format:
	@astyle --indent=spaces=2 main.c

.PHONY: all clean format
