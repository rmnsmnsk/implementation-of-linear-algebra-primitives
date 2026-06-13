#ifndef MATRIX_MARKET_H
#define MATRIX_MARKET_H

#include "COO.h"

COO* load_matrix_market(const char* filename);
void free_matrix_coo(COO* mat);

#endif
