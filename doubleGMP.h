#ifndef _DOUBLE_GMP_HEADER_
#define _DOUBLE_GMP_HEADER_

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

// Ersatz fuer GMP, falls kein GMP auf der Plattform vorhanden ist

typedef long double* mpf_t[1];

void mpf_init2(mpf_t number, int bits);
void mpf_set_d(mpf_t number, long double value);
void mpf_set_prec(mpf_t number, int prec);
void mpf_sub(mpf_t result, mpf_t number1, mpf_t number2);
void mpf_mul(mpf_t result, mpf_t number1, mpf_t number2);
void mpf_div(mpf_t result, mpf_t number1, mpf_t number2);
void mpf_add(mpf_t result, mpf_t number1, mpf_t number2);
void mpf_div_ui(mpf_t result, mpf_t number, uint32_t inum);
void mpf_mul_ui(mpf_t result, mpf_t number, uint32_t inum);
void mpf_set(mpf_t result, mpf_t number);
int mpf_cmp_ui(mpf_t number, uint32_t inum);
void mpf_clear(mpf_t number);
unsigned long mpf_get_prec(mpf_t number);
void mpf_inp_str(mpf_t number, FILE *file, int base);
void mpf_out_str(FILE *file, int base, int mant, mpf_t number);

#endif