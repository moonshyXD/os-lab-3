#include "console.h"
#include "filter.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int ignore_sigpipe(void) {
    struct sigaction action = {.sa_handler = SIG_IGN};
    if (sigemptyset(&action.sa_mask) == -1 || sigaction(SIGPIPE, &action, NULL) == -1) {
        perror("sigaction");
        return -1;
    }
    return 0;
}

static int process_problem(Matrix *matrix, const Matrix *kernel, size_t iterations, size_t limit) {
    size_t threads = limit < matrix->rows ? limit : matrix->rows;
    fprintf(stderr, "pid=%ld thread_limit=%zu threads=%zu\n", (long)getpid(), limit, threads);
    struct timespec start, finish;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("clock_gettime");
        return -1;
    }
    if (apply_filter(matrix, kernel, iterations, threads) == -1) {
        perror("Фильтрация");
        return -1;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &finish) == -1) {
        perror("clock_gettime");
        return -1;
    }
    double elapsed =
        (double)(finish.tv_sec - start.tv_sec) + (double)(finish.tv_nsec - start.tv_nsec) / 1e9;
    fprintf(stderr, "elapsed_seconds=%.9f\n", elapsed);
    if (print_matrix(matrix) == -1) {
        perror("Вывод матрицы");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    size_t limit;
    int option = parse_options(argc, argv, &limit);
    if (option != 0) {
        return option > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (ignore_sigpipe() == -1) {
        return EXIT_FAILURE;
    }

    Matrix matrix = {0}, kernel = {0};
    size_t iterations = 0;
    int result = read_problem(&matrix, &kernel, &iterations);
    if (result == 0) {
        result = process_problem(&matrix, &kernel, iterations, limit);
    }
    free(matrix.data);
    free(kernel.data);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
