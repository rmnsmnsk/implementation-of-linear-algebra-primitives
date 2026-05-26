#include "COO.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "matrix_market.h"

static inline double get_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

COO *generate_random_coo(int rows, int cols, float density, unsigned seed) {
  srand(seed);
  int nnz = (int)((float)rows * (float)cols * density);

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
  if (C)
    free_matrix(C);

  double total = 0.0;
  for (int i = 0; i < iterations; i++) {
    double t0 = get_time();
    COO *res = multiplication_two_matrix(A, B);
    double t1 = get_time();
    total += (t1 - t0);
    if (res)
      free_matrix(res);
  }

  free_matrix(A);
  free_matrix(B);
  return total / iterations;
}

double benchmark_mat_vec(int rows, int cols, float density, int iterations) {
  COO *A = generate_random_coo(rows, cols, density, 42);
  COO *v = generate_random_coo(cols, 1, 1.0f, 43);

  COO *r = multiplication_matrix_and_vector(A, v);
  if (r)
    free_matrix(r);

  double total = 0.0;
  for (int i = 0; i < iterations; i++) {
    double t0 = get_time();
    COO *res = multiplication_matrix_and_vector(A, v);
    double t1 = get_time();
    total += (t1 - t0);
    if (res)
      free_matrix(res);
  }

  free_matrix(A);
  free_matrix(v);
  return total / iterations;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <size> <density> <op> OR %s <file.mtx> <op> --mtx\n", argv[0], argv[0]);
        return 1;
    }

    double result = 0.0;
    const char *op = NULL;

    if (argc == 4 && strcmp(argv[3], "--mtx") == 0) {
        op = argv[2];
        COO *A = load_matrix_market(argv[1]);
        if (!A) return 1;
        printf("Loaded %s: %dx%d, nnz=%d\n", argv[1], A->rows, A->columns, A->nnz);

        if (strcmp(op, "mm") == 0) {
            COO *B = generate_random_coo(A->columns, A->rows, 0.05f, 42);
            double t0 = get_time();
            COO *C = multiplication_two_matrix(A, B);
            double t1 = get_time();
            result = t1 - t0;
            if (C) free_matrix(C);
            free_matrix(B);
        } else if (strcmp(op, "mv") == 0) {
            COO *v = generate_random_coo(A->columns, 1, 1.0f, 42);
            double t0 = get_time();
            COO *r = multiplication_matrix_and_vector(A, v);
            double t1 = get_time();
            result = t1 - t0;
            if (r) free_matrix(r);
            free_matrix(v);
        } else {
            fprintf(stderr, "Unknown op: %s\n", op);
            free_matrix(A); return 1;
        }
        free_matrix(A);
    } else {
        int size = atoi(argv[1]);
        float density = (float)atof(argv[2]);
        op = argv[3];
        if (strcmp(op, "mm") == 0) result = benchmark_multiply(size, size, density, 5);
        else if (strcmp(op, "mv") == 0) result = benchmark_mat_vec(size, size, density, 5);
        else { fprintf(stderr, "Unknown op: %s\n", op); return 1; }
    }

    printf("%.6f\n", result);
    return 0;
}