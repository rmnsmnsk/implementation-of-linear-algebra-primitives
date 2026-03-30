#include "COO.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include<math.h>

COO *sort_matrix(COO *matrix) {
  if (matrix == NULL) {
    return NULL;
  }
  if (matrix->rows_indices == NULL || matrix->coll_indices == NULL ||
      matrix->values == NULL) {
    return NULL;
  }
  int *indices = malloc(sizeof(int) * matrix->nnz);
  if (indices == NULL) {
    return NULL;
  }
  for (int d = 0; d < matrix->nnz; d++) {
    indices[d] = d;
  }

  for (int i = 0; i < matrix->nnz - 1; i++) {
    for (int j = i + 1; j < matrix->nnz; j++) {
      int id_1 = indices[i];
      int id_2 = indices[j];

      if (matrix->rows_indices[id_1] > matrix->rows_indices[id_2]) {
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
      } else if (matrix->rows_indices[id_1] == matrix->rows_indices[id_2] &&
                 matrix->coll_indices[id_1] > matrix->coll_indices[id_2]) {
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
      }
    }
  }

  int *new_row = malloc(sizeof(int) * matrix->nnz);
  if (new_row == NULL) {
    free(indices);
    return NULL;
  }
  int *new_col = malloc(sizeof(int) * matrix->nnz);
  if (new_col == NULL) {
    free(indices);
    free(new_row);
    return NULL;
  }
  float *new_values = malloc(sizeof(float) * matrix->nnz);
  if (new_values == NULL) {
    free(indices);
    free(new_row);
    free(new_col);
    return NULL;
  }

  for (int y = 0; y < matrix->nnz; y++) {
    int new_index = indices[y];
    new_row[y] = matrix->rows_indices[new_index];
    new_col[y] = matrix->coll_indices[new_index];
    new_values[y] = matrix->values[new_index];
  }

  free(matrix->rows_indices);
  free(matrix->coll_indices);
  free(matrix->values);
  free(indices);

  matrix->rows_indices = new_row;
  matrix->coll_indices = new_col;
  matrix->values = new_values;

  return matrix;
}

bool is_line(COO *matrix) {
  return matrix->rows == 1;
}

float *make_table_vector(COO *vector) {
  if (vector == NULL) {
    return NULL;
  }
  if (vector->rows_indices == NULL || vector->coll_indices == NULL || vector->values == NULL) return NULL;

  float *vector_values = calloc(vector->rows, sizeof(float));
  for (int i = 0; i < vector->nnz; ++i) {
    vector_values[vector->rows_indices[i]] = vector->values[i];
  }
  return vector_values;
}

COO *multiplication_two_matrix(COO *first, COO *second) {
  if (first == NULL || second == NULL || (first->columns != second->rows)) {
    return NULL;
  }
  if (first->rows_indices == NULL || first->coll_indices == NULL || first->values == NULL) return NULL;
  if (second->rows_indices == NULL || second->coll_indices == NULL || second->values == NULL) return NULL;

  COO *result = (COO *)malloc(sizeof(COO));
  if (result == NULL) return NULL;
  result->rows = first->rows;
  result->columns = second->columns;

  int max_nnz = first->rows * second->columns;
  if (max_nnz > first->nnz * second->nnz) {
    max_nnz = first->nnz * second->nnz;
  }

  int *temp_rows = malloc(sizeof(int) * max_nnz);
  int *temp_cols = malloc(sizeof(int) * max_nnz);
  float *temp_vals = malloc(sizeof(float) * max_nnz);
  if (temp_rows == NULL || temp_cols == NULL || temp_vals == NULL) {
    free(result);
    free(temp_rows);
    free(temp_cols);
    free(temp_vals);
    return NULL;
  }

  int nnz = 0;

  first = sort_matrix(first);
  second = sort_matrix(second);
  if (first == NULL || second == NULL) {
    free(result);
    free(temp_rows);
    free(temp_cols);
    free(temp_vals);
    return NULL;
  }

  for (int i = 0; i < first->nnz; i++) {
    for (int j = 0; j < second->nnz; j++) {
      if (first->coll_indices[i] > second->rows_indices[j]) {
        continue;
      }
      if (first->coll_indices[i] == second->rows_indices[j]) {
        int row = first->rows_indices[i];
        int col = second->coll_indices[j];
        float val = first->values[i] * second->values[j];

        int found = -1;
        for (int r = 0; r < nnz; r++) {
          if (temp_rows[r] == row && temp_cols[r] == col) {
            found = r;
            break;
          }
        }

        if (found != -1) {
          temp_vals[found] += val;
        } else {
          temp_rows[nnz] = row;
          temp_cols[nnz] = col;
          temp_vals[nnz] = val;
          nnz++;
        }
      }
    }
  }

  result->nnz = nnz;
  result->rows_indices = malloc(sizeof(int) * nnz);
  result->coll_indices = malloc(sizeof(int) * nnz);
  result->values = malloc(sizeof(float) * nnz);
  if (result->rows_indices == NULL || result->coll_indices == NULL || result->values == NULL) {
    free(result->rows_indices);
    free(result->coll_indices);
    free(result->values);
    free(result);
    free(temp_rows);
    free(temp_cols);
    free(temp_vals);
    return NULL;
  }

  for (int i = 0; i < nnz; i++) {
    result->rows_indices[i] = temp_rows[i];
    result->coll_indices[i] = temp_cols[i];
    result->values[i] = temp_vals[i];
  }

  free(temp_rows);
  free(temp_cols);
  free(temp_vals);

  return result;
}

COO *multiplication_matrix_and_vector(COO *first, COO *second) {
  if (first == NULL || second == NULL || is_line(second)) return NULL;

  float *vector_values = make_table_vector(second);

  COO *result = (COO *)malloc(sizeof(COO));
  if (result == NULL) return NULL;
  result->rows = first->rows;
  result->columns = 1;

  float *temp_vals = calloc(first->rows, sizeof(float));
  if (temp_vals == NULL) {
    free(result);
    return NULL;
  }

  for (int i = 0; i < first->nnz; ++i) {
    temp_vals[first->rows_indices[i]] += first->values[i] * vector_values[first->coll_indices[i]];
  }

  int nnz = 0;
  for (int i = 0; i < first->rows; ++i) {
    if (temp_vals[i] != 0) nnz++;
  }

  result->nnz = nnz;
  result->rows_indices = malloc(sizeof(int) * nnz);
  result->coll_indices = malloc(sizeof(int) * nnz);
  result->values = malloc(sizeof(float) * nnz);
  if (result->rows_indices == NULL || result->coll_indices == NULL || result->values == NULL) {
    free(result->rows_indices);
    free(result->coll_indices);
    free(result->values);
    free(result);
    free(temp_vals);
    return NULL;
  }

  int idx = 0;
  for (int i = 0; i < first->rows; ++i) {
    if (temp_vals[i] != 0) {
      result->rows_indices[idx] = i;
      result->coll_indices[idx] = 0;
      result->values[idx] = temp_vals[i];
      idx++;
    }
  }

  free(temp_vals);
  return result;
}

COO *multiplication_vector_and_matrix(COO *first, COO *second) {
  if (first == NULL || second == NULL || !is_line(first)) return NULL;

  float *vector_values = make_table_vector(first);

  COO *result = (COO *)malloc(sizeof(COO));
  if (result == NULL) return NULL;
  result->rows = 1;
  result->columns = second->columns;

  float *temp_vals = calloc(second->columns, sizeof(float));
  if (temp_vals == NULL) {
    free(result);
    return NULL;
  }

  for (int i = 0; i < second->nnz; ++i) {
    temp_vals[second->coll_indices[i]] += second->values[i] * vector_values[second->rows_indices[i]];
  }

  int nnz = 0;
  for (int i = 0; i < second->columns; ++i) {
    if (temp_vals[i] != 0){
      nnz++;
    }
  }

  result->nnz = nnz;
  result->rows_indices = malloc(sizeof(int) * nnz);
  result->coll_indices = malloc(sizeof(int) * nnz);
  result->values = malloc(sizeof(float) * nnz);
  if (result->rows_indices == NULL || result->coll_indices == NULL || result->values == NULL) {
    free(result->rows_indices);
    free(result->coll_indices);
    free(result->values);
    free(result);
    free(temp_vals);
    return NULL;
  }

  int idx = 0;
  for (int i = 0; i < second->columns; ++i) {
    if (temp_vals[i] != 0) {
      result->rows_indices[idx] = 0;
      result->coll_indices[idx] = i;
      result->values[idx] = temp_vals[i];
      idx++;
    }
  }

  free(temp_vals);
  return result;
}

void coo_map(COO* mat, float (*func)(float)){
  if (mat == NULL || func == NULL || mat -> nnz == 0){
    return;
  }
  for (int i = 0; i < mat->nnz; ++i){
    mat->values[i] = func(mat -> values[i]);
  }
  return;
}

COO* coo_map2(COO* first, COO* second, float (*func)(float, float)){
    first = sort_matrix(first);
    second = sort_matrix(second);
    int max = first -> nnz + second -> nnz;
    COO* result = malloc(sizeof(COO));
    if (result == NULL || (first -> rows != second -> rows) || (first -> columns != second -> columns)){
      return NULL;
    }
    int* result_row_indices = malloc(sizeof(int) * max);
    if (result_row_indices ==   NULL){
      free(result);
      return NULL;
    }
    int* result_coll_indices = malloc(sizeof(int) * max);
    if (result_coll_indices ==  NULL){
      free(result);
      free(result_row_indices);
      return NULL;
    }
    float* result_values = malloc(sizeof(float) * max);
    if (result_values == NULL){
      free(result);
      free(result_row_indices);
      free(result_coll_indices);
      return NULL;
    }
    int count = 0;
    int i = 0;
    int j = 0;
    while (i < first -> nnz && j < second -> nnz){
      if ((first -> rows_indices[i] == second -> rows_indices[j]) && (first -> coll_indices[i] == second -> coll_indices[j])){
        float value = func(first->values[i], second->values[j]);
        if (value != 0.0f && !isnan(value) && !isinf(value)){
          result_values[count] = value;
          result_row_indices[count] = first -> rows_indices[i];
          result_coll_indices[count] = first -> coll_indices[i];
          count++;
        }
        i++;
        j++;
      }
      else if (first->rows_indices[i] < second->rows_indices[j] || (first->rows_indices[i] == second->rows_indices[j] && first->coll_indices[i] < second->coll_indices[j])){
        float value = func(first -> values[i], 0.0f);
        if (value != 0.0f && !isnan(value) && !isinf(value)){
          result_values[count] = value;
          result_row_indices[count] = first -> rows_indices[i];
          result_coll_indices[count] = first -> coll_indices[i];
          count++;
        }
        i++;
      }
      else{
        float value = func(0.0f, second -> values[j]);
        if (value != 0.0f && !isnan(value) && !isinf(value)){
          result_values[count] = value;
          result_row_indices[count] = second -> rows_indices[j];
          result_coll_indices[count] = second -> coll_indices[j];
          count++;
        }
        j++;
      }
    }
    while (i < first->nnz) {
      float val = func(first->values[i], 0.0f);
      if (val != 0.0f && !isnan(val) && !isinf(val)){
        result_values[count] = val;
        result_row_indices[count] = first -> rows_indices[i];
        result_coll_indices[count] = first -> coll_indices[i];
        count++;
      }
      i++;
    }
    while (j < second->nnz) {
      float val = func(0.0f, second->values[j]);
      if (val != 0.0f && !isnan(val) && !isinf(val)){
        result_values[count] = val;
        result_row_indices[count] = second -> rows_indices[j];
        result_coll_indices[count] = second -> coll_indices[j];
        count++;
      }
      j++;
    }
    result_row_indices = realloc(result_row_indices, count * sizeof(int));
    result_coll_indices = realloc(result_coll_indices, count * sizeof(int));
    result_values = realloc(result_values, count * sizeof(float));
    result -> rows_indices = result_row_indices;
    result -> coll_indices = result_coll_indices;
    result -> values = result_values;
    result -> columns = first -> columns;
    result -> rows = first -> rows;
    result -> nnz = count;
    return result;
}