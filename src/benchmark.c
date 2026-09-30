#include "COO.h"
#include "cs.h"
#include "matrix.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define WARMUP_RUNS 3
#define MEASURED_RUNS 15
#define MIN_SAMPLE_DURATION_MS 100.0
#define STUDENT_T_95_DF14 2.1447866879

typedef struct {
    double mean;
    double standard_deviation;
    double confidence_95;
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

static BenchmarkStats calculate_stats(const double* samples)
{
    double sum = 0.0;
    for (int i = 0; i < MEASURED_RUNS; i++)
        sum += samples[i];

    BenchmarkStats stats;
    stats.mean = sum / MEASURED_RUNS;

    double squared_deviations = 0.0;
    for (int i = 0; i < MEASURED_RUNS; i++) {
        double deviation = samples[i] - stats.mean;
        squared_deviations += deviation * deviation;
    }
    stats.standard_deviation = sqrt(squared_deviations / (MEASURED_RUNS - 1));
    stats.confidence_95 = STUDENT_T_95_DF14 * stats.standard_deviation / sqrt((double)MEASURED_RUNS);
    return stats;
}

static BenchmarkStats calculate_difference_stats(const double* first, const double* second)
{
    double differences[MEASURED_RUNS];
    for (int i = 0; i < MEASURED_RUNS; i++)
        differences[i] = first[i] - second[i];
    return calculate_stats(differences);
}

static void print_samples(const char* label, const char* name, const double* samples)
{
    printf("%s:%s", label, name);
    for (int i = 0; i < MEASURED_RUNS; i++)
        printf(",%.9f", samples[i]);
    printf("\n");
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

static double measure_indexed_vector_operation(const COO_ColumnIndex* index, const COO* vector, int repetitions)
{
    double start = monotonic_time_ms();
    for (int i = 0; i < repetitions; i++) {
        COO* result = multiplication_matrix_and_vector_coo_indexed(index, vector);
        if (!result)
            return -1.0;
        free_matrix(result);
    }
    return (monotonic_time_ms() - start) / repetitions;
}

static double measure_column_index_build(const COO* matrix, int repetitions)
{
    double start = monotonic_time_ms();
    for (int i = 0; i < repetitions; i++) {
        COO_ColumnIndex* index = create_coo_column_index(matrix);
        if (!index)
            return -1.0;
        free_coo_column_index(index);
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

static int choose_indexed_vector_repetitions(const COO_ColumnIndex* index, const COO* vector)
{
    int repetitions = 1;
    while (repetitions < (1 << 20)) {
        double average = measure_indexed_vector_operation(index, vector, repetitions);
        if (average < 0.0 || average * repetitions >= MIN_SAMPLE_DURATION_MS)
            break;
        repetitions *= 2;
    }
    return repetitions;
}

static int choose_index_build_repetitions(const COO* matrix)
{
    int repetitions = 1;
    while (repetitions < (1 << 20)) {
        double average = measure_column_index_build(matrix, repetitions);
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
    BenchmarkStats difference_stats = calculate_difference_stats(my_samples, cs_samples);
    printf("REPETITIONS:%s,%d,%d\n", name, my_repetitions, cs_repetitions);
    printf("RESULT_MY:%s,%d,%d,%.9f,%.9f,%.9f\n", name, a->nnz, my_result->nnz, my_stats.mean, my_stats.standard_deviation, my_stats.confidence_95);
    printf("RESULT_CS:%s,%d,%d,%.9f,%.9f,%.9f\n", name, a->nnz, get_cs_effective_nnz(cs_result), cs_stats.mean, cs_stats.standard_deviation, cs_stats.confidence_95);
    printf("RESULT_DIFFERENCE:%s,%.9f,%.9f,%.9f\n", name, difference_stats.mean, difference_stats.standard_deviation,
        difference_stats.confidence_95);
    print_samples("SAMPLES_MY", name, my_samples);
    print_samples("SAMPLES_CS", name, cs_samples);
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

    COO_ColumnIndex* column_index = create_coo_column_index(a);
    cs* ca = to_cs(a);
    cs* cv = to_cs(v);
    if (!column_index || !ca || !cv) {
        printf("index or cs conversion failed\n");
        free_coo_column_index(column_index);
        cs_spfree(ca);
        cs_spfree(cv);
        free_matrix(v);
        free_matrix(a);
        return;
    }

    for (int i = 0; i < WARMUP_RUNS; i++) {
        COO* warmup_baseline = multiplication_matrix_and_vector_coo(a, v);
        COO* warmup_indexed = multiplication_matrix_and_vector_coo_indexed(column_index, v);
        cs* warmup_cs = cs_multiply(ca, cv);
        if (!warmup_baseline || !warmup_indexed || !warmup_cs) {
            printf("warmup failed\n");
            free_matrix(warmup_baseline);
            free_matrix(warmup_indexed);
            cs_spfree(warmup_cs);
            free_coo_column_index(column_index);
            cs_spfree(ca);
            cs_spfree(cv);
            free_matrix(v);
            free_matrix(a);
            return;
        }
        free_matrix(warmup_baseline);
        free_matrix(warmup_indexed);
        cs_spfree(warmup_cs);
    }

    double baseline_samples[MEASURED_RUNS];
    double indexed_samples[MEASURED_RUNS];
    double index_build_samples[MEASURED_RUNS];
    double cs_samples[MEASURED_RUNS];
    int baseline_repetitions = choose_coo_repetitions(multiplication_matrix_and_vector_coo, a, v);
    int indexed_repetitions = choose_indexed_vector_repetitions(column_index, v);
    int index_build_repetitions = choose_index_build_repetitions(a);
    int cs_repetitions = choose_cs_repetitions(ca, cv);

    for (int i = 0; i < MEASURED_RUNS; i++) {
        baseline_samples[i] = measure_coo_operation(multiplication_matrix_and_vector_coo, a, v, baseline_repetitions);
        indexed_samples[i] = measure_indexed_vector_operation(column_index, v, indexed_repetitions);
        index_build_samples[i] = measure_column_index_build(a, index_build_repetitions);
        cs_samples[i] = measure_cs_operation(ca, cv, cs_repetitions);
        if (baseline_samples[i] < 0.0 || indexed_samples[i] < 0.0 || index_build_samples[i] < 0.0 || cs_samples[i] < 0.0) {
            printf("measurement failed\n");
            free_coo_column_index(column_index);
            cs_spfree(ca);
            cs_spfree(cv);
            free_matrix(v);
            free_matrix(a);
            return;
        }
    }

    COO* baseline_result = multiplication_matrix_and_vector_coo(a, v);
    COO* indexed_result = multiplication_matrix_and_vector_coo_indexed(column_index, v);
    cs* cs_result = cs_multiply(ca, cv);
    if (!baseline_result || !indexed_result || !cs_result) {
        printf("verification calculation failed\n");
        free_matrix(baseline_result);
        free_matrix(indexed_result);
        cs_spfree(cs_result);
        free_coo_column_index(column_index);
        cs_spfree(ca);
        cs_spfree(cv);
        free_matrix(v);
        free_matrix(a);
        return;
    }

    BenchmarkStats baseline_stats = calculate_stats(baseline_samples);
    BenchmarkStats indexed_stats = calculate_stats(indexed_samples);
    BenchmarkStats index_build_stats = calculate_stats(index_build_samples);
    BenchmarkStats cs_stats = calculate_stats(cs_samples);
    BenchmarkStats optimization_difference_stats = calculate_difference_stats(baseline_samples, indexed_samples);
    BenchmarkStats comparison_difference_stats = calculate_difference_stats(indexed_samples, cs_samples);
    printf("REPETITIONS:%s,%d,%d,%d,%d\n", name, baseline_repetitions, indexed_repetitions, index_build_repetitions, cs_repetitions);
    printf("RESULT_BASELINE:%s,%d,%d,%.9f,%.9f,%.9f\n", name, a->nnz, baseline_result->nnz, baseline_stats.mean,
        baseline_stats.standard_deviation, baseline_stats.confidence_95);
    printf("RESULT_INDEX_BUILD:%s,%d,%.9f,%.9f,%.9f\n", name, a->nnz, index_build_stats.mean,
        index_build_stats.standard_deviation, index_build_stats.confidence_95);
    printf("RESULT_MY:%s,%d,%d,%.9f,%.9f,%.9f\n", name, a->nnz, indexed_result->nnz, indexed_stats.mean,
        indexed_stats.standard_deviation, indexed_stats.confidence_95);
    printf("RESULT_CS:%s,%d,%d,%.9f,%.9f,%.9f\n", name, a->nnz, get_cs_effective_nnz(cs_result), cs_stats.mean,
        cs_stats.standard_deviation, cs_stats.confidence_95);
    printf("RESULT_OPTIMIZATION_DIFFERENCE:%s,%.9f,%.9f,%.9f\n", name, optimization_difference_stats.mean,
        optimization_difference_stats.standard_deviation, optimization_difference_stats.confidence_95);
    printf("RESULT_DIFFERENCE:%s,%.9f,%.9f,%.9f\n", name, comparison_difference_stats.mean,
        comparison_difference_stats.standard_deviation, comparison_difference_stats.confidence_95);
    print_samples("SAMPLES_BASELINE", name, baseline_samples);
    print_samples("SAMPLES_INDEX_BUILD", name, index_build_samples);
    print_samples("SAMPLES_MY", name, indexed_samples);
    print_samples("SAMPLES_CS", name, cs_samples);
    printf("VERIFY_BASELINE:%s,%s\n", name, results_equal(baseline_result, cs_result) ? "OK" : "MISMATCH");
    printf("VERIFY_INDEXED:%s,%s\n", name, results_equal(indexed_result, cs_result) ? "OK" : "MISMATCH");

    free_matrix(baseline_result);
    free_matrix(indexed_result);
    cs_spfree(cs_result);
    free_coo_column_index(column_index);
    cs_spfree(ca);
    cs_spfree(cv);
    free_matrix(v);
    free_matrix(a);
}

int main(int argc, char** argv)
{
    srand(42);
    printf("STATISTICS:mean,sample_standard_deviation,95_percent_student_t_confidence_half_width,n=%d\n", MEASURED_RUNS);

    if (argc == 3) {
        benchmark_matrix_multiply(argv[1], argv[2]);
        benchmark_matrix_vector(argv[1], argv[2]);
        return 0;
    }
    if (argc == 4) {
        if (strcmp(argv[3], "matrix") == 0)
            benchmark_matrix_multiply(argv[1], argv[2]);
        else if (strcmp(argv[3], "vector") == 0)
            benchmark_matrix_vector(argv[1], argv[2]);
        else {
            fprintf(stderr, "Unknown operation: %s\n", argv[3]);
            return 1;
        }
        return 0;
    }
    if (argc != 1) {
        fprintf(stderr, "Usage: %s [matrix.mtx name [matrix|vector]]\n", argv[0]);
        return 1;
    }

    const char* matrices[] = {
        "../matrices/dolphins.mtx",
        "../matrices/lesmis.mtx",
        "../matrices/polbooks.mtx",
        "../matrices/football.mtx",
        "../matrices/celegansneural.mtx",
        "../matrices/netscience.mtx",
        "../matrices/add20/add20.mtx",
        "../matrices/ca-GrQc/ca-GrQc.mtx",
        "../matrices/ca-HepTh/ca-HepTh.mtx",
        "../matrices/roadNet-PA/roadNet-PA.mtx",
        "../matrices/roadNet-TX/roadNet-TX.mtx",
        "../matrices/roadNet-CA/roadNet-CA.mtx"
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
        "ca-HepTh",
        "roadNet-PA",
        "roadNet-TX",
        "roadNet-CA"
    };

    int matrix_count = (int)(sizeof(matrices) / sizeof(matrices[0]));
    for (int i = 0; i < matrix_count; i++) {
        benchmark_matrix_multiply(matrices[i], names[i]);
        benchmark_matrix_vector(matrices[i], names[i]);
    }

    return 0;
}
