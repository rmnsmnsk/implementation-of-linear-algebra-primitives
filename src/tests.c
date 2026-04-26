#include "COO.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

void free_matrix(COO *matrix) {
  if (matrix) {
    free(matrix->rows_indices);
    free(matrix->coll_indices);
    free(matrix->values);
    free(matrix);
  }
}

COO *create_matrix(int nnz, int rows, int cols, int *row_indices,
                   int *col_indices, float *values) {
  COO *matrix = (COO *)malloc(sizeof(COO));
  if (!matrix)
    return NULL;

  matrix->nnz = nnz;
  matrix->rows = rows;
  matrix->columns = cols;
  matrix->rows_indices = (int *)malloc(sizeof(int) * nnz);
  matrix->coll_indices = (int *)malloc(sizeof(int) * nnz);
  matrix->values = (float *)malloc(sizeof(float) * nnz);

  if (!matrix->rows_indices || !matrix->coll_indices || !matrix->values) {
    free(matrix->rows_indices);
    free(matrix->coll_indices);
    free(matrix->values);
    free(matrix);
    return NULL;
  }

  for (int i = 0; i < nnz; i++) {
    matrix->rows_indices[i] = row_indices[i];
    matrix->coll_indices[i] = col_indices[i];
    matrix->values[i] = values[i];
  }

  return matrix;
}

bool matrices_equal(COO *m1, COO *m2) {
  if (m1->rows != m2->rows || m1->columns != m2->columns ||
      m1->nnz != m2->nnz) {
    return false;
  }

  sort_matrix(m1);
  sort_matrix(m2);

  for (int i = 0; i < m1->nnz; i++) {
    if (m1->rows_indices[i] != m2->rows_indices[i] ||
        m1->coll_indices[i] != m2->coll_indices[i] ||
        fabs(m1->values[i] - m2->values[i]) > 1e-5) {
      return false;
    }
  }
  return true;
}

void test_sort_matrix() {
  int row_indices[] = {2, 0, 1, 0};
  int col_indices[] = {1, 2, 0, 0};
  float values[] = {5.0, 1.0, 3.0, 2.0};

  COO *matrix = create_matrix(4, 3, 3, row_indices, col_indices, values);
  assert(matrix != NULL);

  COO *sorted = sort_matrix(matrix);
  assert(sorted != NULL);

  assert(sorted->rows_indices[0] == 0 && sorted->coll_indices[0] == 0 &&
         fabs(sorted->values[0] - 2.0) < 1e-5);
  assert(sorted->rows_indices[1] == 0 && sorted->coll_indices[1] == 2 &&
         fabs(sorted->values[1] - 1.0) < 1e-5);
  assert(sorted->rows_indices[2] == 1 && sorted->coll_indices[2] == 0 &&
         fabs(sorted->values[2] - 3.0) < 1e-5);
  assert(sorted->rows_indices[3] == 2 && sorted->coll_indices[3] == 1 &&
         fabs(sorted->values[3] - 5.0) < 1e-5);

  assert(sort_matrix(NULL) == NULL);

  free_matrix(matrix);
}

void test_is_line() {
  int row_indices[] = {0, 0, 0};
  int col_indices[] = {0, 1, 2};
  float values[] = {1.0, 2.0, 3.0};
  COO *row_vector = create_matrix(3, 1, 3, row_indices, col_indices, values);
  assert(is_line(row_vector) == true);

  int row_indices2[] = {0, 1, 2};
  int col_indices2[] = {0, 1, 2};
  float values2[] = {1.0, 2.0, 3.0};
  COO *matrix = create_matrix(3, 3, 3, row_indices2, col_indices2, values2);
  assert(is_line(matrix) == false);

  free_matrix(row_vector);
  free_matrix(matrix);
}

void test_make_table_vector() {
  int row_indices[] = {0, 2, 1};
  int col_indices[] = {0, 0, 0};
  float values[] = {1.0, 3.0, 2.0};
  COO *vector = create_matrix(3, 3, 1, row_indices, col_indices, values);

  float *table = make_table_vector(vector);
  assert(table != NULL);
  assert(fabs(table[0] - 1.0) < 1e-5);
  assert(fabs(table[1] - 2.0) < 1e-5);
  assert(fabs(table[2] - 3.0) < 1e-5);

  free(table);
  free_matrix(vector);
}

void test_multiplication_two_matrix() {
  int row_a[] = {0, 0, 1, 1};
  int col_a[] = {0, 2, 1, 2};
  float val_a[] = {1.0, 2.0, 3.0, 4.0};
  COO *A = create_matrix(4, 2, 3, row_a, col_a, val_a);

  int row_b[] = {0, 1, 2, 2};
  int col_b[] = {0, 1, 0, 1};
  float val_b[] = {5.0, 6.0, 7.0, 8.0};
  COO *B = create_matrix(4, 3, 2, row_b, col_b, val_b);

  COO *result = multiplication_two_matrix(A, B);

  assert(result != NULL);
  assert(result->rows == 2);
  assert(result->columns == 2);

  sort_matrix(result);
  assert(result->nnz == 4);

  float found_00 = 0, found_01 = 0, found_10 = 0, found_11 = 0;
  for (int i = 0; i < result->nnz; i++) {
    if (result->rows_indices[i] == 0 && result->coll_indices[i] == 0) {
      found_00 = result->values[i];
    }
    if (result->rows_indices[i] == 0 && result->coll_indices[i] == 1) {
      found_01 = result->values[i];
    }
    if (result->rows_indices[i] == 1 && result->coll_indices[i] == 0) {
      found_10 = result->values[i];
    }
    if (result->rows_indices[i] == 1 && result->coll_indices[i] == 1) {
      found_11 = result->values[i];
    }
  }

  assert(fabs(found_00 - 19.0) < 1e-5);
  assert(fabs(found_01 - 16.0) < 1e-5);
  assert(fabs(found_10 - 28.0) < 1e-5);
  assert(fabs(found_11 - 50.0) < 1e-5);

  free_matrix(A);
  free_matrix(B);
  free_matrix(result);
}

void test_multiplication_matrix_and_vector() {
  int row_a[] = {0, 0, 1, 1};
  int col_a[] = {0, 1, 0, 1};
  float val_a[] = {2.0, 3.0, 4.0, 5.0};
  COO *A = create_matrix(4, 2, 2, row_a, col_a, val_a);

  int row_v[] = {0, 1};
  int col_v[] = {0, 0};
  float val_v[] = {1.0, 2.0};
  COO *V = create_matrix(2, 2, 1, row_v, col_v, val_v);

  COO *result = multiplication_matrix_and_vector(A, V);
  assert(result != NULL);
  assert(result->rows == 2);
  assert(result->columns == 1);

  sort_matrix(result);
  assert(result->nnz == 2);

  assert(result->rows_indices[0] == 0 && result->coll_indices[0] == 0 &&
         fabs(result->values[0] - 8.0) < 1e-5);
  assert(result->rows_indices[1] == 1 && result->coll_indices[1] == 0 &&
         fabs(result->values[1] - 14.0) < 1e-5);

  free_matrix(A);
  free_matrix(V);
  free_matrix(result);
}

void test_multiplication_vector_and_matrix() {
  int row_v[] = {0, 0};
  int col_v[] = {0, 1};
  float val_v[] = {1.0, 2.0};
  COO *V = create_matrix(2, 1, 2, row_v, col_v, val_v);

  int row_a[] = {0, 0, 1, 1};
  int col_a[] = {0, 1, 0, 1};
  float val_a[] = {2.0, 3.0, 4.0, 5.0};
  COO *A = create_matrix(4, 2, 2, row_a, col_a, val_a);

  COO *result = multiplication_vector_and_matrix(V, A);
  assert(result != NULL);
  assert(result->rows == 1);
  assert(result->columns == 2);

  sort_matrix(result);
  assert(result->nnz == 2);

  assert(result->rows_indices[0] == 0 && result->coll_indices[0] == 0 &&
         fabs(result->values[0] - 10.0) < 1e-5);
  assert(result->rows_indices[1] == 0 && result->coll_indices[1] == 1 &&
         fabs(result->values[1] - 13.0) < 1e-5);

  free_matrix(V);
  free_matrix(A);
  free_matrix(result);
}

float square(float x) { return x * x; }

void test_coo_map() {
  int row_indices[] = {0, 1, 2};
  int col_indices[] = {0, 1, 2};
  float values[] = {2.0, 3.0, 4.0};
  COO *matrix = create_matrix(3, 3, 3, row_indices, col_indices, values);

  coo_map(matrix, square);

  assert(fabs(matrix->values[0] - 4.0) < 1e-5);
  assert(fabs(matrix->values[1] - 9.0) < 1e-5);
  assert(fabs(matrix->values[2] - 16.0) < 1e-5);

  free_matrix(matrix);
}

float add(float a, float b) { return a + b; }

float multiply(float a, float b) { return a * b; }

void test_coo_map2() {
  int row1[] = {0, 0, 1, 2};
  int col1[] = {0, 1, 1, 2};
  float val1[] = {1.0, 2.0, 3.0, 4.0};
  COO *A = create_matrix(4, 3, 3, row1, col1, val1);

  int row2[] = {0, 1, 1, 2};
  int col2[] = {0, 1, 2, 2};
  float val2[] = {5.0, 6.0, 7.0, 8.0};
  COO *B = create_matrix(4, 3, 3, row2, col2, val2);

  COO *result = coo_map2(A, B, add);
  assert(result != NULL);
  assert(result->rows == 3);
  assert(result->columns == 3);

  sort_matrix(result);

  assert(result->nnz == 5);
  assert(result->rows_indices[0] == 0 && result->coll_indices[0] == 0 &&
         fabs(result->values[0] - 6.0) < 1e-5);
  assert(result->rows_indices[1] == 0 && result->coll_indices[1] == 1 &&
         fabs(result->values[1] - 2.0) < 1e-5);
  assert(result->rows_indices[2] == 1 && result->coll_indices[2] == 1 &&
         fabs(result->values[2] - 9.0) < 1e-5);
  assert(result->rows_indices[3] == 1 && result->coll_indices[3] == 2 &&
         fabs(result->values[3] - 7.0) < 1e-5);
  assert(result->rows_indices[4] == 2 && result->coll_indices[4] == 2 &&
         fabs(result->values[4] - 12.0) < 1e-5);

  free_matrix(A);
  free_matrix(B);
  free_matrix(result);
}

int main() {
  test_sort_matrix();
  test_is_line();
  test_make_table_vector();
  test_multiplication_two_matrix();
  test_multiplication_matrix_and_vector();
  test_multiplication_vector_and_matrix();
  test_coo_map();
  test_coo_map2();

  printf("All tests passed\n");
  return 0;
}