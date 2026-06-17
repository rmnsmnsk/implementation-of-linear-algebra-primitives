#pragma once
#include "COO.h"

COO* read_matrix_market(const char* filename);
void print_matrix_info(COO* matrix, const char* name);
