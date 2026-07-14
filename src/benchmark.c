#include "COO.h"
#include "cs.h"
#include "matrix.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

COO* create_random_vector(int size, float density)
{
    if (size <= 0)
        return NULL;

    int nnz = (int)((float)size * density);
    if (nnz < 1)
        nnz = 1;
    if (nnz > size)
        nnz = size;

    COO* v = malloc(sizeof(COO));
    if (!v)
        return NULL;

    v->rows = size;
    v->columns = 1;
    v->nnz = nnz;

    v->rows_indices = malloc(nnz * sizeof(int));
    v->coll_indices = malloc(nnz * sizeof(int));
    v->values = malloc(nnz * sizeof(float));

    if (!v->rows_indices || !v->coll_indices || !v->values) {
        free_matrix(v);
        return NULL;
    }

    int* used = calloc(size, sizeof(int));
    if (!used) {
        free_matrix(v);
        return NULL;
    }

    int count = 0;
    while (count < nnz) {
        int idx = rand() % size;
        if (!used[idx]) {
            used[idx] = 1;
            v->rows_indices[count] = idx;
            v->coll_indices[count] = 0;
            v->values[count] = (float)(rand() % 100) / 10.0f + 1.0f;
            count++;
        }
    }

    free(used);
    return v;
}

cs* to_cs(COO* m)
{
    if (!m)
        return NULL;

    int64_t* col_counts = calloc(m->columns, sizeof(int64_t));
    if (!col_counts)
        return NULL;

    for (int i = 0; i < m->nnz; i++) {
        col_counts[m->coll_indices[i]]++;
    }

    cs* A = malloc(sizeof(cs));
    if (!A) {
        free(col_counts);
        return NULL;
    }

    A->m = m->rows;
    A->n = m->columns;
    A->nz = -1;
    A->nzmax = m->nnz;
    A->i = malloc(m->nnz * sizeof(int64_t));
    A->p = malloc((m->columns + 1) * sizeof(int64_t));
    A->x = malloc(m->nnz * sizeof(double));

    if (!A->i || !A->p || !A->x) {
        free(A->i);
        free(A->p);
        free(A->x);
        free(A);
        free(col_counts);
        return NULL;
    }

    A->p[0] = 0;
    for (int i = 0; i < m->columns; i++) {
        A->p[i + 1] = A->p[i] + col_counts[i];
    }

    int64_t* pos = malloc(m->columns * sizeof(int64_t));
    if (!pos) {
        free(A->i);
        free(A->p);
        free(A->x);
        free(A);
        free(col_counts);
        return NULL;
    }

    for (int i = 0; i < m->columns; i++) {
        pos[i] = A->p[i];
    }

    for (int i = 0; i < m->nnz; i++) {
        int col = m->coll_indices[i];
        int64_t idx = pos[col]++;
        A->i[idx] = m->rows_indices[i];
        A->x[idx] = (double)m->values[i];
    }

    free(col_counts);
    free(pos);

    return A;
}

int get_cs_nnz(cs* A)
{
    if (!A)
        return 0;
    if (A->nz >= 0)
        return (int)A->nz;
    int64_t nnz = 0;
    for (int64_t i = 0; i < A->n; i++) {
        nnz += A->p[i + 1] - A->p[i];
    }
    return (int)nnz;
}

void benchmark_matrix_multiply(const char* path, const char* name)
{
    printf("\nMatrix-Matrix Multiplication: %s\n", name);

    COO* a = read_matrix_market(path);
    if (!a) {
        printf("load failed\n");
        return;
    }

    if (a->rows != a->columns) {
        printf("not square, skip\n");
        free_matrix(a);
        return;
    }

    clock_t t1 = clock();
    COO* res = multiplication_two_matrix(a, a);
    clock_t t2 = clock();

    if (res) {
        double sec = (double)(t2 - t1) / CLOCKS_PER_SEC;
        printf("RESULT_MY:%s,%d,%d,%.3f\n", name, a->nnz, res->nnz, sec * 1000);
        free_matrix(res);
    } else {
        printf("my mul failed\n");
    }

    cs* ca = to_cs(a);
    if (!ca) {
        printf("cs convert failed\n");
        free_matrix(a);
        return;
    }

    clock_t t3 = clock();
    cs* cc = cs_multiply(ca, ca);
    clock_t t4 = clock();

    if (cc) {
        double sec = (double)(t4 - t3) / CLOCKS_PER_SEC;
        int nnz = get_cs_nnz(cc);
        printf("RESULT_CS:%s,%d,%d,%.3f\n", name, a->nnz, nnz, sec * 1000);
        cs_spfree(cc);
    } else {
        printf("cs mul failed\n");
    }

    cs_spfree(ca);
    free_matrix(a);
}

void benchmark_matrix_vector(const char* path, const char* name)
{
    printf("\nMatrix-Vector Multiplication: %s\n", name);

    COO* a = read_matrix_market(path);
    if (!a) {
        printf("load failed\n");
        return;
    }

    COO* v = create_random_vector(a->columns, 0.1f);
    if (!v) {
        printf("vector create failed\n");
        free_matrix(a);
        return;
    }

    clock_t t1 = clock();
    COO* res = multiplication_matrix_and_vector_coo(a, v);
    clock_t t2 = clock();

    if (res) {
        double sec = (double)(t2 - t1) / CLOCKS_PER_SEC;
        printf("RESULT_MY:%s,%d,%d,%.3f\n", name, a->nnz, res->nnz, sec * 1000);
        free_matrix(res);
    } else {
        printf("my vec mul failed\n");
    }

    cs* ca = to_cs(a);
    cs* cv = to_cs(v);

    if (ca && cv) {
        clock_t t3 = clock();
        cs* cc = cs_multiply(ca, cv);
        clock_t t4 = clock();

        if (cc) {
            double sec = (double)(t4 - t3) / CLOCKS_PER_SEC;
            int nnz = get_cs_nnz(cc);
            printf("RESULT_CS:%s,%d,%d,%.3f\n", name, a->nnz, nnz, sec * 1000);
            cs_spfree(cc);
        } else {
            printf("cs vec mul failed\n");
        }
    }

    cs_spfree(ca);
    cs_spfree(cv);
    free_matrix(v);
    free_matrix(a);
}

int main()
{
    srand(time(NULL));

    const char* matrices[] = {
        "../matrices/dolphins.mtx",
        "../matrices/lesmis.mtx",
        "../matrices/polbooks.mtx",
        "../matrices/football.mtx",
        "../matrices/celegansneural.mtx",
        "../matrices/netscience.mtx"
    };
    const char* names[] = {
        "dolphins",
        "lesmis",
        "polbooks",
        "football",
        "celegansneural",
        "netscience"
    };

    for (int i = 0; i < 6; i++) {
        benchmark_matrix_multiply(matrices[i], names[i]);
        benchmark_matrix_vector(matrices[i], names[i]);
    }

    return 0;
}