#include "doubleGMP.h"

void mpf_init2(mpf_t number, int bits) {
	number[0] = calloc(1, sizeof(long double));
	*number[0] = 0.0;
}

void mpf_set_d(mpf_t number, long double value) {
	*number[0] = value;
}

void mpf_set_prec(mpf_t number, int prec) {
}

void mpf_sub(mpf_t result, mpf_t number1, mpf_t number2) {
	*result[0] = (*number1[0])-(*number2[0]);
}

void mpf_mul(mpf_t result, mpf_t number1, mpf_t number2) {
	*result[0] = (*number1[0])*(*number2[0]);
}

void mpf_div(mpf_t result, mpf_t number1, mpf_t number2) {
	*result[0] = (*number1[0])/(*number2[0]);
}

void mpf_add(mpf_t result, mpf_t number1, mpf_t number2) {
	*result[0] = (*number1[0])+(*number2[0]);
}

void mpf_div_ui(mpf_t result, mpf_t number, uint32_t inum) {
	long double num = inum;
	*result[0] = (*number[0])/num;
}

void mpf_mul_ui(mpf_t result, mpf_t number, uint32_t inum) {
	*result[0] = (*number[0])*inum;
}

void mpf_set(mpf_t result, mpf_t number) {
	*result[0] = *number[0];
}

int mpf_cmp_ui(mpf_t number, uint32_t inum) {
	return (*number[0]) - inum;
}

void mpf_clear(mpf_t number) {
	free(number[0]);
}

unsigned long mpf_get_prec(mpf_t number) {
	return sizeof(*number)*8;
}

void mpf_inp_str(mpf_t number, FILE *file, int base) {
}

void mpf_out_str(FILE *file, int base, int mant, mpf_t number) {
}