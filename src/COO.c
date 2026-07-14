#include "COO.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int row;
    int col;
    float val;
} COO_Element;

static int compare_coo_elements(const void* a, const void* b)
{
    const COO_Element* elem1 = (const COO_Element*)a;
    const COO_Element* elem2 = (const COO_Element*)b;
    if (elem1->row != elem2->row) {
        return elem1->row - elem2->row;
    }
    return elem1->col - elem2->col;
}

COO* sort_matrix(COO* matrix)
{
    if (matrix == NULL || matrix->nnz <= 0)
        return NULL;
    if (matrix->rows_indices == NULL || matrix->coll_indices == NULL || matrix->values == NULL)
        return NULL;

    COO_Element* elements = malloc(sizeof(COO_Element) * matrix->nnz);
    if (elements == NULL)
        return NULL;

    for (int i = 0; i < matrix->nnz; i++) {
        elements[i].row = matrix->rows_indices[i];
        elements[i].col = matrix->coll_indices[i];
        elements[i].val = matrix->values[i];
    }

    qsort(elements, matrix->nnz, sizeof(COO_Element), compare_coo_elements);

    for (int i = 0; i < matrix->nnz; i++) {
        matrix->rows_indices[i] = elements[i].row;
        matrix->coll_indices[i] = elements[i].col;
        matrix->values[i] = elements[i].val;
    }

    free(elements);
    return matrix;
}

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

    if (!copy->rows_indices || !copy->coll_indices || !copy->values) {
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

bool is_line(COO* matrix)
{
    return matrix && (matrix->rows == 1 && matrix->columns > 1);
}

float* make_table_vector(COO* vector)
{
    if (vector == NULL)
        return NULL;

    int size = vector->rows;
    float* result = calloc(size, sizeof(float));
    if (result == NULL)
        return NULL;

    for (int i = 0; i < vector->nnz; i++) {
        if (vector->rows_indices[i] < size) {
            result[vector->rows_indices[i]] = vector->values[i];
        }
    }
    return result;
}

COO* multiplication_two_matrix(COO* first, COO* second)
{
    if (!first || !second || first->columns != second->rows)
        return NULL;
    if (!first->rows_indices || !second->rows_indices)
        return NULL;

    first = sort_matrix(first);
    second = sort_matrix(second);
    if (!first || !second)
        return NULL;

    int* row_start = calloc(first->columns + 1, sizeof(int));
    if (!row_start)
        return NULL;

    for (int i = 0; i < second->nnz; i++) {
        row_start[second->rows_indices[i] + 1]++;
    }
    for (int i = 0; i < first->columns; i++) {
        row_start[i + 1] += row_start[i];
    }

    COO* result = malloc(sizeof(COO));
    if (!result) {
        free(row_start);
        return NULL;
    }

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
    float* temp_row_buf = calloc(second->columns, sizeof(float));

    if (!temp_rows || !temp_cols || !temp_vals || !temp_row_buf) {
        free(result);
        free(row_start);
        free(temp_rows);
        free(temp_cols);
        free(temp_vals);
        free(temp_row_buf);
        return NULL;
    }

    int res_nnz = 0;
    int current_row = -1;

    for (int i = 0; i < first->nnz; i++) {
        int r1 = first->rows_indices[i];
        int c1 = first->coll_indices[i];
        float v1 = first->values[i];

        if (r1 != current_row) {
            if (current_row != -1) {
                for (int c = 0; c < second->columns; c++) {
                    if (fabsf(temp_row_buf[c]) > 1e-6f) {
                        if (res_nnz < max_nnz) {
                            temp_rows[res_nnz] = current_row;
                            temp_cols[res_nnz] = c;
                            temp_vals[res_nnz] = temp_row_buf[c];
                            res_nnz++;
                        }
                        temp_row_buf[c] = 0.0f;
                    }
                }
            }
            current_row = r1;
        }

        int start_j = row_start[c1];
        int end_j = row_start[c1 + 1];
        for (int j = start_j; j < end_j; j++) {
            temp_row_buf[second->coll_indices[j]] += v1 * second->values[j];
        }
    }

    if (current_row != -1) {
        for (int c = 0; c < second->columns; c++) {
            if (fabsf(temp_row_buf[c]) > 1e-6f) {
                if (res_nnz < max_nnz) {
                    temp_rows[res_nnz] = current_row;
                    temp_cols[res_nnz] = c;
                    temp_vals[res_nnz] = temp_row_buf[c];
                    res_nnz++;
                }
            }
        }
    }

    result->nnz = res_nnz;
    if (res_nnz == 0) {
        result->rows_indices = NULL;
        result->coll_indices = NULL;
        result->values = NULL;
    } else {
        result->rows_indices = malloc(sizeof(int) * res_nnz);
        result->coll_indices = malloc(sizeof(int) * res_nnz);
        result->values = malloc(sizeof(float) * res_nnz);
        if (!result->rows_indices || !result->coll_indices || !result->values) {
            free(result->rows_indices);
            free(result->coll_indices);
            free(result->values);
            free(result);
            free(row_start);
            free(temp_rows);
            free(temp_cols);
            free(temp_vals);
            free(temp_row_buf);
            return NULL;
        }
        for (int i = 0; i < res_nnz; i++) {
            result->rows_indices[i] = temp_rows[i];
            result->coll_indices[i] = temp_cols[i];
            result->values[i] = temp_vals[i];
        }
    }

    free(row_start);
    free(temp_rows);
    free(temp_cols);
    free(temp_vals);
    free(temp_row_buf);
    return result;
}

float* multiplication_matrix_and_vector(COO* matrix, const float* vector)
{
    if (!matrix || !vector)
        return NULL;

    float* result = calloc(matrix->rows, sizeof(float));
    if (!result)
        return NULL;

    for (int i = 0; i < matrix->nnz; ++i) {
        int row = matrix->rows_indices[i];
        int col = matrix->coll_indices[i];
        if (row >= 0 && row < matrix->rows && col >= 0 && col < matrix->columns) {
            result[row] += matrix->values[i] * vector[col];
        }
    }
    return result;
}

COO* multiplication_matrix_and_vector_coo(COO* matrix, COO* vector)
{
    if (!matrix || !vector)
        return NULL;
    if (vector->columns != 1)
        return NULL;

    float* dense_vector = make_table_vector(vector);
    if (!dense_vector)
        return NULL;

    float* result_dense = multiplication_matrix_and_vector(matrix, dense_vector);
    free(dense_vector);

    if (!result_dense)
        return NULL;

    COO* result = malloc(sizeof(COO));
    if (!result) {
        free(result_dense);
        return NULL;
    }

    result->rows = matrix->rows;
    result->columns = 1;

    int nnz = 0;
    for (int i = 0; i < matrix->rows; i++) {
        if (fabsf(result_dense[i]) > 1e-6f)
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
        if (!result->rows_indices || !result->coll_indices || !result->values) {
            free(result->rows_indices);
            free(result->coll_indices);
            free(result->values);
            free(result);
            free(result_dense);
            return NULL;
        }

        int idx = 0;
        for (int i = 0; i < matrix->rows; i++) {
            if (fabsf(result_dense[i]) > 1e-6f) {
                result->rows_indices[idx] = i;
                result->coll_indices[idx] = 0;
                result->values[idx] = result_dense[i];
                idx++;
            }
        }
    }

    free(result_dense);
    return result;
}

COO* multiplication_vector_and_matrix(COO* first, COO* second)
{
    if (first == NULL || second == NULL || !is_line(first))
        return NULL;

    float* vector_values = calloc(first->columns, sizeof(float));
    if (!vector_values)
        return NULL;

    for (int i = 0; i < first->nnz; ++i) {
        if (first->coll_indices[i] < first->columns) {
            vector_values[first->coll_indices[i]] = first->values[i];
        }
    }

    COO* result = malloc(sizeof(COO));
    if (!result) {
        free(vector_values);
        return NULL;
    }

    result->rows = 1;
    result->columns = second->columns;

    float* temp_vals = calloc(second->columns, sizeof(float));
    if (!temp_vals) {
        free(vector_values);
        free(result);
        return NULL;
    }

    for (int i = 0; i < second->nnz; ++i) {
        int row = second->rows_indices[i];
        int col = second->coll_indices[i];
        if (row < first->columns && col < second->columns) {
            temp_vals[col] += second->values[i] * vector_values[row];
        }
    }

    int nnz = 0;
    for (int i = 0; i < second->columns; ++i) {
        if (fabsf(temp_vals[i]) > 1e-6f)
            nnz++;
    }

    result->nnz = nnz;

    if (nnz > 0) {
        result->rows_indices = malloc(sizeof(int) * nnz);
        result->coll_indices = malloc(sizeof(int) * nnz);
        result->values = malloc(sizeof(float) * nnz);
        if (!result->rows_indices || !result->coll_indices || !result->values) {
            free(result->rows_indices);
            free(result->coll_indices);
            free(result->values);
            free(result);
            free(vector_values);
            free(temp_vals);
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
    } else {
        result->rows_indices = NULL;
        result->coll_indices = NULL;
        result->values = NULL;
    }

    free(vector_values);
    free(temp_vals);
    return result;
}

void coo_map(COO* mat, float (*func)(float))
{
    if (!mat || !func || mat->nnz == 0)
        return;
    for (int i = 0; i < mat->nnz; ++i)
        mat->values[i] = func(mat->values[i]);
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
    if (!result)
        return NULL;

    int* result_row_indices = malloc(sizeof(int) * max);
    int* result_coll_indices = malloc(sizeof(int) * max);
    float* result_values = malloc(sizeof(float) * max);

    if (!result_row_indices || !result_coll_indices || !result_values) {
        free(result_row_indices);
        free(result_coll_indices);
        free(result_values);
        free(result);
        return NULL;
    }

    int count = 0, i = 0, j = 0;
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
        if (!result->rows_indices || !result->coll_indices || !result->values) {
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
