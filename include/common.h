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

#include "synge.h"
#include "stack.h"

#ifndef COMMON_H
#define COMMON_H

/* value macros */
#define str(x)					#x
#define mstr(x)					str(x)
#define len(x)					(sizeof(x) / sizeof(*x))

/* inbuilt caller names */
#define SYNGE_MAIN				"<main>"
#define SYNGE_IF				"<if>"
#define SYNGE_ELSE				"<else>"

/* internal "magic numbers" */
#define SYNGE_MAX_PRECISION		64
#define SYNGE_MAX_DEPTH			2048
#define SYNGE_EPSILON			"1e-" mstr(SYNGE_MAX_PRECISION + 1)

/* word-related things */
#define SYNGE_PREV_ANSWER		"ans"
#define SYNGE_PREV_EXPRESSION	"_"
#define SYNGE_WORD_CHARS		"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ\'\"_"
#define SYNGE_FUNCTION_CHARS	"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ0123456789\'\"_"

/* traceback format macros */
#define SYNGE_TRACEBACK_FORMAT	"Synge Traceback (most recent call last):\n" \
								"%s" \
								"%s: %s"

#define SYNGE_TRACEBACK_MODULE		"  Module %s\n"
#define SYNGE_TRACEBACK_CONDITIONAL	"  %s condition, at %d\n"
#define SYNGE_TRACEBACK_FUNCTION	"  Function %s, at %d\n"

/* in-place macros */
#define assert(cond, reason)	do { if(!(cond)) { fprintf(stderr, "synge: assertion '%s' (%s) failed\n", reason, #cond); abort(); }} while(0)

/* useful macros */
#define issignop(str) (get_op(str).tp == op_add || get_op(str).tp == op_subtract)

#define isaddop(str) (get_op(str).tp == op_add || get_op(str).tp == op_subtract)
#define issetop(str) (get_op(str).tp == op_var_set || get_op(str).tp == op_func_set)
#define isdelop(str) (get_op(str).tp == op_del)

#define iscreop(str) (get_op(str).tp == op_ca_decrement || get_op(str).tp == op_ca_increment)
#define isparenop(str) (get_op(str).tp == op_lparen || get_op(str).tp == op_rparen)

#define ismodop(str) (get_op(str).tp == op_ca_add || get_op(str).tp == op_ca_subtract || \
		      get_op(str).tp == op_ca_multiply || get_op(str).tp == op_ca_divide || get_op(str).tp == op_ca_int_divide || get_op(str).tp == op_ca_modulo || \
		      get_op(str).tp == op_ca_index || \
		      get_op(str).tp == op_ca_band || get_op(str).tp == op_ca_bor || get_op(str).tp == op_ca_bxor || \
		      get_op(str).tp == op_ca_bshiftl || get_op(str).tp == op_ca_bshiftr)

#define isparen(type) (type == lparen || type == rparen)
#define isop(type) (type == addop || type == signop || type == multop || type == expop || type == compop || type == bitop || type == setop)
#define isterm(type) (type == number || type == userword || type == setword || type == rparen || type == postmod || type == premod)
#define isnumword(type) (type == number || type == userword || type == setword)
#define isword(type) (type == userword || type == setword)

/* get amount of memory needed to store a sprintf() -- including null terminator */
#define lenprintf(...) (synge_snprintf(NULL, 0, __VA_ARGS__) + 1)

/* macros for casting void stack pointers */
#define SYNGE_T(x) (*(synge_t *) x)
#define FUNCTION(x) ((struct synge_func *) x)

struct synge_const {
	char *name;
	char *description;
	/* an mpfr_* like function to set the value of the special number */
	int (*value)();
};

/* internal stack types */
enum s_type {
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
};

struct synge_op {
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
};


#define debug(...) do { _debug("%s: ", __func__); _debug(__VA_ARGS__); } while(0)
void _debug(char *, ...);

#define print_stack(stack) _print_stack((char *) __func__, stack)
void _print_stack(char *, struct stack *);

void cheeky(char *, ...);
struct synge_err to_error_code(int, int);

synge_t *num_dup(synge_t);
char *str_dup(char *);
struct synge_op get_op(char *);
struct synge_const get_special_num(char *);

void synge_clear(void *);
char *get_word(char *, char *, char **);
bool contains_word(char *, char *, char *);
char *trim_spaces(char *);

int iszero(synge_t);
int deg_to_rad(synge_t, synge_t, mpfr_rnd_t);
int deg_to_grad(synge_t, synge_t, mpfr_rnd_t);
int grad_to_deg(synge_t, synge_t, mpfr_rnd_t);
int grad_to_rad(synge_t, synge_t, mpfr_rnd_t);
int rad_to_deg(synge_t, synge_t, mpfr_rnd_t);
int rad_to_grad(synge_t, synge_t, mpfr_rnd_t);

struct synge_err synge_lex_string(char *, struct stack **);
struct synge_err synge_infix_parse(struct stack **, struct stack **);
struct synge_err synge_eval_rpnstack(struct stack **, synge_t *);
struct synge_err synge_internal_compute_string(char *, synge_t *, char *, int);

#endif
