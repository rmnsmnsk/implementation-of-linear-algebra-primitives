#include "matrix.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

COO* read_matrix_market(const char* filename)
{
    if (!filename)
        return NULL;

    FILE* file = fopen(filename, "r");
    if (!file)
        return NULL;

    char line[2048];
    int rows = 0, cols = 0, nnz = 0;
    int header_found = 0;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '%')
            continue;

        if (strlen(line) < 2)
            continue;

        if (!header_found) {
            if (sscanf(line, "%d %d %d", &rows, &cols, &nnz) == 3) {
                header_found = 1;
                break;
            }
        }
    }

    if (!header_found || rows <= 0 || cols <= 0 || nnz <= 0) {
        fclose(file);
        return NULL;
    }

    COO* m = malloc(sizeof(COO));
    if (!m) {
        fclose(file);
        return NULL;
    }

    m->rows = rows;
    m->columns = cols;
    m->nnz = nnz;

    m->rows_indices = malloc(sizeof(int) * nnz);
    m->coll_indices = malloc(sizeof(int) * nnz);
    m->values = malloc(sizeof(float) * nnz);

    if (!m->rows_indices || !m->coll_indices || !m->values) {
        free(m->rows_indices);
        free(m->coll_indices);
        free(m->values);
        free(m);
        fclose(file);
        return NULL;
    }

    int idx = 0;
    while (fgets(line, sizeof(line), file) && idx < nnz) {
        if (line[0] == '%')
            continue;

        if (strlen(line) < 2)
            continue;

        int r, c;
        float v;
        int parsed = sscanf(line, "%d %d %f", &r, &c, &v);

        if (parsed >= 2) {
            m->rows_indices[idx] = r - 1;
            m->coll_indices[idx] = c - 1;
            m->values[idx] = (parsed == 3) ? v : 1.0f;
            idx++;
        }
    }

    fclose(file);
    m->nnz = idx;
    return m;
}

void print_matrix_info(COO* m, const char* name)
{
    if (!m || !name)
        return;
    printf("Matrix: %s\n", name);
    printf("Size: %d x %d\n", m->rows, m->columns);
    printf("NNZ: %d\n", m->nnz);
    if (m->rows > 0 && m->columns > 0) {
        double density = 100.0 * (double)m->nnz / (m->rows * m->columns);
        printf("Density: %.6f%%\n", density);
    }
}
