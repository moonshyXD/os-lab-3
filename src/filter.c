#include "filter.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const Matrix *source;
    const Matrix *kernel;
    Matrix *destination;
    size_t first_row;
    size_t end_row;
    int result;
    pthread_t thread;
} RowTask;

static void *run_rows(void *argument) {
    RowTask *task = argument;
    task->result = convolve_rows(
        task->source, task->kernel, task->destination, task->first_row, task->end_row);
    return NULL;
}

static int join_workers(RowTask *tasks, size_t started) {
    int result = 0;
    for (size_t i = 0; i < started; ++i) {
        int error = pthread_join(tasks[i].thread, NULL);
        if (error != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(error));
            _Exit(EXIT_FAILURE);
        }
        if (tasks[i].result != 0) {
            result = -1;
        }
    }
    return result;
}

static int start_workers(RowTask *tasks, size_t count, RowTask *main_task, size_t *started) {
    size_t chunk = main_task->source->rows / count;
    size_t remainder = main_task->source->rows % count;
    for (size_t i = 0; i < count - 1; ++i) {
        size_t end = main_task->first_row + chunk + (i < remainder);
        tasks[i] = *main_task;
        tasks[i].end_row = end;
        int error = pthread_create(&tasks[i].thread, NULL, run_rows, &tasks[i]);
        if (error != 0) {
            return error;
        }
        ++*started;
        main_task->first_row = end;
    }
    return 0;
}

static int run_pass(const Matrix *source, const Matrix *kernel, Matrix *destination, size_t limit) {
    size_t count = limit < source->rows ? limit : source->rows;
    RowTask main_task = {
        .source = source, .kernel = kernel, .destination = destination, .end_row = source->rows};
    RowTask *tasks = NULL;
    if (count > 1) {
        if (count - 1 > SIZE_MAX / sizeof(*tasks)) {
            errno = EOVERFLOW;
            return -1;
        }
        tasks = calloc(count - 1, sizeof(*tasks));
        if (tasks == NULL) {
            return -1;
        }
    }
    size_t started = 0;
    int error = start_workers(tasks, count, &main_task, &started);
    if (error == 0) {
        run_rows(&main_task);
    }
    int result = join_workers(tasks, started);
    free(tasks);
    if (error != 0 || result != 0 || main_task.result != 0) {
        errno = error != 0 ? error : ERANGE;
        return -1;
    }
    return 0;
}

static int is_filter_input_valid(const Matrix *matrix,
                                 const Matrix *kernel,
                                 size_t iterations,
                                 size_t threads) {
    if (matrix->data == NULL || kernel->data == NULL) {
        return 0;
    }
    if (iterations == 0 || threads == 0) {
        return 0;
    }
    return kernel->rows > 0 && kernel->rows == kernel->cols && kernel->rows % 2 == 1;
}

int apply_filter(Matrix *matrix, const Matrix *kernel, size_t iterations, size_t threads) {
    if (!is_filter_input_valid(matrix, kernel, iterations, threads)) {
        errno = EINVAL;
        return -1;
    }
    Matrix scratch = {0};
    if (matrix_create(&scratch, matrix->rows, matrix->cols) == -1) {
        return -1;
    }
    for (size_t i = 0; i < iterations; ++i) {
        if (run_pass(matrix, kernel, &scratch, threads) == -1) {
            free(scratch.data);
            return -1;
        }
        double *previous = matrix->data;
        matrix->data = scratch.data;
        scratch.data = previous;
    }
    free(scratch.data);
    return 0;
}
