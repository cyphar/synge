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

#ifndef __SYNGE_H__
#define __SYNGE_H__

#define SYNGE_MAIN "<main>"
#define SYNGE_FORMAT "Lf"

typedef long double synge_t;

typedef struct {
	enum {
		SUCCESS,
		DIVIDE_BY_ZERO,
		MODULO_BY_ZERO,
		UNMATCHED_LEFT_PARENTHESIS,
		UNMATCHED_RIGHT_PARENTHESIS,
		UNKNOWN_TOKEN,
		UNKNOWN_VARIABLE,
		UNKNOWN_FUNCTION,
		FUNCTION_WRONG_ARGC,
		OPERATOR_WRONG_ARGC,
		TOO_MANY_VALUES,
		EMPTY_STACK,
		NUM_OVERFLOW,
		INVALID_VARIABLE_CHAR,
		EMPTY_VARIABLE_NAME,
		RESERVED_VARIABLE,
		DELETED_VARIABLE,
		DELETED_FUNCTION,
		ERROR_FUNC_ASSIGNMENT,
		UNDEFINED,
		TOO_DEEP,
		UNKNOWN_ERROR
	} code;
	int position;
} error_code;

typedef struct __synge_settings__ {
	enum {
		degrees,
		radians
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

	enum {
		dynamic = -1
	} precision;
} synge_settings;

typedef struct __function__ {
	char *name;
	synge_t (*get)(synge_t);
	/* to hard-code explanations and name strings */
	char *prototype;
	char *description;
} function;

int get_precision(synge_t); /* returns minimum decimal precision needed to print number */

synge_settings get_synge_settings(void); /* returns active settings */
function *get_synge_function_list(void); /* returns list of available functions */
void set_synge_settings(synge_settings); /* set active settings to given settings */

char *get_error_msg(error_code); /* returns a string which describes the error code (DO NOT FREE) */
char *get_error_msg_pos(int, int); /* returns a string which describes the error code (DO NOT FREE) */

error_code internal_compute_infix_string(char *, synge_t *, char *, int); /* takes an infix-style string and runs it through the "engine" */
#define compute_infix_string(...) internal_compute_infix_string(__VA_ARGS__, SYNGE_MAIN, 0) 

int is_success_code(int); /* returns true if the return code should be treated as a success, otherwise false */
int ignore_code(int); /* returns true if the return code and result should be ignored, otherwise false */

void synge_start(void); /* run at program initiation -- assertion will fail if not run before using synge functions */
void synge_end(void); /* run at program termination -- memory WILL leak if not run at end */

void synge_reset_traceback(void); /* reset the traceback list */
#endif /* __SYNGE_H__ */
