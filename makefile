CC= gcc
CFLAGS= -Wall -Wextra -pedantic -std=c99

thingy: main.c
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm thingy
