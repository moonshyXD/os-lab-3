CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L

convolution: src/*.c src/*.h
	$(CC) $(CFLAGS) src/*.c -o convolution -pthread -lm

strace: convolution report/inc/strace-full-input.txt
	@set -eu; \
	mkdir -p tmp; \
	trace_dir=$$(mktemp -d tmp/strace.XXXXXX); \
	printf 'strace: %s/trace.txt\nstdout: %s/result.txt\nstderr: %s/diagnostic.txt\n' \
		"$$trace_dir" "$$trace_dir" "$$trace_dir"; \
	strace -f -tt -T -o "$$trace_dir/trace.txt" ./convolution --threads 4 \
		< report/inc/strace-full-input.txt \
		> "$$trace_dir/result.txt" 2> "$$trace_dir/diagnostic.txt"

clean:
	rm -f convolution

.PHONY: strace clean
