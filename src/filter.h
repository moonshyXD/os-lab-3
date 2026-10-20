#ifndef FILTER_H
#define FILTER_H

#include "matrix.h"

int apply_filter(Matrix *matrix, const Matrix *kernel, size_t iterations, size_t threads);

#endif
