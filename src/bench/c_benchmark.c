#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "../c_impl/COO.h"

static inline double get_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

COO *generate_random_coo(int rows, int cols, float density, unsigned seed) {
    srand(seed);
    int nnz = (int)(rows * cols * density);
    COO *mat = malloc(sizeof(COO));
    mat->rows = rows;
    mat->columns = cols;
    mat->nnz = nnz;
    mat->rows_indices = malloc(sizeof(int) * nnz);
    mat->coll_indices = malloc(sizeof(int) * nnz);
    mat->values = malloc(sizeof(float) * nnz);
    for (int i = 0; i < nnz; i++) {
        mat->rows_indices[i] = rand() % rows;
        mat->coll_indices[i] = rand() % cols;
        mat->values[i] = (float)rand() / RAND_MAX * 2.0f - 1.0f;
    }
    return mat;
}

double benchmark_multiply(int rows, int cols, float density, int iterations) {
    COO *A = generate_random_coo(rows, cols, density, 42);
    COO *B = generate_random_coo(cols, rows, density, 43);

    COO *C = multiplication_two_matrix(A, B);
    if (C) free_matrix(C);

    double total = 0.0;
    for (int i = 0; i < iterations; i++) {
        double t0 = get_time();
        COO *res = multiplication_two_matrix(A, B);
        double t1 = get_time();
        total += (t1 - t0);
        if (res) free_matrix(res);
    }

    free_matrix(A);
    free_matrix(B);
    return total / iterations;
}

double benchmark_mat_vec(int rows, int cols, float density, int iterations) {
    COO *A = generate_random_coo(rows, cols, density, 42);
    COO *v = generate_random_coo(cols, 1, 1.0f, 43);

    COO *r = multiplication_matrix_and_vector(A, v);
    if (r) free_matrix(r);

    double total = 0.0;
    for (int i = 0; i < iterations; i++) {
        double t0 = get_time();
        COO *res = multiplication_matrix_and_vector(A, v);
        double t1 = get_time();
        total += (t1 - t0);
        if (res) free_matrix(res);
    }

    free_matrix(A);
    free_matrix(v);
    return total / iterations;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <size> <density> <op>\n", argv[0]);
        return 1;
    }

    int size = atoi(argv[1]);
    float density = atof(argv[2]);
    const char *op = argv[3];

    double result;
    if (strcmp(op, "mm") == 0) {
        result = benchmark_multiply(size, size, density, 5);
    } else if (strcmp(op, "mv") == 0) {
        result = benchmark_mat_vec(size, size, density, 5);
    } else {
        fprintf(stderr, "Unknown operation: %s\n", op);
        return 1;
    }

    printf("%.6f\n", result);
    return 0;
}