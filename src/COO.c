#include "COO.h"
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef COO_PROFILE
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
    uint64_t* touched_bits;
    size_t* touched_words;
    size_t touched_word_count;
    size_t touched_count;
    size_t capacity;
    size_t max_capacity;
    int nnz;
} MatrixMultiplyWorkspace;

static bool initialize_multiply_workspace(const COO* first, const COO* second, MatrixMultiplyWorkspace* workspace)
{
    size_t matrix_elements = (size_t)first->rows * (size_t)second->columns;
    workspace->max_capacity = matrix_elements;
    if (workspace->max_capacity > INT_MAX)
        workspace->max_capacity = INT_MAX;

    workspace->capacity = (size_t)first->nnz * (size_t)second->nnz;
    if (workspace->capacity > matrix_elements)
        workspace->capacity = matrix_elements;
    if (workspace->capacity > 10000000)
        workspace->capacity = 10000000;
    if (workspace->capacity == 0 && workspace->max_capacity > 0)
        workspace->capacity = 1;

    workspace->nnz = 0;
    workspace->touched_count = 0;
    workspace->row_start = calloc((size_t)first->columns + 1, sizeof(int));
    workspace->rows = malloc(sizeof(int) * workspace->capacity);
    workspace->columns = malloc(sizeof(int) * workspace->capacity);
    workspace->values = malloc(sizeof(float) * workspace->capacity);
    workspace->row_buffer = calloc((size_t)second->columns, sizeof(float));
    workspace->touched_word_count = ((size_t)second->columns + 63) / 64;
    workspace->touched_bits = calloc(workspace->touched_word_count, sizeof(uint64_t));
    workspace->touched_words = malloc(sizeof(size_t) * workspace->touched_word_count);

    return workspace->row_start && workspace->rows && workspace->columns && workspace->values && workspace->row_buffer && workspace->touched_bits
        && workspace->touched_words;
}

static void free_multiply_workspace(MatrixMultiplyWorkspace* workspace)
{
    free(workspace->row_start);
    free(workspace->rows);
    free(workspace->columns);
    free(workspace->values);
    free(workspace->row_buffer);
    free(workspace->touched_bits);
    free(workspace->touched_words);
}

static int compare_sizes(const void* first, const void* second)
{
    size_t a = *(const size_t*)first;
    size_t b = *(const size_t*)second;
    return (a > b) - (a < b);
}

static bool reserve_multiply_result(MatrixMultiplyWorkspace* workspace)
{
    if ((size_t)workspace->nnz < workspace->capacity)
        return true;
    if (workspace->capacity >= workspace->max_capacity)
        return false;

    size_t new_capacity = workspace->capacity > workspace->max_capacity / 2 ? workspace->max_capacity : workspace->capacity * 2;
    if (new_capacity == 0)
        new_capacity = 1;

    int* new_rows = malloc(sizeof(int) * new_capacity);
    int* new_columns = malloc(sizeof(int) * new_capacity);
    float* new_values = malloc(sizeof(float) * new_capacity);
    if (!new_rows || !new_columns || !new_values) {
        free(new_rows);
        free(new_columns);
        free(new_values);
        return false;
    }

    if (workspace->nnz > 0) {
        memcpy(new_rows, workspace->rows, sizeof(int) * (size_t)workspace->nnz);
        memcpy(new_columns, workspace->columns, sizeof(int) * (size_t)workspace->nnz);
        memcpy(new_values, workspace->values, sizeof(float) * (size_t)workspace->nnz);
    }
    free(workspace->rows);
    free(workspace->columns);
    free(workspace->values);
    workspace->rows = new_rows;
    workspace->columns = new_columns;
    workspace->values = new_values;
    workspace->capacity = new_capacity;
    return true;
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

static bool accumulate_product_row(const COO* first, const COO* second, MatrixMultiplyWorkspace* workspace, int begin, int end)
{
    for (int i = begin; i < end; i++) {
        int column = first->coll_indices[i];
        if (column < 0 || column >= first->columns)
            return false;
        float value = first->values[i];
        for (int j = workspace->row_start[column]; j < workspace->row_start[column + 1]; j++) {
            int result_column = second->coll_indices[j];
            if (result_column < 0 || result_column >= second->columns)
                return false;
            size_t word = (size_t)result_column / 64;
            unsigned int bit = (unsigned int)result_column % 64;
            uint64_t mask = UINT64_C(1) << bit;
            if (workspace->touched_bits[word] == 0)
                workspace->touched_words[workspace->touched_count++] = word;
            workspace->touched_bits[word] |= mask;
            workspace->row_buffer[result_column] += value * second->values[j];
        }
    }
    return true;
}

static bool flush_product_row(MatrixMultiplyWorkspace* workspace, int row)
{
    bool sparse_scan = workspace->touched_count * 4 < workspace->touched_word_count;
    if (sparse_scan)
        qsort(workspace->touched_words, workspace->touched_count, sizeof(size_t), compare_sizes);

    size_t scan_count = sparse_scan ? workspace->touched_count : workspace->touched_word_count;
    for (size_t index = 0; index < scan_count; index++) {
        size_t word = sparse_scan ? workspace->touched_words[index] : index;
        uint64_t bits = workspace->touched_bits[word];
        workspace->touched_bits[word] = 0;
        while (bits != 0) {
            unsigned int bit = (unsigned int)__builtin_ctzll(bits);
            int column = (int)(word * 64 + bit);
            float value = workspace->row_buffer[column];
            workspace->row_buffer[column] = 0.0f;
            bits &= bits - 1;
            if (fabsf(value) <= 1e-6f)
                continue;
            if (!reserve_multiply_result(workspace))
                return false;
            workspace->rows[workspace->nnz] = row;
            workspace->columns[workspace->nnz] = column;
            workspace->values[workspace->nnz] = value;
            workspace->nnz++;
        }
    }
    workspace->touched_count = 0;
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
        bool accumulated = accumulate_product_row(first, second, &workspace, begin, end);
        PROFILE_ADD(accumulation_ms, accumulation_started);

        PROFILE_BEGIN(buffer_scan_started);
        bool flushed = flush_product_row(&workspace, row);
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

typedef struct {
    int column;
    int row;
    float value;
} COO_ColumnElement;

struct COO_ColumnIndex {
    int rows;
    int columns;
    int nnz;
    int column_count;
    int* column_indices;
    int* column_offsets;
    int* row_indices;
    float* values;
};

typedef struct {
    int* keys;
    float* values;
    unsigned char* occupied;
    size_t capacity;
    size_t size;
    bool dense;
} RowAccumulator;

#define DENSE_ROW_ACCUMULATOR_LIMIT 10000000

static int compare_column_elements(const void* first, const void* second)
{
    const COO_ColumnElement* a = first;
    const COO_ColumnElement* b = second;
    if (a->column != b->column)
        return (a->column > b->column) - (a->column < b->column);
    return (a->row > b->row) - (a->row < b->row);
}

static size_t integer_hash(int key, size_t capacity)
{
    uint32_t value = (uint32_t)key;
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return (size_t)value & (capacity - 1);
}

void free_coo_column_index(COO_ColumnIndex* index)
{
    if (!index)
        return;
    free(index->column_indices);
    free(index->column_offsets);
    free(index->row_indices);
    free(index->values);
    free(index);
}

COO_ColumnIndex* create_coo_column_index(const COO* matrix)
{
    if (!matrix || matrix->rows < 0 || matrix->columns < 0 || matrix->nnz < 0)
        return NULL;
    if (matrix->nnz > 0 && (!matrix->rows_indices || !matrix->coll_indices || !matrix->values))
        return NULL;

    COO_ColumnIndex* index = calloc(1, sizeof(COO_ColumnIndex));
    if (!index)
        return NULL;
    index->rows = matrix->rows;
    index->columns = matrix->columns;
    index->nnz = matrix->nnz;
    if (matrix->nnz == 0)
        return index;

    COO_ColumnElement* elements = malloc(sizeof(COO_ColumnElement) * (size_t)matrix->nnz);
    if (!elements) {
        free_coo_column_index(index);
        return NULL;
    }

    for (int i = 0; i < matrix->nnz; i++) {
        int row = matrix->rows_indices[i];
        int column = matrix->coll_indices[i];
        if (row < 0 || row >= matrix->rows || column < 0 || column >= matrix->columns) {
            free(elements);
            free_coo_column_index(index);
            return NULL;
        }
        elements[i].column = column;
        elements[i].row = row;
        elements[i].value = matrix->values[i];
    }
    qsort(elements, (size_t)matrix->nnz, sizeof(COO_ColumnElement), compare_column_elements);

    index->column_count = 1;
    for (int i = 1; i < matrix->nnz; i++) {
        if (elements[i].column != elements[i - 1].column)
            index->column_count++;
    }

    index->column_indices = malloc(sizeof(int) * (size_t)index->column_count);
    index->column_offsets = malloc(sizeof(int) * ((size_t)index->column_count + 1));
    index->row_indices = malloc(sizeof(int) * (size_t)matrix->nnz);
    index->values = malloc(sizeof(float) * (size_t)matrix->nnz);
    if (!index->column_indices || !index->column_offsets || !index->row_indices || !index->values) {
        free(elements);
        free_coo_column_index(index);
        return NULL;
    }

    int column_position = -1;
    int previous_column = -1;
    for (int i = 0; i < matrix->nnz; i++) {
        if (i == 0 || elements[i].column != previous_column) {
            column_position++;
            previous_column = elements[i].column;
            index->column_indices[column_position] = previous_column;
            index->column_offsets[column_position] = i;
        }
        index->row_indices[i] = elements[i].row;
        index->values[i] = elements[i].value;
    }
    index->column_offsets[index->column_count] = matrix->nnz;
    free(elements);
    return index;
}

static int find_indexed_column(const COO_ColumnIndex* index, int column)
{
    int left = 0;
    int right = index->column_count;
    while (left < right) {
        int middle = left + (right - left) / 2;
        if (index->column_indices[middle] < column)
            left = middle + 1;
        else
            right = middle;
    }
    if (left < index->column_count && index->column_indices[left] == column)
        return left;
    return -1;
}

static void free_row_accumulator(RowAccumulator* accumulator)
{
    free(accumulator->keys);
    free(accumulator->values);
    free(accumulator->occupied);
}

static bool initialize_row_accumulator(RowAccumulator* accumulator, int row_count)
{
    if (row_count > 0 && row_count <= DENSE_ROW_ACCUMULATOR_LIMIT) {
        accumulator->capacity = (size_t)row_count;
        accumulator->values = calloc(accumulator->capacity, sizeof(float));
        accumulator->occupied = calloc(accumulator->capacity, sizeof(unsigned char));
        accumulator->dense = true;
        return accumulator->values && accumulator->occupied;
    }

    accumulator->capacity = 8;
    accumulator->keys = malloc(sizeof(int) * accumulator->capacity);
    accumulator->values = calloc(accumulator->capacity, sizeof(float));
    accumulator->occupied = calloc(accumulator->capacity, sizeof(unsigned char));
    return accumulator->keys && accumulator->values && accumulator->occupied;
}

static bool grow_row_accumulator(RowAccumulator* accumulator)
{
    if (accumulator->capacity > SIZE_MAX / 2)
        return false;
    size_t new_capacity = accumulator->capacity * 2;
    int* new_keys = malloc(sizeof(int) * new_capacity);
    float* new_values = calloc(new_capacity, sizeof(float));
    unsigned char* new_occupied = calloc(new_capacity, sizeof(unsigned char));
    if (!new_keys || !new_values || !new_occupied) {
        free(new_keys);
        free(new_values);
        free(new_occupied);
        return false;
    }

    for (size_t i = 0; i < accumulator->capacity; i++) {
        if (!accumulator->occupied[i])
            continue;
        size_t position = integer_hash(accumulator->keys[i], new_capacity);
        while (new_occupied[position])
            position = (position + 1) & (new_capacity - 1);
        new_occupied[position] = 1;
        new_keys[position] = accumulator->keys[i];
        new_values[position] = accumulator->values[i];
    }

    free(accumulator->keys);
    free(accumulator->values);
    free(accumulator->occupied);
    accumulator->keys = new_keys;
    accumulator->values = new_values;
    accumulator->occupied = new_occupied;
    accumulator->capacity = new_capacity;
    return true;
}

static bool add_row_value(RowAccumulator* accumulator, int row, float value)
{
    if (accumulator->dense) {
        size_t position = (size_t)row;
        if (!accumulator->occupied[position]) {
            accumulator->occupied[position] = 1;
            accumulator->size++;
        }
        accumulator->values[position] += value;
        return true;
    }

    if ((accumulator->size + 1) * 2 > accumulator->capacity && !grow_row_accumulator(accumulator))
        return false;

    size_t position = integer_hash(row, accumulator->capacity);
    while (accumulator->occupied[position] && accumulator->keys[position] != row)
        position = (position + 1) & (accumulator->capacity - 1);
    if (!accumulator->occupied[position]) {
        accumulator->occupied[position] = 1;
        accumulator->keys[position] = row;
        accumulator->size++;
    }
    accumulator->values[position] += value;
    return true;
}

static COO* create_indexed_vector_result(const COO_ColumnIndex* index, const RowAccumulator* accumulator)
{
    COO* result = calloc(1, sizeof(COO));
    if (!result)
        return NULL;
    result->rows = index->rows;
    result->columns = 1;

    for (size_t i = 0; i < accumulator->capacity; i++) {
        if (accumulator->occupied[i] && fabsf(accumulator->values[i]) > 1e-6f)
            result->nnz++;
    }
    if (result->nnz == 0)
        return result;

    result->rows_indices = malloc(sizeof(int) * (size_t)result->nnz);
    result->coll_indices = malloc(sizeof(int) * (size_t)result->nnz);
    result->values = malloc(sizeof(float) * (size_t)result->nnz);
    if (!result->rows_indices || !result->coll_indices || !result->values) {
        free_matrix(result);
        return NULL;
    }

    int result_position = 0;
    for (size_t i = 0; i < accumulator->capacity; i++) {
        if (!accumulator->occupied[i] || fabsf(accumulator->values[i]) <= 1e-6f)
            continue;
        result->rows_indices[result_position] = accumulator->dense ? (int)i : accumulator->keys[i];
        result->coll_indices[result_position] = 0;
        result->values[result_position] = accumulator->values[i];
        result_position++;
    }
    if (!accumulator->dense && !sort_matrix(result)) {
        free_matrix(result);
        return NULL;
    }
    return result;
}

COO* multiplication_matrix_and_vector_coo_indexed(const COO_ColumnIndex* index, const COO* vector)
{
    if (!index || !vector || vector->columns != 1 || index->columns != vector->rows || vector->nnz < 0)
        return NULL;
    if (vector->nnz > 0 && (!vector->rows_indices || !vector->coll_indices || !vector->values))
        return NULL;

    RowAccumulator accumulator = { 0 };
    if (!initialize_row_accumulator(&accumulator, index->rows)) {
        free_row_accumulator(&accumulator);
        return NULL;
    }

    for (int i = 0; i < vector->nnz; i++) {
        int column = vector->rows_indices[i];
        if (column < 0 || column >= vector->rows || vector->coll_indices[i] != 0) {
            free_row_accumulator(&accumulator);
            return NULL;
        }
        if (fabsf(vector->values[i]) <= 1e-6f)
            continue;

        int column_position = find_indexed_column(index, column);
        if (column_position < 0)
            continue;
        int begin = index->column_offsets[column_position];
        int end = index->column_offsets[column_position + 1];
        for (int j = begin; j < end; j++) {
            if (!add_row_value(&accumulator, index->row_indices[j], index->values[j] * vector->values[i])) {
                free_row_accumulator(&accumulator);
                return NULL;
            }
        }
    }

    COO* result = create_indexed_vector_result(index, &accumulator);
    free_row_accumulator(&accumulator);
    return result;
}

typedef struct {
    int* keys;
    float* values;
    unsigned char* occupied;
    size_t capacity;
} SparseVectorIndex;

static void free_sparse_vector_index(SparseVectorIndex* index)
{
    free(index->keys);
    free(index->values);
    free(index->occupied);
}

static size_t sparse_vector_hash(int key, size_t capacity)
{
    uint32_t value = (uint32_t)key;
    value ^= value >> 16;
    value *= UINT32_C(0x7feb352d);
    value ^= value >> 15;
    value *= UINT32_C(0x846ca68b);
    value ^= value >> 16;
    return (size_t)value & (capacity - 1);
}

static bool initialize_sparse_vector_index(const COO* vector, SparseVectorIndex* index)
{
    if ((size_t)vector->nnz > SIZE_MAX / 2)
        return false;

    size_t required_capacity = (size_t)vector->nnz * 2;
    index->capacity = 8;
    while (index->capacity < required_capacity) {
        if (index->capacity > SIZE_MAX / 2)
            return false;
        index->capacity *= 2;
    }

    index->keys = malloc(sizeof(int) * index->capacity);
    index->values = calloc(index->capacity, sizeof(float));
    index->occupied = calloc(index->capacity, sizeof(unsigned char));
    if (!index->keys || !index->values || !index->occupied)
        return false;

    for (int i = 0; i < vector->nnz; i++) {
        int key = vector->rows_indices[i];
        size_t position = sparse_vector_hash(key, index->capacity);
        while (index->occupied[position] && index->keys[position] != key)
            position = (position + 1) & (index->capacity - 1);

        if (!index->occupied[position]) {
            index->occupied[position] = 1;
            index->keys[position] = key;
        }
        index->values[position] += vector->values[i];
    }
    return true;
}

static float find_sparse_vector_value(const SparseVectorIndex* index, int row)
{
    PROFILE_INCREMENT(lookup_calls);
    size_t position = sparse_vector_hash(row, index->capacity);
    while (index->occupied[position]) {
        PROFILE_INCREMENT(lookup_comparisons);
        if (index->keys[position] == row)
            return index->values[position];
        position = (position + 1) & (index->capacity - 1);
    }
    return 0.0f;
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
    if (!sort_matrix(matrix)) {
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

    SparseVectorIndex vector_index = { 0 };
    PROFILE_BEGIN(index_started);
    if (!initialize_sparse_vector_index(vector, &vector_index)) {
        free_sparse_vector_index(&vector_index);
        free_matrix(result);
        return NULL;
    }
    PROFILE_ADD(index_ms, index_started);

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

        row_sum += matrix->values[i] * find_sparse_vector_value(&vector_index, column);
    }

    if (fabsf(row_sum) > 1e-6f) {
        result->rows_indices[result->nnz] = current_row;
        result->coll_indices[result->nnz] = 0;
        result->values[result->nnz] = row_sum;
        result->nnz++;
    }
    PROFILE_ADD(accumulation_ms, accumulation_started);

    PROFILE_BEGIN(cleanup_started);
    free_sparse_vector_index(&vector_index);
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
