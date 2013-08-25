/* Synge: A shunting-yard calculation "engine"
 * Copyright (C) 2013 Cyphar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __INTERNAL_H__
#define __INTERNAL_H__

#include "synge.h"

typedef struct {
	char *alias;
	char *function;
} function_alias;

typedef struct {
	char *name;
	char *description;
	/* an mpfr_* like function to set the value of the special number */
	int (*value)();
} special_number;

/* internal stack types */
typedef enum __stack_type__ {
	number,

	setop	=  1,
	modop	=  2,
	compop	=  3,
	bitop	=  4,
	addop	=  5,
	multop	=  6,
	signop	=  7,
	preop	=  8,
	expop	=  9,
	ifop	= 10,
	elseop	= 11,

	/* not treated as operator in parsing */
	delop,
	premod,
	postmod,

	func,
	userword, /* user function or variable */
	setword, /* user function or variable to be set */
	expression, /* saved expression */

	lparen,
	rparen,
} s_type;

typedef struct operator {
	char *str;
	enum {
		op_add,
		op_subtract,
		op_multiply,
		op_divide,
		op_int_divide,
		op_modulo,
		op_index,

		op_lparen,
		op_rparen,

		op_gt,
		op_gteq,
		op_lt,
		op_lteq,
		op_neq,
		op_eq,

		op_band,
		op_bor,
		op_bxor,
		op_binv,
		op_bnot,
		op_bshiftl,
		op_bshiftr,

		op_if,
		op_else,

		op_var_set,
		op_func_set,
		op_del,

		op_ca_add,
		op_ca_subtract,
		op_ca_multiply,
		op_ca_divide,
		op_ca_int_divide,
		op_ca_modulo,
		op_ca_index,

		op_ca_band,
		op_ca_bor,
		op_ca_bxor,
		op_ca_bshiftl,
		op_ca_bshiftr,

		op_ca_increment,
		op_ca_decrement,

		op_none
	} tp;
} operator;

error_code synge_internal_compute_string(char *, synge_t *, char *, int);

#endif
