#include "matrix.h"
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

int matrix_create(Matrix *matrix, size_t rows, size_t cols) {
    if (rows == 0 || cols == 0) {
        errno = EINVAL;
        return -1;
    }
    if (cols > SIZE_MAX / rows || rows * cols > PTRDIFF_MAX / sizeof(double)) {
        errno = EOVERFLOW;
        return -1;
    }
    double *data = malloc(rows * cols * sizeof(*data));
    if (data == NULL) {
        return -1;
    }
    *matrix = (Matrix){rows, cols, data};
    return 0;
}

int convolve_rows(const Matrix *source,
                  const Matrix *kernel,
                  Matrix *destination,
                  size_t first_row,
                  size_t end_row) {
    ptrdiff_t center = (ptrdiff_t)(kernel->rows / 2);
    for (size_t row = first_row; row < end_row; ++row) {
        for (size_t col = 0; col < source->cols; ++col) {
            double sum = 0.0;
            for (size_t u = 0; u < kernel->rows; ++u) {
                ptrdiff_t y = (ptrdiff_t)row + center - (ptrdiff_t)u;
                if (y < 0 || y >= (ptrdiff_t)source->rows) {
                    continue;
                }
                for (size_t v = 0; v < kernel->cols; ++v) {
                    ptrdiff_t x = (ptrdiff_t)col + center - (ptrdiff_t)v;
                    if (x < 0 || x >= (ptrdiff_t)source->cols) {
                        continue;
                    }
                    sum += source->data[(size_t)y * source->cols + (size_t)x] *
                           kernel->data[u * kernel->cols + v];
                }
            }
            if (!isfinite(sum)) {
                return -1;
            }
            destination->data[row * source->cols + col] = sum;
        }
    }
    return 0;
}
