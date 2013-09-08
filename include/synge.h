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

#include <stdarg.h>
#include <stdio.h>
#include <mpfr.h>

#include "ohmic.h"

#ifndef __SYNGE_H__
#define __SYNGE_H__

#define SYNGE_FORMAT		"Rf"
#define SYNGE_PRECISION		1024
#define SYNGE_ROUND		GMP_RNDN

#define synge_printf(...)	mpfr_printf(__VA_ARGS__)
#define synge_fprintf(...)	mpfr_fprintf(__VA_ARGS__)
#define synge_sprintf(...)	mpfr_sprintf(__VA_ARGS__)
#define synge_snprintf(...)	mpfr_snprintf(__VA_ARGS__)
#define synge_vprintf(...)	mpfr_vprintf(__VA_ARGS__)
#define synge_vfprintf(...)	mpfr_vfprintf(__VA_ARGS__)
#define synge_vnprintf(...)	mpfr_vnprintf(__VA_ARGS__)

#if !defined(true) || !defined(false) || !defined(bool)
#	define bool int
#	define true 1
#	define false 0
#endif

typedef mpfr_t synge_t;

typedef struct {
	enum {
		SUCCESS,
		DIVIDE_BY_ZERO,
		MODULO_BY_ZERO,
		UNMATCHED_LEFT_PARENTHESIS,
		UNMATCHED_RIGHT_PARENTHESIS,
		UNKNOWN_TOKEN,
		INVALID_LEFT_OPERAND,
		INVALID_RIGHT_OPERAND,
		INVALID_DELETE,
		FUNCTION_WRONG_ARGC,
		OPERATOR_WRONG_ARGC,
		MISSING_IF,
		MISSING_ELSE,
		EMPTY_IF,
		EMPTY_ELSE,
		TOO_MANY_VALUES,
		EMPTY_STACK,
		NUM_OVERFLOW,
		RESERVED_VARIABLE,
		UNKNOWN_WORD,
		ERROR_FUNC_ASSIGNMENT,
		ERROR_DELETE,
		UNDEFINED,
		TOO_DEEP,
		UNKNOWN_ERROR
	} code;
	int position;
} error_code;

enum {
	dynamic = -1
};

typedef struct {
	enum {
		degrees,
		radians,
		gradians
	} mode;

	enum {
		simple,
		position,
		traceback,
	} error;

	enum {
		flexible,
		strict
	} strict;

	int precision;
} synge_settings;

typedef struct {
	/* hard-coded name and description strings */
	char *name;
	char *prototype;
	char *description;

	/* a function pointer with the same format as the mpfr_* functions */
	int (*get)();
} function;

typedef struct {
	char *name;
	char *description;
} word;

int synge_get_precision(synge_t); /* returns minimum decimal precision needed to print number */

synge_settings synge_get_settings(void); /* returns active settings */
void synge_set_settings(synge_settings); /* set active settings to given settings */

function *synge_get_function_list(void); /* returns list of available builtin functions */
ohm_t *synge_get_variable_list(void); /* returns list of variables */
ohm_t *synge_get_expression_list(void); /* returns list of user functions */
word *synge_get_constant_list(void); /* returns list of builtin constants (must be freed) */

char *synge_error_msg(error_code); /* returns a string which describes the error code (DO NOT FREE) */
char *synge_error_msg_pos(int, int); /* returns a string which describes the error code (DO NOT FREE) */

error_code synge_compute_string(char *, synge_t *); /* takes an infix-style string and runs it through the synge core */

/* returns true if the return code should be treated as a success, otherwise false */
#define synge_is_success_code(code) \
	(code == SUCCESS)

/* returns true if the return code and result should be ignored, otherwise false */
#define synge_is_ignore_code(code) \
	(code == EMPTY_STACK || code == ERROR_FUNC_ASSIGNMENT || code == ERROR_DELETE)

void synge_seed(unsigned int seed); /* seed synge's pseudorandom number generator */
void synge_start(void); /* run at program initiation -- assertion will fail if not run before using synge functions */
void synge_end(void); /* run at program termination -- memory WILL leak if not run at end */

void synge_reset_traceback(void); /* reset the traceback list */
#endif /* __SYNGE_H__ */
