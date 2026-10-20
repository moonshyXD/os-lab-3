CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L

convolution: src/*.c src/*.h
	$(CC) $(CFLAGS) src/*.c -o convolution -pthread -lm

clean:
	rm -f convolution convolution-fail convolution-sanitize

.PHONY: clean
