#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>

typedef struct {
    size_t rows;
    size_t cols;
    double *data;
} Matrix;

int matrix_create(Matrix *matrix, size_t rows, size_t cols);
int convolve_rows(const Matrix *source,
                  const Matrix *kernel,
                  Matrix *destination,
                  size_t first_row,
                  size_t end_row);

#endif
