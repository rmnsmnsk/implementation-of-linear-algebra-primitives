#pragma once
#include <stdbool.h>

typedef struct COO {
    int nnz; // кол-во ненулевых элементов
    int rows; // кол-во строк
    int columns; // кол-во столбцов
    int* rows_indices; // индексы строк ненулевых элементов
    int* coll_indices; // индексы столбцов ненулевых элементов
    float* values; // сами значения
} COO;

void free_matrix(COO* matrix);
COO* create_matrix_copy(COO* original);
COO* sort_matrix(COO* matrix);
bool is_line(COO* matrix);
float* make_table_vector(COO* vector);

float* multiplication_matrix_and_vector(COO* matrix, const float* vector);
COO* multiplication_matrix_and_vector_coo(COO* matrix, COO* vector);
COO* multiplication_two_matrix(COO* first, COO* second);
COO* multiplication_vector_and_matrix(COO* first, COO* second);
void coo_map(COO* mat, float (*func)(float));
COO* coo_map2(COO* first, COO* second, float (*func)(float, float));

#ifdef COO_PROFILE
typedef struct {
    unsigned long calls;
    unsigned long long lookup_calls;
    unsigned long long lookup_comparisons;
    double total_ms;
    double sort_ms;
    double workspace_ms;
    double index_ms;
    double accumulation_ms;
    double buffer_scan_ms;
    double result_ms;
    double cleanup_ms;
} COO_Profile;

void coo_profile_reset(void);
COO_Profile coo_profile_get(void);
#endif
