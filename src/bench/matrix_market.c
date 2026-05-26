#include <stdio.h>
#include <stdlib.h>
#include "matrix_market.h"

COO *load_matrix_market(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", filename);
        return NULL;
    }

    char line[256];
    // Пропускаем строки комментариев (начинаются с %)
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != '%') break;
    }

    // Читаем размеры: rows cols nnz
    int rows, cols, nnz;
    if (sscanf(line, "%d %d %d", &rows, &cols, &nnz) != 3) {
        fclose(f);
        return NULL;
    }

    COO *mat = malloc(sizeof(COO));
    mat->rows = rows;
    mat->columns = cols;
    mat->nnz = nnz;
    mat->rows_indices = malloc(sizeof(int) * nnz);
    mat->coll_indices = malloc(sizeof(int) * nnz);
    mat->values = malloc(sizeof(float) * nnz);

    for (int i = 0; i < nnz; i++) {
        int r, c;
        float v;
        fscanf(f, "%d %d %f", &r, &c, &v);
        // Matrix Market использует 1-based indexing → переводим в 0-based
        mat->rows_indices[i] = r - 1;
        mat->coll_indices[i] = c - 1;
        mat->values[i] = v;
    }

    fclose(f);
    return mat;
}

void free_matrix_coo(COO *mat) {
    if (mat) {
        free(mat->rows_indices);
        free(mat->coll_indices);
        free(mat->values);
        free(mat);
    }
}
