#pragma once
#include <stdbool.h>

typedef struct COO {
    int nnz; // кол-во ненулевых элементов
    int rows; // кол-во строк
    int columns; // кол-во столбцов
    int* rows_indices; // индексы !нулевых элементов
    int* coll_indices;
    float* values; // сами значения
} COO;
void free_matrix(COO* matrix);
COO* create_matrix_copy(COO* original);
COO* sort_matrix(COO* matrix);
bool is_line(COO* matrix);
float* make_table_vector(COO* vector);
COO* multiplication_two_matrix(COO* first, COO* second);
COO* multiplication_matrix_and_vector(COO* first, COO* second);
COO* multiplication_vector_and_matrix(COO* first, COO* second);
void coo_map(COO* mat, float (*func)(float));
COO* coo_map2(COO* first, COO* second, float (*func)(float, float));
