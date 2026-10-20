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

static double monotonic_seconds(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        perror("clock_gettime");
        return -1.0;
    }
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

static int process_problem(Matrix *matrix, const Matrix *kernel, size_t iterations, size_t limit) {
    size_t threads = limit < matrix->rows ? limit : matrix->rows;
    fprintf(stderr, "pid=%ld thread_limit=%zu threads=%zu\n", (long)getpid(), limit, threads);
    double start = monotonic_seconds();
    if (start < 0) {
        return -1;
    }
    if (apply_filter(matrix, kernel, iterations, threads) == -1) {
        perror("Фильтрация");
        return -1;
    }
    double finish = monotonic_seconds();
    if (finish < 0) {
        return -1;
    }
    double elapsed = finish - start;
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
