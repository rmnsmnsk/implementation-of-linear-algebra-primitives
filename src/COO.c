#include "COO.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef COO_PROFILE
#include <string.h>
#include <time.h>

static COO_Profile active_profile;

static double profile_time_ms(void)
{
    struct timespec time;
    timespec_get(&time, TIME_UTC);
    return (double)time.tv_sec * 1000.0 + (double)time.tv_nsec / 1000000.0;
}

void coo_profile_reset(void)
{
    memset(&active_profile, 0, sizeof(active_profile));
}

COO_Profile coo_profile_get(void)
{
    return active_profile;
}

#define PROFILE_BEGIN(name) double name = profile_time_ms()
#define PROFILE_ADD(field, name) (active_profile.field += (profile_time_ms() - (name)))
#define PROFILE_INCREMENT(field) active_profile.field++
#else
#define PROFILE_BEGIN(name)
#define PROFILE_ADD(field, name)
#define PROFILE_INCREMENT(field)
#endif

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

static bool coo_is_sorted(const COO* matrix)
{
    for (int i = 1; i < matrix->nnz; i++) {
        if (matrix->rows_indices[i - 1] > matrix->rows_indices[i])
            return false;
        if (matrix->rows_indices[i - 1] == matrix->rows_indices[i] && matrix->coll_indices[i - 1] > matrix->coll_indices[i])
            return false;
    }
    return true;
}

COO* sort_matrix(COO* matrix)
{
    if (matrix == NULL || matrix->nnz <= 0)
        return NULL;
    if (matrix->rows_indices == NULL || matrix->coll_indices == NULL || matrix->values == NULL)
        return NULL;

    if (coo_is_sorted(matrix))
        return matrix;

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

typedef struct {
    int* row_start;
    int* rows;
    int* columns;
    float* values;
    float* row_buffer;
    size_t capacity;
    int nnz;
} MatrixMultiplyWorkspace;

static bool initialize_multiply_workspace(const COO* first, const COO* second, MatrixMultiplyWorkspace* workspace)
{
    workspace->capacity = (size_t)first->nnz * (size_t)second->nnz;
    size_t matrix_elements = (size_t)first->rows * (size_t)second->columns;
    if (workspace->capacity > matrix_elements)
        workspace->capacity = matrix_elements;
    if (workspace->capacity > 10000000)
        workspace->capacity = 10000000;

    workspace->nnz = 0;
    workspace->row_start = calloc((size_t)first->columns + 1, sizeof(int));
    workspace->rows = malloc(sizeof(int) * workspace->capacity);
    workspace->columns = malloc(sizeof(int) * workspace->capacity);
    workspace->values = malloc(sizeof(float) * workspace->capacity);
    workspace->row_buffer = calloc((size_t)second->columns, sizeof(float));

    return workspace->row_start && workspace->rows && workspace->columns && workspace->values && workspace->row_buffer;
}

static void free_multiply_workspace(MatrixMultiplyWorkspace* workspace)
{
    free(workspace->row_start);
    free(workspace->rows);
    free(workspace->columns);
    free(workspace->values);
    free(workspace->row_buffer);
}

static bool build_row_index(const COO* second, int row_count, int* row_start)
{
    for (int i = 0; i < second->nnz; i++) {
        int row = second->rows_indices[i];
        if (row < 0 || row >= row_count)
            return false;
        row_start[row + 1]++;
    }
    for (int i = 0; i < row_count; i++)
        row_start[i + 1] += row_start[i];
    return true;
}

static bool accumulate_product_row(const COO* first, const COO* second, const int* row_start, int begin, int end, float* row_buffer)
{
    for (int i = begin; i < end; i++) {
        int column = first->coll_indices[i];
        if (column < 0 || column >= first->columns)
            return false;
        float value = first->values[i];
        for (int j = row_start[column]; j < row_start[column + 1]; j++) {
            int result_column = second->coll_indices[j];
            if (result_column < 0 || result_column >= second->columns)
                return false;
            row_buffer[result_column] += value * second->values[j];
        }
    }
    return true;
}

static bool flush_product_row(MatrixMultiplyWorkspace* workspace, int row, int column_count)
{
    for (int column = 0; column < column_count; column++) {
        float value = workspace->row_buffer[column];
        workspace->row_buffer[column] = 0.0f;
        if (fabsf(value) <= 1e-6f)
            continue;
        if ((size_t)workspace->nnz >= workspace->capacity)
            return false;
        workspace->rows[workspace->nnz] = row;
        workspace->columns[workspace->nnz] = column;
        workspace->values[workspace->nnz] = value;
        workspace->nnz++;
    }
    return true;
}

static COO* create_multiply_result(const COO* first, const COO* second, const MatrixMultiplyWorkspace* workspace)
{
    COO* result = malloc(sizeof(COO));
    if (!result)
        return NULL;

    result->rows = first->rows;
    result->columns = second->columns;
    result->nnz = workspace->nnz;
    result->rows_indices = NULL;
    result->coll_indices = NULL;
    result->values = NULL;

    if (workspace->nnz == 0)
        return result;

    result->rows_indices = malloc(sizeof(int) * workspace->nnz);
    result->coll_indices = malloc(sizeof(int) * workspace->nnz);
    result->values = malloc(sizeof(float) * workspace->nnz);
    if (!result->rows_indices || !result->coll_indices || !result->values) {
        free_matrix(result);
        return NULL;
    }

    for (int i = 0; i < workspace->nnz; i++) {
        result->rows_indices[i] = workspace->rows[i];
        result->coll_indices[i] = workspace->columns[i];
        result->values[i] = workspace->values[i];
    }
    return result;
}

COO* multiplication_two_matrix(COO* first, COO* second)
{
    if (!first || !second || first->columns != second->rows)
        return NULL;
    if (!first->rows_indices || !second->rows_indices)
        return NULL;

    PROFILE_BEGIN(total_started);
    PROFILE_BEGIN(sort_started);
    first = sort_matrix(first);
    second = sort_matrix(second);
    PROFILE_ADD(sort_ms, sort_started);
    if (!first || !second)
        return NULL;

    MatrixMultiplyWorkspace workspace = { 0 };
    PROFILE_BEGIN(workspace_started);
    if (!initialize_multiply_workspace(first, second, &workspace)) {
        free_multiply_workspace(&workspace);
        return NULL;
    }
    PROFILE_ADD(workspace_ms, workspace_started);

    PROFILE_BEGIN(index_started);
    if (!build_row_index(second, first->columns, workspace.row_start)) {
        free_multiply_workspace(&workspace);
        return NULL;
    }
    PROFILE_ADD(index_ms, index_started);

    int begin = 0;
    while (begin < first->nnz) {
        int row = first->rows_indices[begin];
        if (row < 0 || row >= first->rows) {
            free_multiply_workspace(&workspace);
            return NULL;
        }

        int end = begin + 1;
        while (end < first->nnz && first->rows_indices[end] == row)
            end++;

        PROFILE_BEGIN(accumulation_started);
        bool accumulated = accumulate_product_row(first, second, workspace.row_start, begin, end, workspace.row_buffer);
        PROFILE_ADD(accumulation_ms, accumulation_started);

        PROFILE_BEGIN(buffer_scan_started);
        bool flushed = flush_product_row(&workspace, row, second->columns);
        PROFILE_ADD(buffer_scan_ms, buffer_scan_started);

        if (!accumulated || !flushed) {
            free_multiply_workspace(&workspace);
            return NULL;
        }
        begin = end;
    }

    PROFILE_BEGIN(result_started);
    COO* result = create_multiply_result(first, second, &workspace);
    PROFILE_ADD(result_ms, result_started);
    PROFILE_BEGIN(cleanup_started);
    free_multiply_workspace(&workspace);
    PROFILE_ADD(cleanup_ms, cleanup_started);
    PROFILE_ADD(total_ms, total_started);
    PROFILE_INCREMENT(calls);
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

static float find_sparse_vector_value(const COO* vector, int row)
{
    PROFILE_INCREMENT(lookup_calls);
    int left = 0;
    int right = vector->nnz;
    while (left < right) {
        PROFILE_INCREMENT(lookup_comparisons);
        int middle = left + (right - left) / 2;
        if (vector->rows_indices[middle] < row)
            left = middle + 1;
        else
            right = middle;
    }

    float value = 0.0f;
    while (left < vector->nnz && vector->rows_indices[left] == row) {
        PROFILE_INCREMENT(lookup_comparisons);
        value += vector->values[left];
        left++;
    }
    return value;
}

COO* multiplication_matrix_and_vector_coo(COO* matrix, COO* vector)
{
    if (!matrix || !vector)
        return NULL;
    if (vector->columns != 1 || matrix->columns != vector->rows)
        return NULL;

    PROFILE_BEGIN(total_started);
    PROFILE_BEGIN(workspace_started);

    COO* result = malloc(sizeof(COO));
    if (!result)
        return NULL;

    result->rows = matrix->rows;
    result->columns = 1;
    result->nnz = 0;
    result->rows_indices = NULL;
    result->coll_indices = NULL;
    result->values = NULL;

    if (matrix->nnz == 0 || vector->nnz == 0)
        return result;

    if (!matrix->rows_indices || !matrix->coll_indices || !matrix->values || !vector->rows_indices || !vector->coll_indices || !vector->values) {
        free(result);
        return NULL;
    }

    for (int i = 0; i < matrix->nnz; i++) {
        if (matrix->rows_indices[i] < 0 || matrix->rows_indices[i] >= matrix->rows || matrix->coll_indices[i] < 0 || matrix->coll_indices[i] >= matrix->columns) {
            free(result);
            return NULL;
        }
    }
    for (int i = 0; i < vector->nnz; i++) {
        if (vector->rows_indices[i] < 0 || vector->rows_indices[i] >= vector->rows || vector->coll_indices[i] != 0) {
            free(result);
            return NULL;
        }
    }
    PROFILE_ADD(workspace_ms, workspace_started);

    PROFILE_BEGIN(sort_started);
    if (!sort_matrix(matrix) || !sort_matrix(vector)) {
        free(result);
        return NULL;
    }
    PROFILE_ADD(sort_ms, sort_started);

    PROFILE_BEGIN(result_started);
    int capacity = matrix->nnz < matrix->rows ? matrix->nnz : matrix->rows;
    result->rows_indices = malloc(sizeof(int) * capacity);
    result->coll_indices = malloc(sizeof(int) * capacity);
    result->values = malloc(sizeof(float) * capacity);
    if (!result->rows_indices || !result->coll_indices || !result->values) {
        free_matrix(result);
        return NULL;
    }
    PROFILE_ADD(result_ms, result_started);

    int current_row = matrix->rows_indices[0];
    float row_sum = 0.0f;

    PROFILE_BEGIN(accumulation_started);
    for (int i = 0; i < matrix->nnz; i++) {
        int row = matrix->rows_indices[i];
        int column = matrix->coll_indices[i];

        if (row != current_row) {
            if (fabsf(row_sum) > 1e-6f) {
                result->rows_indices[result->nnz] = current_row;
                result->coll_indices[result->nnz] = 0;
                result->values[result->nnz] = row_sum;
                result->nnz++;
            }
            current_row = row;
            row_sum = 0.0f;
        }

        row_sum += matrix->values[i] * find_sparse_vector_value(vector, column);
    }

    if (fabsf(row_sum) > 1e-6f) {
        result->rows_indices[result->nnz] = current_row;
        result->coll_indices[result->nnz] = 0;
        result->values[result->nnz] = row_sum;
        result->nnz++;
    }
    PROFILE_ADD(accumulation_ms, accumulation_started);

    PROFILE_BEGIN(cleanup_started);
    if (result->nnz == 0) {
        free(result->rows_indices);
        free(result->coll_indices);
        free(result->values);
        result->rows_indices = NULL;
        result->coll_indices = NULL;
        result->values = NULL;
    }
    PROFILE_ADD(cleanup_ms, cleanup_started);
    PROFILE_ADD(total_ms, total_started);
    PROFILE_INCREMENT(calls);

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
