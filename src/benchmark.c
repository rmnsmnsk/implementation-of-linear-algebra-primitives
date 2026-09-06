#include "COO.h"
#include "cs.h"
#include "matrix.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define WARMUP_RUNS 3
#define MEASURED_RUNS 15
#define MIN_SAMPLE_DURATION_MS 100.0

typedef struct {
    double median;
    double first_quartile;
    double third_quartile;
} BenchmarkStats;

static double monotonic_time_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return 1000.0 * (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000.0 * (double)ts.tv_sec + (double)ts.tv_nsec / 1000000.0;
#endif
}

static int compare_doubles(const void* first, const void* second)
{
    double a = *(const double*)first;
    double b = *(const double*)second;
    return (a > b) - (a < b);
}

static BenchmarkStats calculate_stats(double* samples)
{
    qsort(samples, MEASURED_RUNS, sizeof(double), compare_doubles);
    BenchmarkStats stats;
    stats.median = samples[MEASURED_RUNS / 2];
    stats.first_quartile = samples[MEASURED_RUNS / 4];
    stats.third_quartile = samples[(3 * MEASURED_RUNS) / 4];
    return stats;
}

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

static int get_cs_effective_nnz(const cs* matrix)
{
    if (!matrix || matrix->nz >= 0)
        return 0;

    int nnz = 0;
    for (int64_t column = 0; column < matrix->n; column++) {
        for (int64_t position = matrix->p[column]; position < matrix->p[column + 1]; position++) {
            if (fabs(matrix->x[position]) > 1e-6)
                nnz++;
        }
    }
    return nnz;
}

static COO* from_cs(const cs* matrix)
{
    if (!matrix || matrix->nz >= 0)
        return NULL;

    int nnz = 0;
    for (int64_t column = 0; column < matrix->n; column++) {
        for (int64_t position = matrix->p[column]; position < matrix->p[column + 1]; position++) {
            if (fabs(matrix->x[position]) > 1e-6)
                nnz++;
        }
    }

    COO* result = malloc(sizeof(COO));
    if (!result)
        return NULL;

    result->rows = (int)matrix->m;
    result->columns = (int)matrix->n;
    result->nnz = nnz;
    result->rows_indices = NULL;
    result->coll_indices = NULL;
    result->values = NULL;

    if (nnz == 0)
        return result;

    result->rows_indices = malloc(sizeof(int) * nnz);
    result->coll_indices = malloc(sizeof(int) * nnz);
    result->values = malloc(sizeof(float) * nnz);
    if (!result->rows_indices || !result->coll_indices || !result->values) {
        free_matrix(result);
        return NULL;
    }

    int index = 0;
    for (int64_t column = 0; column < matrix->n; column++) {
        for (int64_t position = matrix->p[column]; position < matrix->p[column + 1]; position++) {
            if (fabs(matrix->x[position]) <= 1e-6)
                continue;
            result->rows_indices[index] = (int)matrix->i[position];
            result->coll_indices[index] = (int)column;
            result->values[index] = (float)matrix->x[position];
            index++;
        }
    }
    return result;
}

static int results_equal(COO* first, const cs* second)
{
    COO* second_coo = from_cs(second);
    if (!first || !second_coo) {
        free_matrix(second_coo);
        return 0;
    }

    if (first->rows != second_coo->rows || first->columns != second_coo->columns || first->nnz != second_coo->nnz) {
        free_matrix(second_coo);
        return 0;
    }

    if (first->nnz > 0 && (!sort_matrix(first) || !sort_matrix(second_coo))) {
        free_matrix(second_coo);
        return 0;
    }

    int equal = 1;
    for (int i = 0; i < first->nnz; i++) {
        double expected = second_coo->values[i];
        double difference = fabs((double)first->values[i] - expected);
        double tolerance = 1e-4 * (1.0 + fabs(expected));
        if (first->rows_indices[i] != second_coo->rows_indices[i] || first->coll_indices[i] != second_coo->coll_indices[i] || difference > tolerance) {
            equal = 0;
            break;
        }
    }

    free_matrix(second_coo);
    return equal;
}

typedef COO* (*CooOperation)(COO*, COO*);

static double measure_coo_operation(CooOperation operation, COO* first, COO* second, int repetitions)
{
    double start = monotonic_time_ms();
    for (int i = 0; i < repetitions; i++) {
        COO* result = operation(first, second);
        if (!result)
            return -1.0;
        free_matrix(result);
    }
    return (monotonic_time_ms() - start) / repetitions;
}

static double measure_cs_operation(const cs* first, const cs* second, int repetitions)
{
    double start = monotonic_time_ms();
    for (int i = 0; i < repetitions; i++) {
        cs* result = cs_multiply(first, second);
        if (!result)
            return -1.0;
        cs_spfree(result);
    }
    return (monotonic_time_ms() - start) / repetitions;
}

static int choose_coo_repetitions(CooOperation operation, COO* first, COO* second)
{
    int repetitions = 1;
    while (repetitions < (1 << 20)) {
        double average = measure_coo_operation(operation, first, second, repetitions);
        if (average < 0.0 || average * repetitions >= MIN_SAMPLE_DURATION_MS)
            break;
        repetitions *= 2;
    }
    return repetitions;
}

static int choose_cs_repetitions(const cs* first, const cs* second)
{
    int repetitions = 1;
    while (repetitions < (1 << 20)) {
        double average = measure_cs_operation(first, second, repetitions);
        if (average < 0.0 || average * repetitions >= MIN_SAMPLE_DURATION_MS)
            break;
        repetitions *= 2;
    }
    return repetitions;
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

    if (!sort_matrix(a)) {
        printf("sort failed\n");
        free_matrix(a);
        return;
    }
    cs* ca = to_cs(a);
    if (!ca) {
        printf("cs convert failed\n");
        free_matrix(a);
        return;
    }

    for (int i = 0; i < WARMUP_RUNS; i++) {
        COO* warmup_my = multiplication_two_matrix(a, a);
        cs* warmup_cs = cs_multiply(ca, ca);
        if (!warmup_my || !warmup_cs) {
            printf("warmup failed\n");
            free_matrix(warmup_my);
            cs_spfree(warmup_cs);
            cs_spfree(ca);
            free_matrix(a);
            return;
        }
        free_matrix(warmup_my);
        cs_spfree(warmup_cs);
    }

    double my_samples[MEASURED_RUNS];
    double cs_samples[MEASURED_RUNS];
    int my_repetitions = choose_coo_repetitions(multiplication_two_matrix, a, a);
    int cs_repetitions = choose_cs_repetitions(ca, ca);

    for (int i = 0; i < MEASURED_RUNS; i++) {
        my_samples[i] = measure_coo_operation(multiplication_two_matrix, a, a, my_repetitions);
        cs_samples[i] = measure_cs_operation(ca, ca, cs_repetitions);
        if (my_samples[i] < 0.0 || cs_samples[i] < 0.0) {
            printf("measurement failed\n");
            cs_spfree(ca);
            free_matrix(a);
            return;
        }
    }

    COO* my_result = multiplication_two_matrix(a, a);
    cs* cs_result = cs_multiply(ca, ca);
    if (!my_result || !cs_result) {
        printf("verification calculation failed\n");
        free_matrix(my_result);
        cs_spfree(cs_result);
        cs_spfree(ca);
        free_matrix(a);
        return;
    }

    BenchmarkStats my_stats = calculate_stats(my_samples);
    BenchmarkStats cs_stats = calculate_stats(cs_samples);
    printf("REPETITIONS:%s,%d,%d\n", name, my_repetitions, cs_repetitions);
    printf("RESULT_MY:%s,%d,%d,%.6f,%.6f,%.6f\n", name, a->nnz, my_result->nnz, my_stats.median, my_stats.first_quartile, my_stats.third_quartile);
    printf("RESULT_CS:%s,%d,%d,%.6f,%.6f,%.6f\n", name, a->nnz, get_cs_effective_nnz(cs_result), cs_stats.median, cs_stats.first_quartile, cs_stats.third_quartile);
    printf("VERIFY:%s,%s\n", name, results_equal(my_result, cs_result) ? "OK" : "MISMATCH");

    free_matrix(my_result);
    cs_spfree(cs_result);
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

    if (!sort_matrix(a) || !sort_matrix(v)) {
        printf("sort failed\n");
        free_matrix(v);
        free_matrix(a);
        return;
    }

    cs* ca = to_cs(a);
    cs* cv = to_cs(v);
    if (!ca || !cv) {
        printf("cs convert failed\n");
        cs_spfree(ca);
        cs_spfree(cv);
        free_matrix(v);
        free_matrix(a);
        return;
    }

    for (int i = 0; i < WARMUP_RUNS; i++) {
        COO* warmup_my = multiplication_matrix_and_vector_coo(a, v);
        cs* warmup_cs = cs_multiply(ca, cv);
        if (!warmup_my || !warmup_cs) {
            printf("warmup failed\n");
            free_matrix(warmup_my);
            cs_spfree(warmup_cs);
            cs_spfree(ca);
            cs_spfree(cv);
            free_matrix(v);
            free_matrix(a);
            return;
        }
        free_matrix(warmup_my);
        cs_spfree(warmup_cs);
    }

    double my_samples[MEASURED_RUNS];
    double cs_samples[MEASURED_RUNS];
    int my_repetitions = choose_coo_repetitions(multiplication_matrix_and_vector_coo, a, v);
    int cs_repetitions = choose_cs_repetitions(ca, cv);

    for (int i = 0; i < MEASURED_RUNS; i++) {
        my_samples[i] = measure_coo_operation(multiplication_matrix_and_vector_coo, a, v, my_repetitions);
        cs_samples[i] = measure_cs_operation(ca, cv, cs_repetitions);
        if (my_samples[i] < 0.0 || cs_samples[i] < 0.0) {
            printf("measurement failed\n");
            cs_spfree(ca);
            cs_spfree(cv);
            free_matrix(v);
            free_matrix(a);
            return;
        }
    }

    COO* my_result = multiplication_matrix_and_vector_coo(a, v);
    cs* cs_result = cs_multiply(ca, cv);
    if (!my_result || !cs_result) {
        printf("verification calculation failed\n");
        free_matrix(my_result);
        cs_spfree(cs_result);
        cs_spfree(ca);
        cs_spfree(cv);
        free_matrix(v);
        free_matrix(a);
        return;
    }

    BenchmarkStats my_stats = calculate_stats(my_samples);
    BenchmarkStats cs_stats = calculate_stats(cs_samples);
    printf("REPETITIONS:%s,%d,%d\n", name, my_repetitions, cs_repetitions);
    printf("RESULT_MY:%s,%d,%d,%.6f,%.6f,%.6f\n", name, a->nnz, my_result->nnz, my_stats.median, my_stats.first_quartile, my_stats.third_quartile);
    printf("RESULT_CS:%s,%d,%d,%.6f,%.6f,%.6f\n", name, a->nnz, get_cs_effective_nnz(cs_result), cs_stats.median, cs_stats.first_quartile, cs_stats.third_quartile);
    printf("VERIFY:%s,%s\n", name, results_equal(my_result, cs_result) ? "OK" : "MISMATCH");

    free_matrix(my_result);
    cs_spfree(cs_result);
    cs_spfree(ca);
    cs_spfree(cv);
    free_matrix(v);
    free_matrix(a);
}

int main(void)
{
    srand(42);

    const char* matrices[] = {
        "../matrices/dolphins.mtx",
        "../matrices/lesmis.mtx",
        "../matrices/polbooks.mtx",
        "../matrices/football.mtx",
        "../matrices/celegansneural.mtx",
        "../matrices/netscience.mtx",
        "../matrices/add20/add20.mtx",
        "../matrices/ca-GrQc/ca-GrQc.mtx",
        "../matrices/ca-HepTh/ca-HepTh.mtx"
    };
    const char* names[] = {
        "dolphins",
        "lesmis",
        "polbooks",
        "football",
        "celegansneural",
        "netscience",
        "add20",
        "ca-GrQc",
        "ca-HepTh"
    };

    int matrix_count = (int)(sizeof(matrices) / sizeof(matrices[0]));
    for (int i = 0; i < matrix_count; i++) {
        benchmark_matrix_multiply(matrices[i], names[i]);
        benchmark_matrix_vector(matrices[i], names[i]);
    }

    return 0;
}
