#include "COO.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

COO* create_matrix_copy(COO* original)
{
    if (original == NULL)
        return NULL;

    COO* copy = malloc(sizeof(COO));
    if (copy == NULL)
        return NULL;

    copy->nnz = original->nnz;
    copy->rows = original->rows;
    copy->columns = original->columns;

    copy->rows_indices = malloc(sizeof(int) * original->nnz);
    copy->coll_indices = malloc(sizeof(int) * original->nnz);
    copy->values = malloc(sizeof(float) * original->nnz);

    if (copy->rows_indices == NULL || copy->coll_indices == NULL || copy->values == NULL) {
        free_matrix(copy);
        return NULL;
    }

    for (int i = 0; i < original->nnz; i++) {
        copy->rows_indices[i] = original->rows_indices[i];
        copy->coll_indices[i] = original->coll_indices[i];
        copy->values[i] = original->values[i];
    }

    return copy;
}

COO* sort_matrix(COO* matrix)
{
    if (matrix == NULL || matrix->nnz <= 0)
        return NULL;
    if (matrix->rows_indices == NULL || matrix->coll_indices == NULL || matrix->values == NULL)
        return NULL;

    int* indices = malloc(sizeof(int) * matrix->nnz);
    if (indices == NULL)
        return NULL;

    for (int i = 0; i < matrix->nnz; i++)
        indices[i] = i;

    for (int i = 0; i < matrix->nnz - 1; i++) {
        for (int j = i + 1; j < matrix->nnz; j++) {
            int id1 = indices[i];
            int id2 = indices[j];

            if (matrix->rows_indices[id1] > matrix->rows_indices[id2] || (matrix->rows_indices[id1] == matrix->rows_indices[id2] && matrix->coll_indices[id1] > matrix->coll_indices[id2])) {
                int temp = indices[i];
                indices[i] = indices[j];
                indices[j] = temp;
            }
        }
    }

    int* new_row = malloc(sizeof(int) * matrix->nnz);
    if (new_row == NULL) {
        free(indices);
        return NULL;
    }

    int* new_col = malloc(sizeof(int) * matrix->nnz);
    if (new_col == NULL) {
        free(indices);
        free(new_row);
        return NULL;
    }

    float* new_values = malloc(sizeof(float) * matrix->nnz);
    if (new_values == NULL) {
        free(indices);
        free(new_row);
        free(new_col);
        return NULL;
    }

    for (int i = 0; i < matrix->nnz; i++) {
        int new_idx = indices[i];
        new_row[i] = matrix->rows_indices[new_idx];
        new_col[i] = matrix->coll_indices[new_idx];
        new_values[i] = matrix->values[new_idx];
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

bool is_line(COO* matrix)
{
    return matrix->rows == 1 && matrix->columns > 1;
}

float* make_table_vector(COO* vector)
{
    if (vector == NULL)
        return NULL;

    int size = (vector->rows == 1) ? vector->columns : vector->rows;
    float* vector_values = calloc(size, sizeof(float));
    if (vector_values == NULL)
        return NULL;

    for (int i = 0; i < vector->nnz; ++i) {
        int idx = (vector->rows == 1) ? vector->coll_indices[i] : vector->rows_indices[i];
        if (idx >= 0 && idx < size)
            vector_values[idx] = vector->values[i];
    }
    return vector_values;
}

COO* multiplication_two_matrix(COO* first, COO* second)
{
    if (first == NULL || second == NULL || (first->columns != second->rows)) {
        return NULL;
    }
    if (first->rows_indices == NULL || first->coll_indices == NULL || first->values == NULL)
        return NULL;
    if (second->rows_indices == NULL || second->coll_indices == NULL || second->values == NULL)
        return NULL;

    first = sort_matrix(first);
    second = sort_matrix(second);
    if (first == NULL || second == NULL)
        return NULL;

    COO* result = (COO*)malloc(sizeof(COO));
    if (result == NULL)
        return NULL;

    result->rows = first->rows;
    result->columns = second->columns;

    int max_nnz = first->nnz * second->nnz;
    if (max_nnz > first->rows * second->columns)
        max_nnz = first->rows * second->columns;

    if (max_nnz > 10000000)
        max_nnz = 10000000;

    int* temp_rows = malloc(sizeof(int) * max_nnz);
    int* temp_cols = malloc(sizeof(int) * max_nnz);
    float* temp_vals = malloc(sizeof(float) * max_nnz);

    if (temp_rows == NULL || temp_cols == NULL || temp_vals == NULL) {
        free(temp_rows);
        free(temp_cols);
        free(temp_vals);
        free(result);
        return NULL;
    }

    int nnz = 0;

    for (int i = 0; i < first->nnz; i++) {
        for (int j = 0; j < second->nnz; j++) {
            if (first->coll_indices[i] == second->rows_indices[j]) {
                int row = first->rows_indices[i];
                int col = second->coll_indices[j];
                float val = first->values[i] * second->values[j];

                int found = -1;
                for (int k = 0; k < nnz; k++) {
                    if (temp_rows[k] == row && temp_cols[k] == col) {
                        found = k;
                        break;
                    }
                }

                if (found != -1) {
                    temp_vals[found] += val;
                } else if (nnz < max_nnz) {
                    temp_rows[nnz] = row;
                    temp_cols[nnz] = col;
                    temp_vals[nnz] = val;
                    nnz++;
                }
            }
        }
    }

    result->nnz = nnz;
    if (nnz == 0) {
        result->rows_indices = NULL;
        result->coll_indices = NULL;
        result->values = NULL;
    } else {
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
    }

    free(temp_rows);
    free(temp_cols);
    free(temp_vals);

    return result;
}

COO* multiplication_matrix_and_vector(COO* first, COO* second)
{
    if (first == NULL || second == NULL || is_line(second))
        return NULL;

    float* vector_values = make_table_vector(second);
    if (vector_values == NULL)
        return NULL;

    COO* result = (COO*)malloc(sizeof(COO));
    if (result == NULL) {
        free(vector_values);
        return NULL;
    }

    result->rows = first->rows;
    result->columns = 1;

    float* temp_vals = calloc(first->rows, sizeof(float));
    if (temp_vals == NULL) {
        free(result);
        free(vector_values);
        return NULL;
    }

    for (int i = 0; i < first->nnz; ++i) {
        int row = first->rows_indices[i];
        int col = first->coll_indices[i];
        if (row >= 0 && row < first->rows && col >= 0 && col < first->columns) {
            temp_vals[row] += first->values[i] * vector_values[col];
        }
    }

    int nnz = 0;
    for (int i = 0; i < first->rows; ++i) {
        if (fabsf(temp_vals[i]) > 1e-6f)
            nnz++;
    }

    result->nnz = nnz;
    if (nnz == 0) {
        result->rows_indices = NULL;
        result->coll_indices = NULL;
        result->values = NULL;
    } else {
        result->rows_indices = malloc(sizeof(int) * nnz);
        result->coll_indices = malloc(sizeof(int) * nnz);
        result->values = malloc(sizeof(float) * nnz);

        if (result->rows_indices == NULL || result->coll_indices == NULL || result->values == NULL) {
            free(result->rows_indices);
            free(result->coll_indices);
            free(result->values);
            free(result);
            free(temp_vals);
            free(vector_values);
            return NULL;
        }

        int idx = 0;
        for (int i = 0; i < first->rows; ++i) {
            if (fabsf(temp_vals[i]) > 1e-6f) {
                result->rows_indices[idx] = i;
                result->coll_indices[idx] = 0;
                result->values[idx] = temp_vals[i];
                idx++;
            }
        }
    }

    free(temp_vals);
    free(vector_values);
    return result;
}

COO* multiplication_vector_and_matrix(COO* first, COO* second)
{
    if (first == NULL || second == NULL || !is_line(first))
        return NULL;

    float* vector_values = make_table_vector(first);
    if (vector_values == NULL)
        return NULL;

    COO* result = (COO*)malloc(sizeof(COO));
    if (result == NULL) {
        free(vector_values);
        return NULL;
    }

    result->rows = 1;
    result->columns = second->columns;

    float* temp_vals = calloc(second->columns, sizeof(float));
    if (temp_vals == NULL) {
        free(result);
        free(vector_values);
        return NULL;
    }

    for (int i = 0; i < second->nnz; ++i) {
        int row = second->rows_indices[i];
        int col = second->coll_indices[i];
        if (row >= 0 && row < second->rows && col >= 0 && col < second->columns) {
            temp_vals[col] += second->values[i] * vector_values[row];
        }
    }

    int nnz = 0;
    for (int i = 0; i < second->columns; ++i) {
        if (fabsf(temp_vals[i]) > 1e-6f)
            nnz++;
    }

    result->nnz = nnz;
    if (nnz == 0) {
        result->rows_indices = NULL;
        result->coll_indices = NULL;
        result->values = NULL;
    } else {
        result->rows_indices = malloc(sizeof(int) * nnz);
        result->coll_indices = malloc(sizeof(int) * nnz);
        result->values = malloc(sizeof(float) * nnz);

        if (result->rows_indices == NULL || result->coll_indices == NULL || result->values == NULL) {
            free(result->rows_indices);
            free(result->coll_indices);
            free(result->values);
            free(result);
            free(temp_vals);
            free(vector_values);
            return NULL;
        }

        int idx = 0;
        for (int i = 0; i < second->columns; ++i) {
            if (fabsf(temp_vals[i]) > 1e-6f) {
                result->rows_indices[idx] = 0;
                result->coll_indices[idx] = i;
                result->values[idx] = temp_vals[i];
                idx++;
            }
        }
    }

    free(temp_vals);
    free(vector_values);
    return result;
}

void coo_map(COO* mat, float (*func)(float))
{
    if (mat == NULL || func == NULL || mat->nnz == 0) {
        return;
    }
    for (int i = 0; i < mat->nnz; ++i) {
        mat->values[i] = func(mat->values[i]);
    }
}

COO* coo_map2(COO* first, COO* second, float (*func)(float, float))
{
    if (first == NULL || second == NULL)
        return NULL;
    if (first->rows != second->rows || first->columns != second->columns)
        return NULL;

    first = sort_matrix(first);
    second = sort_matrix(second);
    if (first == NULL || second == NULL)
        return NULL;

    int max = first->nnz + second->nnz;
    COO* result = malloc(sizeof(COO));
    if (result == NULL)
        return NULL;

    int* result_row_indices = malloc(sizeof(int) * max);
    int* result_coll_indices = malloc(sizeof(int) * max);
    float* result_values = malloc(sizeof(float) * max);

    if (result_row_indices == NULL || result_coll_indices == NULL || result_values == NULL) {
        free(result_row_indices);
        free(result_coll_indices);
        free(result_values);
        free(result);
        return NULL;
    }

    int count = 0;
    int i = 0, j = 0;

    while (i < first->nnz && j < second->nnz) {
        if (first->rows_indices[i] == second->rows_indices[j] && first->coll_indices[i] == second->coll_indices[j]) {
            float value = func(first->values[i], second->values[j]);
            if (fabsf(value) > 1e-6f) {
                result_values[count] = value;
                result_row_indices[count] = first->rows_indices[i];
                result_coll_indices[count] = first->coll_indices[i];
                count++;
            }
            i++;
            j++;
        } else if (first->rows_indices[i] < second->rows_indices[j] || (first->rows_indices[i] == second->rows_indices[j] && first->coll_indices[i] < second->coll_indices[j])) {
            float value = func(first->values[i], 0.0f);
            if (fabsf(value) > 1e-6f) {
                result_values[count] = value;
                result_row_indices[count] = first->rows_indices[i];
                result_coll_indices[count] = first->coll_indices[i];
                count++;
            }
            i++;
        } else {
            float value = func(0.0f, second->values[j]);
            if (fabsf(value) > 1e-6f) {
                result_values[count] = value;
                result_row_indices[count] = second->rows_indices[j];
                result_coll_indices[count] = second->coll_indices[j];
                count++;
            }
            j++;
        }
    }

    while (i < first->nnz) {
        float val = func(first->values[i], 0.0f);
        if (fabsf(val) > 1e-6f) {
            result_values[count] = val;
            result_row_indices[count] = first->rows_indices[i];
            result_coll_indices[count] = first->coll_indices[i];
            count++;
        }
        i++;
    }

    while (j < second->nnz) {
        float val = func(0.0f, second->values[j]);
        if (fabsf(val) > 1e-6f) {
            result_values[count] = val;
            result_row_indices[count] = second->rows_indices[j];
            result_coll_indices[count] = second->coll_indices[j];
            count++;
        }
        j++;
    }

    result->rows = first->rows;
    result->columns = first->columns;
    result->nnz = count;

    if (count == 0) {
        free(result_row_indices);
        free(result_coll_indices);
        free(result_values);
        result->rows_indices = NULL;
        result->coll_indices = NULL;
        result->values = NULL;
    } else {
        result->rows_indices = realloc(result_row_indices, count * sizeof(int));
        result->coll_indices = realloc(result_coll_indices, count * sizeof(int));
        result->values = realloc(result_values, count * sizeof(float));

        if (result->rows_indices == NULL || result->coll_indices == NULL || result->values == NULL) {
            free(result->rows_indices);
            free(result->coll_indices);
            free(result->values);
            free(result);
            return NULL;
        }
    }

    return result;
}

void free_matrix(COO* matrix)
{
    if (matrix == NULL)
        return;
    free(matrix->rows_indices);
    free(matrix->coll_indices);
    free(matrix->values);
    free(matrix);
}
