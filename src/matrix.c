#include "matrix.h"
#include <ctype.h>
#include <limits.h>
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
    int symmetric = 0;

    if (fgets(line, sizeof(line), file)) {
        char banner[64], object[64], format[64], field[64], symmetry[64];
        if (sscanf(line, "%63s %63s %63s %63s %63s", banner, object, format, field, symmetry) == 5 && strcmp(banner, "%%MatrixMarket") == 0) {
            symmetric = strcmp(symmetry, "symmetric") == 0;
        } else {
            if (fseek(file, 0, SEEK_SET) != 0) {
                fclose(file);
                return NULL;
            }
        }
    }

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
    if (symmetric && nnz > INT_MAX / 2) {
        free(m);
        fclose(file);
        return NULL;
    }

    int capacity = symmetric ? nnz * 2 : nnz;
    m->nnz = 0;

    m->rows_indices = malloc(sizeof(int) * capacity);
    m->coll_indices = malloc(sizeof(int) * capacity);
    m->values = malloc(sizeof(float) * capacity);

    if (!m->rows_indices || !m->coll_indices || !m->values) {
        free(m->rows_indices);
        free(m->coll_indices);
        free(m->values);
        free(m);
        fclose(file);
        return NULL;
    }

    int idx = 0;
    int stored_entries = 0;
    while (fgets(line, sizeof(line), file) && stored_entries < nnz) {
        if (line[0] == '%')
            continue;

        if (strlen(line) < 2)
            continue;

        int r, c;
        float v;
        int parsed = sscanf(line, "%d %d %f", &r, &c, &v);

        if (parsed >= 2) {
            int row = r - 1;
            int column = c - 1;
            float value = (parsed == 3) ? v : 1.0f;

            if (row < 0 || row >= rows || column < 0 || column >= cols) {
                free(m->rows_indices);
                free(m->coll_indices);
                free(m->values);
                free(m);
                fclose(file);
                return NULL;
            }

            m->rows_indices[idx] = row;
            m->coll_indices[idx] = column;
            m->values[idx] = value;
            idx++;
            if (symmetric && row != column) {
                m->rows_indices[idx] = column;
                m->coll_indices[idx] = row;
                m->values[idx] = value;
                idx++;
            }
            stored_entries++;
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
