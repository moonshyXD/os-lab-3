#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make
gcc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L src/*.c tests/thread_failures.c -pthread -lm -Wl,--wrap=pthread_create -Wl,--wrap=pthread_join -o convolution-fail
python3 tests/style.py
python3 tests/check.py
gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L -fsanitize=address,undefined -fno-omit-frame-pointer src/*.c -pthread -lm -o convolution-sanitize
ASAN_OPTIONS=detect_leaks=1 python3 tests/check.py ./convolution-sanitize
