#include "COO.h"
#include "matrix.h"
#include "cs.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

cs* to_cs(COO* m)
{
    if (!m) return NULL;

    cs* T = cs_spalloc(m->nnz, m->rows, m->columns, 1, 1);
    if (!T) return NULL;

    for (int i = 0; i < m->nnz; i++) {
        if (!cs_entry(T, m->rows_indices[i], m->coll_indices[i], (double)m->values[i])) {
            cs_spfree(T);
            return NULL;
        }
    }

    cs* C = cs_compress(T);
    cs_spfree(T);
    return C;
}

int get_cs_nnz(cs* A)
{
    if (!A) return 0;
    if (A->nz >= 0) return (int)A->nz;
    int nnz = 0;
    for (int i = 0; i < A->n; i++) {
        nnz += A->p[i+1] - A->p[i];
    }
    return nnz;
}

void run(const char* path, const char* name)
{
    if (!path || !name) return;
    printf("%s\n", name);

    COO* a = read_matrix_market(path);
    if (!a) {
        printf("load failed\n");
        return;
    }
    print_matrix_info(a, name);

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
        printf("my: nnz=%d time=%.3fs\n", res->nnz, sec);
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

    cs* ca_t = cs_transpose(ca, 1);
    if (!ca_t) {
        printf("cs transpose failed\n");
        cs_spfree(ca);
        free_matrix(a);
        return;
    }

    clock_t t3 = clock();
    cs* cc = cs_multiply(ca, ca_t);
    clock_t t4 = clock();

    if (cc) {
        double sec = (double)(t4 - t3) / CLOCKS_PER_SEC;
        int nnz = get_cs_nnz(cc);
        printf("cs: nnz=%d time=%.3fs\n", nnz, sec);
        printf("RESULT_CS:%s,%d,%d,%.3f\n", name, a->nnz, nnz, sec * 1000);
        cs_spfree(cc);
    } else {
        printf("cs mul failed\n");
    }

    cs_spfree(ca_t);
    cs_spfree(ca);
    free_matrix(a);
}

int main()
{
    run("../matrices/dolphins.mtx", "dolphins");
    run("../matrices/lesmis.mtx", "lesmis");
    run("../matrices/polbooks.mtx", "polbooks");
    run("../matrices/football.mtx", "football");
    run("../matrices/celegansneural.mtx", "celegansneural");
    run("../matrices/netscience.mtx", "netscience");
    return 0;
}