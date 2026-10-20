#include "console.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char *line;
    size_t capacity;
    int interactive;
} Reader;

static void skip_space(char **cursor) {
    while (isspace((unsigned char)**cursor)) {
        ++*cursor;
    }
}

static int parse_size(char **cursor, size_t *value) {
    skip_space(cursor);
    if (!isdigit((unsigned char)**cursor)) {
        return -1;
    }
    errno = 0;
    char *end;
    uintmax_t parsed = strtoumax(*cursor, &end, 10);
    if (errno != 0 || parsed == 0 || parsed > SIZE_MAX ||
        (*end != '\0' && !isspace((unsigned char)*end))) {
        return -1;
    }
    *cursor = end;
    *value = (size_t)parsed;
    return 0;
}

static int at_end(char *cursor) {
    skip_space(&cursor);
    return *cursor == '\0';
}

static int read_line(Reader *reader, const char *prompt) {
    if (reader->interactive) {
        fputs(prompt, stderr);
        fflush(stderr);
    }
    if (getline(&reader->line, &reader->capacity, stdin) >= 0) {
        return 0;
    }
    if (ferror(stdin)) {
        perror("Чтение ввода");
    } else {
        fputs("Ввод закончился раньше, чем получены все данные.\n", stderr);
    }
    return -1;
}

static int read_dimensions(Reader *reader, size_t *rows, size_t *cols) {
    do {
        if (read_line(reader, "Число строк и столбцов (например, 2 3): ") == -1) {
            return -1;
        }
        char *cursor = reader->line;
        if (parse_size(&cursor, rows) == 0 && parse_size(&cursor, cols) == 0 && at_end(cursor)) {
            return 0;
        }
        fputs("Нужны два положительных целых числа через пробел.\n", stderr);
    } while (reader->interactive);
    return -1;
}

static int read_size(Reader *reader, const char *prompt, size_t *value, int odd) {
    do {
        if (read_line(reader, prompt) == -1) {
            return -1;
        }
        char *cursor = reader->line;
        if (parse_size(&cursor, value) == 0 && at_end(cursor) && (!odd || *value % 2 == 1)) {
            return 0;
        }
        fputs(odd ? "Нужно положительное нечётное целое число.\n"
                  : "Нужно положительное целое число.\n",
              stderr);
    } while (reader->interactive);
    return -1;
}

static int parse_row(char *cursor, double *values, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        skip_space(&cursor);
        errno = 0;
        char *end;
        double value = strtod(cursor, &end);
        if (end == cursor || !isfinite(value) ||
            (errno != 0 && (errno != ERANGE || value == 0.0)) ||
            (*end != '\0' && !isspace((unsigned char)*end))) {
            return -1;
        }
        values[i] = value;
        cursor = end;
    }
    return at_end(cursor) ? 0 : -1;
}

static int read_matrix(Reader *reader, Matrix *matrix, const char *name) {
    for (size_t row = 0; row < matrix->rows; ++row) {
        for (;;) {
            if (reader->interactive) {
                fprintf(stderr,
                        "%s, строка %zu/%zu (%zu чисел): ",
                        name,
                        row + 1,
                        matrix->rows,
                        matrix->cols);
            }
            if (read_line(reader, "") == -1) {
                return -1;
            }
            if (parse_row(reader->line, matrix->data + row * matrix->cols, matrix->cols) == 0) {
                break;
            }
            fprintf(stderr,
                    "Введите ровно %zu конечных чисел через пробел; "
                    "дробная часть через точку. Повторите строку.\n",
                    matrix->cols);
            if (!reader->interactive) {
                return -1;
            }
        }
    }
    return 0;
}

int parse_options(int argc, char **argv, size_t *threads) {
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        puts("Свёртка матрицы, лабораторная работа 3, вариант 13.\n"
             "Запуск: ./convolution [--threads P]\n"
             "P — максимум потоков вместе с главным; по умолчанию 1.\n"
             "Ввод по строкам:\n"
             "  строки столбцы\n"
             "  значения каждой строки матрицы\n"
             "  нечётный размер квадратного ядра\n"
             "  значения каждой строки ядра\n"
             "  число проходов K\n"
             "Размеры и K положительные; числа конечные, дробная часть через точку.\n"
             "За границами матрицы нули. Ядро переворачивается (свёртка).\n"
             "Можно перенаправить ввод: ./convolution --threads 4 < input.txt");
        return 1;
    }
    *threads = 1;
    if (argc == 1) {
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "--threads") == 0) {
        char *cursor = argv[2];
        if (parse_size(&cursor, threads) == 0 && at_end(cursor)) {
            return 0;
        }
    }
    fputs("Неверные параметры. Используйте ./convolution --help.\n", stderr);
    return -1;
}

static int read_input(Reader *reader, Matrix *matrix, Matrix *kernel, size_t *iterations) {
    size_t rows, cols, size;
    if (read_dimensions(reader, &rows, &cols) == -1) {
        return -1;
    }
    if (matrix_create(matrix, rows, cols) == -1) {
        perror("Выделение матрицы");
        return -1;
    }
    if (read_matrix(reader, matrix, "Матрица") == -1) {
        return -1;
    }
    if (read_size(reader, "Размер ядра (нечётное число, например 3): ", &size, 1) == -1) {
        return -1;
    }
    if (matrix_create(kernel, size, size) == -1) {
        perror("Выделение ядра");
        return -1;
    }
    if (read_matrix(reader, kernel, "Ядро") == -1) {
        return -1;
    }
    return read_size(reader, "Количество проходов K: ", iterations, 0);
}

int read_problem(Matrix *matrix, Matrix *kernel, size_t *iterations) {
    Reader reader = {.interactive = isatty(STDIN_FILENO)};
    if (reader.interactive) {
        fputs("Введите матрицу и ядро построчно. Числа разделяйте пробелами, "
              "дробную часть — точкой.\n",
              stderr);
    }
    int result = read_input(&reader, matrix, kernel, iterations);
    free(reader.line);
    return result;
}

int print_matrix(const Matrix *matrix) {
    if (isatty(STDIN_FILENO)) {
        fputs("Результат:\n", stderr);
    }
    for (size_t row = 0; row < matrix->rows; ++row) {
        for (size_t col = 0; col < matrix->cols; ++col) {
            if (printf("%s%.17g", col == 0 ? "" : " ", matrix->data[row * matrix->cols + col]) <
                0) {
                return -1;
            }
        }
        if (putchar('\n') == EOF) {
            return -1;
        }
    }
    return fflush(stdout) == EOF ? -1 : 0;
}
