#ifndef CONSOLE_H
#define CONSOLE_H

#include "matrix.h"

int parse_options(int argc, char **argv, size_t *threads);
int read_problem(Matrix *matrix, Matrix *kernel, size_t *iterations);
int print_matrix(const Matrix *matrix);

#endif
