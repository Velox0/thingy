CC= gcc
CFLAGS= -Wall -Wextra -pedantic -std=c99

thingy: main.c
	@astyle --indent=spaces=2 $^
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm thingy
