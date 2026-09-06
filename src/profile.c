#include "COO.h"
#include "matrix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static COO* create_profile_vector(int size)
{
    int nnz = size / 10;
    if (nnz < 1)
        nnz = 1;

    COO* vector = malloc(sizeof(COO));
    if (!vector)
        return NULL;

    vector->rows = size;
    vector->columns = 1;
    vector->nnz = nnz;
    vector->rows_indices = malloc(sizeof(int) * nnz);
    vector->coll_indices = calloc((size_t)nnz, sizeof(int));
    vector->values = malloc(sizeof(float) * nnz);
    if (!vector->rows_indices || !vector->coll_indices || !vector->values) {
        free_matrix(vector);
        return NULL;
    }

    for (int i = 0; i < nnz; i++) {
        vector->rows_indices[i] = i * 10;
        vector->values[i] = 1.0f + (float)(i % 10);
    }
    return vector;
}

int main(int argc, char** argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <matrix.mtx> <matrix|vector> <repetitions>\n", argv[0]);
        return 1;
    }

    char* end = NULL;
    long repetitions = strtol(argv[3], &end, 10);
    if (!end || *end != '\0' || repetitions <= 0) {
        fprintf(stderr, "Invalid repetition count: %s\n", argv[3]);
        return 1;
    }

    COO* matrix = read_matrix_market(argv[1]);
    if (!matrix || !sort_matrix(matrix)) {
        fprintf(stderr, "Failed to load matrix: %s\n", argv[1]);
        free_matrix(matrix);
        return 1;
    }

    COO* vector = NULL;
    if (strcmp(argv[2], "vector") == 0) {
        vector = create_profile_vector(matrix->columns);
        if (!vector) {
            fprintf(stderr, "Failed to create vector\n");
            free_matrix(matrix);
            return 1;
        }
    } else if (strcmp(argv[2], "matrix") != 0) {
        fprintf(stderr, "Unknown operation: %s\n", argv[2]);
        free_matrix(matrix);
        return 1;
    }

    volatile long long checksum = 0;
    coo_profile_reset();
    for (long iteration = 0; iteration < repetitions; iteration++) {
        COO* result = vector ? multiplication_matrix_and_vector_coo(matrix, vector) : multiplication_two_matrix(matrix, matrix);
        if (!result) {
            fprintf(stderr, "Operation failed at iteration %ld\n", iteration);
            free_matrix(vector);
            free_matrix(matrix);
            return 1;
        }
        checksum += result->nnz;
        free_matrix(result);
    }

    COO_Profile profile = coo_profile_get();
    printf("PROFILE:%s,%s,%ld,%lld\n", argv[1], argv[2], repetitions, checksum);
    printf("TOTAL_MS:%.6f\n", profile.total_ms);
    printf("SORT_MS:%.6f\n", profile.sort_ms);
    printf("WORKSPACE_MS:%.6f\n", profile.workspace_ms);
    printf("INDEX_MS:%.6f\n", profile.index_ms);
    printf("ACCUMULATION_MS:%.6f\n", profile.accumulation_ms);
    printf("BUFFER_SCAN_MS:%.6f\n", profile.buffer_scan_ms);
    printf("RESULT_MS:%.6f\n", profile.result_ms);
    printf("CLEANUP_MS:%.6f\n", profile.cleanup_ms);
    printf("LOOKUP_CALLS:%llu\n", profile.lookup_calls);
    printf("LOOKUP_COMPARISONS:%llu\n", profile.lookup_comparisons);
    free_matrix(vector);
    free_matrix(matrix);
    return 0;
}
