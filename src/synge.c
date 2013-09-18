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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>

#include "stack.h"
#include "synge.h"
#include "internal.h"
#include "ohmic.h"
#include "linked.h"

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
#define FUNCTION(x) ((function *) x)

/* global state variables */
static int synge_started = false;
static gmp_randstate_t synge_state;

/* variables and functions */
static ohm_t *variable_list = NULL;
static ohm_t *expression_list = NULL;
static synge_t prev_answer;

/* traceback */
static char *error_msg_container = NULL;
static link_t *traceback_list = NULL;

static void print_stack(stack *s) {
#ifdef __SYNGE_DEBUG__
	int i, size = stack_size(s);
	for(i = 0; i < size; i++) {
		s_content tmp = s->content[i];

		if(!tmp.val)
			continue;

		switch(tmp.tp) {
			case number:
				synge_fprintf(stderr, "%.*" SYNGE_FORMAT " ", synge_get_precision(SYNGE_T(tmp.val)), SYNGE_T(tmp.val));
				break;
			case func:
				fprintf(stderr, "%s ", FUNCTION(tmp.val)->name);
				break;
			default:
				fprintf(stderr, "'%s' ", (char *) tmp.val);
				break;
		}
	}
	fprintf(stderr, "\n");
#endif
} /* print_stack() */

static void debug(char *format, ...) {
#ifdef __SYNGE_DEBUG__
	va_list ap;
	va_start(ap, format);
	synge_vfprintf(stderr, format, ap);
	va_end(ap);
#endif
} /* debug() */

static void cheeky(char *format, ...) {
#if defined(__SYNGE_CHEEKY__) && __SYNGE_CHEEKY__ > 0
	va_list ap;
	va_start(ap, format);
	synge_vfprintf(stderr, format, ap);
	va_end(ap);
#endif
} /* cheeky() */

/* for windows, define strcasecmp and strncasecmp */
#ifdef _WINDOWS
int strcasecmp(char *s1, char *s2) {
	while (tolower(*s1) == tolower(*s2)) {
		if (*s1 == '\0' || *s2 == '\0')
			break;
		s1++;
		s2++;
	}

	return tolower(*s1) - tolower(*s2);
} /* strcasecmp() */

int strncasecmp(char *s1, char *s2, size_t n) {
	if (n == 0)
		return 0;

	while (n-- != 0 && tolower(*s1) == tolower(*s2)) {
		if (n == 0 || *s1 == '\0' || *s2 == '\0')
			break;
		s1++;
		s2++;
	}

	return tolower(*s1) - tolower(*s2);
} /* strncasecmp() */
#endif

static void synge_clear(void *tofree) {
	synge_t *item = tofree;
	mpfr_clear(SYNGE_T(item));
} /* synge_clear() */

static int iszero(synge_t num) {
	synge_t epsilon;

	/* generate "epsilon" */
	mpfr_init2(epsilon, SYNGE_PRECISION);
	mpfr_set_str(epsilon, SYNGE_EPSILON, 10, SYNGE_ROUND);

	/* if abs(num) < epsilon then it is zero */
	int cmp = mpfr_cmpabs(num, epsilon);
	mpfr_clears(epsilon, NULL);

	return (cmp < 0 || mpfr_zero_p(num));
} /* iszero() */

static int deg_to_rad(synge_t rad, synge_t deg, mpfr_rnd_t round) {
	/* get pi */
	synge_t pi;
	mpfr_init2(pi, SYNGE_PRECISION);
	mpfr_const_pi(pi, round);

	/* get conversion for deg -> rad */
	synge_t from_deg;
	mpfr_init2(from_deg, SYNGE_PRECISION);
	mpfr_div_si(from_deg, pi, 180, round);

	/* convert it */
	mpfr_mul(rad, deg, from_deg, round);

	/* free memory associated with values */
	mpfr_clears(pi, from_deg, NULL);
	return 0;
} /* deg_to_rad() */

static int deg_to_grad(synge_t grad, synge_t deg, mpfr_rnd_t round) {
	/* get conversion for deg -> grad */
	synge_t from_deg;
	mpfr_init2(from_deg, SYNGE_PRECISION);
	mpfr_set_si(from_deg, 10, round);
	mpfr_div_si(from_deg, from_deg, 9, round);

	/* convert it */
	mpfr_mul(grad, deg, from_deg, round);

	/* free memory associated with values */
	mpfr_clears(from_deg, NULL);
	return 0;
} /* deg_to_grad() */

static int grad_to_deg(synge_t deg, synge_t grad, mpfr_rnd_t round) {
	/* get conversion for grad -> deg */
	synge_t from_grad;
	mpfr_init2(from_grad, SYNGE_PRECISION);
	mpfr_set_si(from_grad, 9, round);
	mpfr_div_si(from_grad, from_grad, 10, round);

	/* convert it */
	mpfr_mul(deg, grad, from_grad, round);

	/* free memory associated with values */
	mpfr_clears(from_grad, NULL);
	return 0;
} /* grad_to_deg() */

static int grad_to_rad(synge_t rad, synge_t grad, mpfr_rnd_t round) {
	/* get pi */
	synge_t pi;
	mpfr_init2(pi, SYNGE_PRECISION);
	mpfr_const_pi(pi, round);

	/* get conversion for grad -> rad */
	synge_t from_grad;
	mpfr_init2(from_grad, SYNGE_PRECISION);
	mpfr_div_si(from_grad, pi, 200, round);

	/* convert it */
	mpfr_mul(rad, grad, from_grad, round);

	/* free memory associated with values */
	mpfr_clears(pi, from_grad, NULL);
	return 0;
} /* grad_to_rad() */

static int rad_to_deg(synge_t deg, synge_t rad, mpfr_rnd_t round) {
	/* get pi */
	synge_t pi;
	mpfr_init2(pi, SYNGE_PRECISION);
	mpfr_const_pi(pi, round);

	/* get conversion for rad -> deg */
	synge_t from_rad;
	mpfr_init2(from_rad, SYNGE_PRECISION);
	mpfr_si_div(from_rad, 180, pi, round);

	/* convert it */
	mpfr_mul(deg, rad, from_rad, round);

	/* free memory associated with values */
	mpfr_clears(pi, from_rad, NULL);
	return 0;
} /* rad_to_deg() */

static int rad_to_grad(synge_t grad, synge_t rad, mpfr_rnd_t round) {
	/* get pi */
	synge_t pi;
	mpfr_init2(pi, SYNGE_PRECISION);
	mpfr_const_pi(pi, round);

	/* get conversion for rad -> grad */
	synge_t from_rad;
	mpfr_init2(from_rad, SYNGE_PRECISION);
	mpfr_si_div(from_rad, 200, pi, round);

	/* convert it */
	mpfr_mul(grad, rad, from_rad, round);

	/* free memory associated with values */
	mpfr_clears(pi, from_rad, NULL);
	return 0;
} /* rad_to_grad() */

static int sy_rand(synge_t to, synge_t number, mpfr_rnd_t round) {
	/* A = rand() -- 0 <= rand() < 1 */
	synge_t random;
	mpfr_init2(random, SYNGE_PRECISION);
	mpfr_urandomb(random, synge_state);

	/* rand(B) = rand() * B -- where 0 <= rand() < 1 */
	mpfr_mul(to, random, number, round);

	/* free memory */
	mpfr_clears(random, NULL);
	return 0;
} /* sy_rand() */

static int sy_int_rand(synge_t to, synge_t number, mpfr_rnd_t round) {
	/* round input */
	mpfr_floor(number, number);

	/* get random number */
	sy_rand(to, number, round);

	/* round output */
	mpfr_round(to, to);
	return 0;
} /* sy_int_rand() */

static int sy_factorial(synge_t to, synge_t num, mpfr_rnd_t round) {
	/* round input */
	synge_t number;
	mpfr_init2(number, SYNGE_PRECISION);
	mpfr_abs(number, num, round);
	mpfr_floor(number, number);

	/* multilply original number by reverse iterator */
	mpfr_set_si(to, 1, round);
	while(!iszero(number)) {
		mpfr_mul(to, to, number, round);
		mpfr_sub_si(number, number, 1, round);
	}

	mpfr_copysign(to, to, num, round);
	mpfr_clears(number, NULL);
	return 0;
} /* sy_factorial() */

static int sy_series(synge_t to, synge_t number, mpfr_rnd_t round) {
	/* round input */
	mpfr_floor(number, number);

	/* (x * (x + 1)) / 2 */
	mpfr_set(to, number, round);
	mpfr_add_si(number, number, 1, round);
	mpfr_mul(to, to, number, round);
	mpfr_div_si(to, to, 2, round);
	return 0;
} /* sy_series() */

static int sy_assert(synge_t to, synge_t check, mpfr_rnd_t round) {
	return mpfr_set_si(to, iszero(check) ? 0 : 1, round);
} /* sy_assert */

/* builtin function names, prototypes, descriptions and function pointers */
static function func_list[] = {
	{"abs",		"abs(x)",		"Absolute value of x",								mpfr_abs},
	{"sqrt",	"sqrt(x)",		"Square root of x",									mpfr_sqrt},
	{"cbrt",	"cbrt(x)",		"Cubic root of x",									mpfr_cbrt},

	{"floor",	"floor(x)",		"Largest integer not greater than x",				mpfr_floor},
	{"round",	"round(x)",		"Closest integer to x",								mpfr_round},
	{"ceil",	"ceil(x)",		"Smallest integer not smaller than x",				mpfr_ceil},

	{"log10",	"log10(x)",		"Base 10 logarithm of x",							mpfr_log10},
	{"log",		"log(x)",		"Base 2 logarithm of x",							mpfr_log2},
	{"ln",		"ln(x)",		"Base e logarithm of x",							mpfr_log},

	{"rand",	"rand(x)",		"Generate a pseudo-random number between 0 and x",	sy_rand},
	{"randi",	"randi(x)",		"Generate a pseudo-random integer between 0 and x",	sy_int_rand},

	{"fact",	"fact(x)",		"Factorial of round(x)",							sy_factorial},
	{"series",	"series(x)",	"Gives addition of all integers up to x",			sy_series},
	{"assert",	"assert(x)",	"Returns 0 is x is 0, and returns 1 otherwise",		sy_assert},

	{"deg_to_rad",	"deg_to_rad(x)",	"Convert x degrees to radians",   			deg_to_rad},
	{"deg_to_grad",	"deg_to_grad(x)",	"Convert x degrees to gradians",   			deg_to_grad},

	{"rad_to_deg",	"rad_to_deg(x)",	"Convert x radians to degrees",   			rad_to_deg},
	{"rad_to_grad",	"rad_to_grad(x)",	"Convert x radians to gradians",   			rad_to_grad},

	{"grad_to_deg",	"grad_to_deg(x)",	"Convert x gradians to degrees",   			grad_to_deg},
	{"grad_to_rad",	"grad_to_rad(x)",	"Convert x gradians to radians",   			grad_to_rad},

	{"asinh",	"asinh(x)",		"Inverse hyperbolic sine of x",						mpfr_asinh},
	{"acosh",	"acosh(x)",		"Inverse hyperbolic cosine of x",					mpfr_acosh},
	{"atanh",	"atanh(x)",		"Inverse hyperbolic tangent of x",					mpfr_atanh},
	{"sinh",	"sinh(x)",		"Hyperbolic sine of x",								mpfr_sinh},
	{"cosh",	"cosh(x)",		"Hyperbolic cosine of x",							mpfr_cosh},
	{"tanh",	"tanh(x)",		"Hyperbolic tangent of x",							mpfr_tanh},

	{"asin",	"asin(x)",		"Inverse sine of x",								mpfr_asin},
	{"acos",	"acos(x)",		"Inverse cosine of x",								mpfr_acos},
	{"atan",	"atan(x)",		"Inverse tangent of x",								mpfr_atan},
	{"sin",		"sin(x)",		"Sine of x",										mpfr_sin},
	{"cos",		"cos(x)",		"Cosine of x",										mpfr_cos},
	{"tan",		"tan(x)",		"Tangent of x",										mpfr_tan},
	{NULL,		NULL,			NULL,												NULL}
};

static int synge_pi(synge_t num, mpfr_rnd_t round) {
	mpfr_const_pi(num, round);
	return 0;
} /* synge_pi() */

static int synge_phi(synge_t num, mpfr_rnd_t round) {
	/* get sqrt(5) */
	synge_t root_five;
	mpfr_init2(root_five, SYNGE_PRECISION);
	mpfr_sqrt_ui(root_five, 5, round);

	/* (1 + sqrt(5)) / 2 */
	mpfr_add_si(num, root_five, 1, round);
	mpfr_div_si(num, num, 2, round);

	mpfr_clears(root_five, NULL);
	return 0;
} /* synge_phi() */

static int synge_euler(synge_t num, mpfr_rnd_t round) {
	/* get one */
	synge_t one;
	mpfr_init2(one, SYNGE_PRECISION);
	mpfr_set_si(one, 1, round);

	/* e^1 */
	mpfr_exp(num, one, round);

	mpfr_clears(one, NULL);
	return 0;
} /* synge_euler() */

static int synge_life(synge_t num, mpfr_rnd_t round) {
	cheeky("What do you get if you multiply six by nine? Fourty two.\n");
	mpfr_set_si(num, 42, round);
	return 0;
} /* synge_life() */

static int synge_true(synge_t num, mpfr_rnd_t round) {
	mpfr_set_si(num, 1, round);
	return 0;
} /* synge_true() */

static int synge_false(synge_t num, mpfr_rnd_t round) {
	mpfr_set_si(num, 0, round);
	return 0;
} /* synge_false() */

static int synge_ans(synge_t num, mpfr_rnd_t round) {
	mpfr_set(num, prev_answer, round);
	return 0;
} /* synge_ans() */

/* inbuilt constants (given using function pointers) */
static special_number constant_list[] = {
	{"pi",				"The ratio of a circle's circumfrence to its diameter",				synge_pi},
	{"phi",				"The golden ratio",													synge_phi},
	{"e",				"The base of a natural logarithm",									synge_euler},
	{"life",			"The answer to the question of life, the universe and everything",	synge_life}, /* Sorry, I couldn't resist */

	{"true",			"Standard true value",												synge_true},
	{"false",			"Standard false value",												synge_false},

	{SYNGE_PREV_ANSWER,	"Gives the last successful answer",									synge_ans},
	{NULL,				NULL,																NULL},
};

/* used for when a (char *) is needed, but needn't be freed and *
 * converts the string into switch-friendly enumeration values. */
static operator op_list[] = {
	{"+",	op_add},
	{"-",	op_subtract},
	{"*",	op_multiply},
	{"/",	op_divide},
	{"//",	op_int_divide}, /* integer division */
	{"%",	op_modulo},
	{"^",	op_index},

	{"(",	op_lparen},
	{")",	op_rparen},

	/* comparison operators */
	{">",	op_gt},
	{">=",	op_gteq},

	{"<",	op_lt},
	{"<=",	op_lteq},

	{"!=",	op_neq},
	{"==",	op_eq},

	/* bitwise operators */
	{"&",	op_band},
	{"|",	op_bor},
	{"#",	op_bxor},

	{"~",	op_binv},
	{"!",	op_bnot},

	{"<<",	op_bshiftl},
	{">>",	op_bshiftr},

	/* tertiary operators */
	{"?",	op_if},
	{":",	op_else},

	/* assignment operators */
	{"=",	op_var_set},
	{":=",	op_func_set},
	{"::",	op_del},

	/* compound assignment operators */
	{"+=",	op_ca_add},
	{"-=",	op_ca_subtract},
	{"*=",	op_ca_multiply},
	{"/=",	op_ca_divide},
	{"//=",	op_ca_int_divide}, /* integer division */
	{"%=",	op_ca_modulo},
	{"^=",	op_ca_index},
	{"&=",	op_ca_band},
	{"|=",	op_ca_bor},
	{"#=",	op_ca_bxor},
	{"<<=",	op_ca_bshiftl},
	{">>=",	op_ca_bshiftr},

	/* prefix/postfix compound assignment operators */
	{"++",	op_ca_increment},
	{"--",	op_ca_decrement},

	/* null terminator */
	{NULL,	op_none}
};

/* default settings */
static synge_settings active_settings = {
	degrees, /* mode */
	position, /* error level */
	strict, /* strictness */
	dynamic /* precision */
};

static char *get_word_ptr(char *string, char *list) {
	int lenstr = strlen(string), lenlist = strlen(list);

	/* go through string */
	int i;
	for(i = 0; i < lenstr; i++) {
		bool found = false;

		/* check current character against allowed character "list" */
		int j;
		for(j = 0; j < lenlist; j++) {
			/* match found */
			if(string[i] == list[j]) {
				found = true;
				break;
			}
		}

		/* current character not in string -- end of word */
		if(!found)
			break;
	}

	/* found end of word */
	return string + i;
} /* get_word_ptr() */

static char *get_word(char *string, char *list, char **endptr) {
	/* get word pointer */
	*endptr = get_word_ptr(string, list);
	int i = *endptr - string;

	/* no word found */
	if(!i)
		return NULL;

	/* copy over the word */
	char *ret = malloc(i + 1);
	memcpy(ret, string, i);
	ret[i] = '\0';

	return ret;
} /* get_word() */

static bool contains_word(char *string, char *word, char *list) {
	int len = strlen(word);
	char *p = string, *cur = NULL;

	do {
		/* word found */
		if(!strncmp(p, word, len))
			return true;

		/* "backup" pointer */
		cur = p;
	} while((p = get_word_ptr(p, list)) != cur || *(p++) != '\0');

	/* word not found */
	return false;
} /* contains_word() */

static char *trim_spaces(char *str) {
	/* move starting pointer to first non-space character */
	while(isspace(*str))
		str++;

	/* return null if entire string was spaces */
	if(*str == '\0')
		return NULL;

	/* move end pointer back to last non-space character */
	char *end = str + strlen(str) - 1;
	while(end > str && isspace(*end))
		end--;

	/* get the length and allocate memory */
	int len = ++end - str;
	char *ret = malloc(len + 1);

	/* copy stripped section and null terminate */
	strncpy(ret, str, len);
	ret[len] = '\0';

	return ret;
} /* trim_spaces() */

/* greedy finder */
static char *get_from_ch_list(char *ch, char **list) {
	int i;
	char *ret = "";
	for(i = 0; list[i] != NULL; i++)
		/* checks against part or entire string against the given list */
		if(!strcmp(list[i], ch) && strlen(ret) < strlen(list[i]))
			ret = list[i];

	return strlen(ret) ? ret : 0;
} /* get_from_ch_list() */

static operator get_op(char *ch) {
	int i;
	operator ret = {NULL, op_none};
	for(i = 0; op_list[i].str != NULL; i++)
		/* checks against part or entire string against the given list */
		if(!strncmp(op_list[i].str, ch, strlen(op_list[i].str)))
			/* get longest match */
			if(!ret.str || strlen(op_list[i].str) > strlen(ret.str))
				ret = op_list[i];

	return ret;
} /* get_op() */

static synge_t *num_dup(synge_t num) {
	synge_t *ret = malloc(sizeof(synge_t));

	mpfr_init2(*ret, SYNGE_PRECISION);
	mpfr_set(*ret, num, SYNGE_ROUND);

	return ret;
} /* num_dup() */

static char *str_dup(char *s) {
	char *ret = malloc(strlen(s) + 1);
	memcpy(ret, s, strlen(s) + 1);
	return ret;
} /* str_dup() */

int synge_get_precision(synge_t num) {
	/* use the current settings' precision if given */
	if(active_settings.precision >= 0)
		return active_settings.precision;

	/* printf knows how to fix rounding errors */
	char *tmp = malloc(lenprintf("%.*" SYNGE_FORMAT, SYNGE_MAX_PRECISION, num));
	synge_sprintf(tmp, "%.*" SYNGE_FORMAT, SYNGE_MAX_PRECISION, num);

	/* move pointer to end and set precision to maximum */
	char *p = tmp + strlen(tmp) - 1;
	int precision = SYNGE_MAX_PRECISION;

	/* find all trailing zeros */
	while(*p-- == '0')
		precision--;

	free(tmp);
	return precision;
} /* get_precision() */

static error_code to_error_code(int error, int position) {
	/* fill a struct with the given values */
	error_code ret = {
		error,
		position
	};
	return ret;
} /* to_error_code() */

static special_number get_special_num(char *s) {
	special_number ret = {NULL, NULL, NULL};
	int i;

	for(i = 0; constant_list[i].name != NULL; i++)
		if(!strcmp(constant_list[i].name, s))
			return constant_list[i];

	return ret;
} /* get_special_num() */

static function *get_func(char *val) {
	/* get the word version of the function */
	function *ret = NULL;

	/* find matching function in builtin function lists */
	int i;
	for(i = 0; func_list[i].name != NULL; i++)
		if(!strncmp(val, func_list[i].name, strlen(func_list[i].name)))
			if(!ret || strlen(func_list[i].name) > strlen(ret->name))
				ret = &func_list[i];

	return ret;
} /* get_func() */

static void synge_strtofr(synge_t *num, char *str, char **endptr) {
	/* NOTE: Signing is now dealt using operators, so ignore signops */
	if(issignop(str)) {
		*endptr = str;
		return;
	}

	/* all special bases begin with 0_ but 0. doesn't count */
	if(strlen(str) <= 2 || *str != '0' || issignop(str + 1) || issignop(str + 2)) {
		/* default to decimal */
		mpfr_strtofr(*num, str, endptr, 10, SYNGE_ROUND);
		return;
	}

	/* go past the first 0 */
	str++;
	switch(*str) {
		case 'x':
			/* hexadecimal */
			mpfr_strtofr(*num, str + 1, endptr, 16, SYNGE_ROUND);
			break;
		case 'd':
			/* decimal */
			mpfr_strtofr(*num, str + 1, endptr, 10, SYNGE_ROUND);
			break;
		case 'b':
			/* binary */
			mpfr_strtofr(*num, str + 1, endptr, 2, SYNGE_ROUND);
			break;
		case 'o':
			/* octal */
			str++;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			/* just a leading zero -> octal */
			mpfr_strtofr(*num, str, endptr, 8, SYNGE_ROUND);
			break;
		default:
			/* default to decimal */
			mpfr_strtofr(*num, --str, endptr, 10, SYNGE_ROUND);
			return;
	}
} /* synge_strtofr() */

static bool isnum(char *string) {
	/* get variable word */
	char *endptr = NULL, *s = get_word(string, SYNGE_WORD_CHARS, &endptr);
	endptr = NULL;

	/* get synge_t number from string */
	synge_t tmp;
	mpfr_init2(tmp, SYNGE_PRECISION);
	synge_strtofr(&tmp, string, &endptr);
	mpfr_clears(tmp, NULL);

	/* all cases where word is a number */
	int ret = ((s && get_special_num(s).name) || string != endptr);
	free(s);
	return ret;
} /* isnum() */

static error_code set_variable(char *str, synge_t val) {
	assert(synge_started == true, "synge must be initialised");
	char *endptr = NULL, *s = get_word(str, SYNGE_WORD_CHARS, &endptr);

	/* make a new copy of the variable to save */
	synge_t tosave;
	mpfr_init2(tosave, SYNGE_PRECISION);
	mpfr_set(tosave, val, SYNGE_ROUND);

	/* free old value (if there is one) */
	if(ohm_search(variable_list, s, strlen(s) + 1)) {
		synge_t *tmp = ohm_search(variable_list, s, strlen(s) + 1);
		mpfr_clear(*tmp);
	}

	/* save the variable */
	ohm_remove(expression_list, s, strlen(s) + 1); /* remove word from function list (fake dynamic typing) */
	ohm_insert(variable_list, s, strlen(s) + 1, tosave, sizeof(synge_t));

	free(s);
	return to_error_code(SUCCESS, -1);
} /* set_variable() */

static error_code set_function(char *str, char *exp) {
	assert(synge_started == true, "synge must be initialised");
	char *endptr = NULL, *s = get_word(str, SYNGE_WORD_CHARS, &endptr);

	/* save the function */
	ohm_remove(variable_list, s, strlen(s) + 1); /* remove word from variable list (fake dynamic typing) */
	ohm_insert(expression_list, s, strlen(s) + 1, exp, strlen(exp) + 1);

	free(s);
	return to_error_code(SUCCESS, -1);
} /* set_function() */

static error_code del_word(char *s, int pos) {
	assert(synge_started == true, "synge must be initialised");

	/* word types */
	enum {
		tp_var,
		tp_func,
		tp_none
	};

	int type = tp_none;

	/* get type of word */
	if(ohm_search(variable_list, s, strlen(s) + 1))
		type = tp_var;
	else if(ohm_search(expression_list, s, strlen(s) + 1))
		type = tp_func;
	else
		return to_error_code(UNKNOWN_WORD, pos);

	/* free from correct list */
	switch(type) {
		case tp_var:
			{
				/* free mpfr_t */
				synge_t *tmp = ohm_search(variable_list, s, strlen(s) + 1);
				mpfr_clear(*tmp);

				/* free entry */
				ohm_remove(variable_list, s, strlen(s) + 1);
			}
			break;
		case tp_func:
			/* free entry */
			ohm_remove(expression_list, s, strlen(s) + 1);
			break;
		default:
			return to_error_code(UNKNOWN_WORD, pos);
			break;
	}

	return to_error_code(SUCCESS, -1);
} /* del_word() */

static char *get_expression_level(char *p, char end) {
	int num_paren = 0, len = 0;
	char *ret = NULL;

	/* until the end of string or a correct level closing ) */
	while(*p && (get_op(p).tp != op_rparen || num_paren)) {
		/* update level of expression */
		switch(get_op(p).tp) {
			case op_rparen:
				num_paren--;
				break;
			case op_lparen:
				num_paren++;
				break;
			default:
				break;
		}

		/* was that the end of the level? */
		if(!num_paren && *p == end)
			break;

		/* copy over character */
		ret = realloc(ret, ++len);
		ret[len - 1] = *p;

		p++;
	}

	/* null terminate */
	ret = realloc(ret, len + 1);
	ret[len] = '\0';
	return ret;
} /* get_expression_level() */

static error_code synge_tokenise_string(char *string, stack **infix_stack) {
	assert(synge_started == true, "synge must be initialised");

	debug("--\nTokenise\n--\n");
	debug("Input: %s\n", string);

	init_stack(*infix_stack);
	int i, pos, tmpoffset;

	int len = strlen(string);
	for(i = 0; i < len; i++) {
		/* get position shorthand */
		pos = i + 1;
		tmpoffset = 0;

		/* ignore spaces */
		if(isspace(string[i]))
			continue;

		char *endptr = NULL;
		char *word = get_word(string + i, SYNGE_WORD_CHARS, &endptr);

		if(isnum(string+i)) {
			synge_t *num = malloc(sizeof(synge_t)); /* allocate memory to be pushed onto the stack */
			mpfr_init2(*num, SYNGE_PRECISION);

			/* if it is a "special" number */
			if(word && get_special_num(word).name) {
				special_number stnum = get_special_num(word);
				stnum.value(*num, SYNGE_ROUND);
				tmpoffset = strlen(stnum.name); /* update iterator to correct offset */
			}
			else {
				/* set value */
				char *endptr = NULL;
				synge_strtofr(num, string + i, &endptr);
				tmpoffset = endptr - (string + i); /* update iterator to correct offset */
			}

			/* implied multiplications just like variables */
			if(top_stack(*infix_stack)) {
				switch(top_stack(*infix_stack)->tp) {
					case number:
					case rparen:
						/* two numbers together have an impiled * (i.e 2::x is 2*::x) */
						push_valstack("*", multop, false, NULL, pos, *infix_stack);
						break;
					default:
						break;
				}
			}

			push_valstack(num, number, true, synge_clear, pos, *infix_stack); /* push given value */

			/* error detection (done per number to ensure numbers are 163% correct) */
			if(mpfr_nan_p(*num)) {
				free(word);
				return to_error_code(UNDEFINED, pos);
			}
		}

		else if(get_op(string+i).str) {
			int oplen = strlen(get_op(string+i).str);
			s_type type;

			/* find and set type appropriate to operator */
			switch(get_op(string+i).tp) {
				case op_add:
				case op_subtract:
					/* if first thing in operator or previous doesn't mean */
					if(!top_stack(*infix_stack) || !isterm(top_stack(*infix_stack)->tp))
						type = signop;
					else
						type = addop;
					break;
				case op_multiply:
				case op_divide:
				case op_int_divide: /* integer division -- like in python */
				case op_modulo:
					type = multop;
					break;
				case op_index:
					type = expop;
					break;
				case op_lparen:
					type = lparen;

					/* every open paren with no operator (and number) before it has an implied * */
					if(top_stack(*infix_stack)) {
						switch(top_stack(*infix_stack)->tp) {
							case number:
							case rparen:
								push_valstack("*", multop, false, NULL, pos + 1, *infix_stack);
								break;
							default:
								break;
						}
					}
					break;
				case op_rparen:
					type = rparen;
					break;
				case op_gt:
				case op_gteq:
				case op_lt:
				case op_lteq:
				case op_neq:
				case op_eq:
					type = compop;
					break;
				case op_band:
				case op_bor:
				case op_bxor:
				case op_bshiftl:
				case op_bshiftr:
					type = bitop;
					break;
				case op_if:
					type = ifop;
					{

						/* get expression */
						char *expr = get_expression_level(string + i + oplen, ':');
						char *stripped = trim_spaces(expr);

						if(!expr || !stripped) {
							free(word);
							free(expr);
							free(stripped);
							return to_error_code(EMPTY_IF, pos);
						}

						/* push expression */
						push_valstack(stripped, expression, true, NULL, pos, *infix_stack);
						tmpoffset = strlen(expr);
						free(expr);
					}
					break;
				case op_else:
					type = elseop;
					{

						/* get expression */
						char *expr = get_expression_level(string + i + oplen, '\0');
						char *stripped = trim_spaces(expr);

						if(!expr || !stripped) {
							free(word);
							free(expr);
							free(stripped);
							return to_error_code(EMPTY_ELSE, pos);
						}

						/* push expression */
						push_valstack(stripped, expression, true, NULL, pos, *infix_stack);
						tmpoffset = strlen(expr);
						free(expr);
					}
					break;
				case op_var_set:
				case op_func_set:
					type = setop;
					break;
				case op_del:
					type = delop;
					if(top_stack(*infix_stack)) {
						/* make delops act more like numbers */
						switch(top_stack(*infix_stack)->tp) {
							case number:
							case rparen:
								/* two numbers together have an impiled * (i.e 2::x is 2*::x) */
								push_valstack("*", multop, false, NULL, pos, *infix_stack);
								break;
							default:
								break;
						}
					}
					break;
				case op_ca_add:
				case op_ca_subtract:
				case op_ca_multiply:
				case op_ca_divide:
				case op_ca_int_divide: /* integer division -- like in python */
				case op_ca_modulo:
				case op_ca_index:
				case op_ca_band:
				case op_ca_bor:
				case op_ca_bxor:
				case op_ca_bshiftl:
				case op_ca_bshiftr:
					type = modop;
					break;
				case op_ca_increment:
				case op_ca_decrement:
					/* a+++b === a++ + b (same as in C) and operators are left-weighted (always assume left) */
					if(top_stack(*infix_stack) && !isop(top_stack(*infix_stack)->tp) && !isparen(top_stack(*infix_stack)->tp))
						type = postmod;
					else
						type = premod;
					break;
				case op_binv:
				case op_bnot:
					type = preop;

					if(top_stack(*infix_stack)) {
						/* make pre-operators act just like normal numbers */
						switch(top_stack(*infix_stack)->tp) {
							case number:
							case rparen:
								/* a number and a pre-opped numbers together have an impiled * (i.e 2~x is 2*~x) */
								push_valstack("*", multop, false, NULL, pos, *infix_stack);
								break;
							default:
								break;
						}
					}
					break;
				case op_none:
				default:
					free(word);
					return to_error_code(UNKNOWN_TOKEN, pos);
			}
			push_valstack(get_op(string+i).str, type, false, NULL, pos, *infix_stack); /* push operator onto stack */

			if(get_op(string+i).tp == op_func_set) {
				char *func_expr = get_expression_level(string + i + oplen, '\0');
				char *stripped = trim_spaces(func_expr);

				push_valstack(stripped, expression, true, NULL, pos, *infix_stack);

				tmpoffset = strlen(func_expr);
				free(func_expr);
			}

			/* update iterator */
			tmpoffset += oplen;
		}
		else if(get_func(string+i)) {
			char *endptr = NULL, *funcword = get_word(string+i, SYNGE_FUNCTION_CHARS, &endptr); /* find the function word */

			/* make functions act just like normal numbers */
			if(top_stack(*infix_stack)) {
				switch(top_stack(*infix_stack)->tp) {
					case number:
					case rparen:
						/* a number and a pre-opped numbers together have an impiled * (i.e 2~x is 2*~x) */
						push_valstack("*", multop, false, NULL, pos, *infix_stack);
						break;
					default:
						break;
				}
			}

			function *functionp = get_func(funcword); /* get the function pointer, name, etc. */
			push_valstack(functionp, func, false, NULL, pos, *infix_stack);

			tmpoffset = strlen(functionp->name); /* update iterator to correct offset */
			free(funcword);
		}
		else if(word && strlen(word) > 0) {
			/* is it a variable or user function? */
			if(top_stack(*infix_stack)) {
				/* make variables act more like numbers (and more like variables) */
				switch(top_stack(*infix_stack)->tp) {
					case number:
					case rparen:
						/* two numbers together have an impiled * (i.e 2x is 2*x) */
						push_valstack("*", multop, false, NULL, pos, *infix_stack);
						break;
					default:
						break;
				}
			}
			char *stripped = trim_spaces(word);

			/* improvise an empty string */
			if(!stripped) {
				stripped = malloc(1);
				*stripped = '\0';
			}

			push_valstack(stripped, userword, true, NULL, pos, *infix_stack);
			tmpoffset = strlen(word); /* update iterator to correct offset */
		}
		else {
			/* catchall -- unknown token */
			free(word);
			return to_error_code(UNKNOWN_TOKEN, pos);
		}

		/* debugging */
		print_stack(*infix_stack);
		free(word);

		/* update iterator */
		i += tmpoffset - 1;
	}

	if(!stack_size(*infix_stack))
		return to_error_code(EMPTY_STACK, -1); /* stack was empty */

	/* debugging */
	print_stack(*infix_stack);
	return to_error_code(SUCCESS, -1);
} /* synge_tokenise_string() */

static bool op_precedes(s_type op1, s_type op2) {
	/* returns whether or not op2 should precede op1 */
	switch(op2) {
		case ifop:
		case elseop:
		case bitop:
		case compop:
		case addop:
		case multop:
			/* left associated operator */
			return op1 >= op2;

		case delop:
		case signop:
		case setop:
		case modop:
		case expop:
		case preop:
			/* right associated operator */
			return op1 > op2;

		default:
			/* what the hell are you doing? */
			return false;
	}
} /* op_precedes() */

/* my implementation of Dijkstra's really cool shunting-yard algorithm */
static error_code synge_infix_parse(stack **infix_stack, stack **rpn_stack) {
	stack *op_stack = malloc(sizeof(stack));

	debug("--\nParse\n--\n");

	init_stack(op_stack);
	init_stack(*rpn_stack);

	int i, size = stack_size(*infix_stack);

	/* reorder stack, in reverse (since we are poping from a full stack and pushing to an empty one) */
	for(i = 0; i < size; i++) {

		s_content stackp = (*infix_stack)->content[i]; /* pointer to current stack item (to make the code more readable) */
		int pos = stackp.position; /* shorthand for the position of errors */

		switch(stackp.tp) {
			case number:
				/* nothing to do, just push it onto the temporary stack */
				push_valstack(num_dup(SYNGE_T(stackp.val)), number, true, synge_clear, pos, *rpn_stack);
				break;
			case expression:
			case userword:
				/* do nothing, just push it onto the stack */
				push_valstack(str_dup(stackp.val), stackp.tp, true, NULL, pos, *rpn_stack);
				break;
			case lparen:
			case func:
				/* again, nothing to do, push it onto the stack */
				push_ststack(stackp, op_stack);
				break;
			case rparen:
				{
					/* keep popping and pushing until you find an lparen, which isn't to be pushed  */
					int found = false;
					while(stack_size(op_stack)) {
						s_content *tmpstackp = pop_stack(op_stack);
						if(tmpstackp->tp == lparen) {
							found = true;
							break;
						}
						push_ststack(*tmpstackp, *rpn_stack); /* push it onto the stack */
					}

					/* push function pointer to ouput (if there is one) */
					if(top_stack(op_stack) && top_stack(op_stack)->tp == func)
						push_ststack(*pop_stack(op_stack), *rpn_stack);

					/* if no lparen was found, this is an unmatched right bracket*/
					if(!found) {
						free_stackm(infix_stack, &op_stack, rpn_stack);
						return to_error_code(UNMATCHED_RIGHT_PARENTHESIS, pos);
					}
				}
				break;
			case premod:
			case delop:
				{
					i++;
					s_content *tmpstackp = &(*infix_stack)->content[i];

					/* ensure that you are pushing a setword */
					if(i >= size || !isword(tmpstackp->tp)) {
						free_stackm(infix_stack, &op_stack, rpn_stack);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					push_valstack(str_dup(tmpstackp->val), setword, true, NULL, tmpstackp->position, *rpn_stack);
					push_ststack(stackp, *rpn_stack);
				}
				break;
			case postmod:
				{
					if(top_stack(*rpn_stack) && top_stack(*rpn_stack)->tp == userword) {
						s_content *pop = pop_stack(*rpn_stack);
						push_valstack(pop->val, setword, true, NULL, pop->position, *rpn_stack);
					}

					push_ststack(stackp, *rpn_stack);
				}
				break;
			case setop:
			case modop:
				{
					if(top_stack(*rpn_stack) && top_stack(*rpn_stack)->tp == userword) {
						s_content *pop = pop_stack(*rpn_stack);
						push_valstack(pop->val, setword, true, NULL, pop->position, *rpn_stack);
					}
				}
				/* pass-through */
			case preop:
			case elseop:
			case ifop:
			case bitop:
			case compop:
			case signop:
			case addop:
			case multop:
			case expop:
				/* reorder operators to be in the correct order of precedence */
				while(stack_size(op_stack)) {
					s_content *tmpstackp = top_stack(op_stack);
					/* if the temporary item is an operator that precedes the current item, pop and push the temporary item onto the stack */
					if(isop(tmpstackp->tp) && op_precedes(tmpstackp->tp, stackp.tp))
						push_ststack(*pop_stack(op_stack), *rpn_stack);
					else break;
				}
				push_ststack(stackp, op_stack); /* finally, push the current item onto the stack */
				break;
			default:
				/* catchall -- unknown token */
				free_stackm(infix_stack, &op_stack, rpn_stack);
				return to_error_code(UNKNOWN_TOKEN, pos);
				break;
		}
	}

	/* re-reverse the stack again (so it's in the correct order) */
	while(stack_size(op_stack)) {
		s_content stackp = *pop_stack(op_stack);
		int pos = stackp.position;

		if(stackp.tp == lparen ||
		   stackp.tp == rparen) {
			/* if there is a left or right bracket, there is an unmatched left bracket */
			if(active_settings.strict >= strict) {
				free_stackm(infix_stack, &op_stack, rpn_stack);
				return to_error_code(UNMATCHED_LEFT_PARENTHESIS, pos);
			}
			else continue;
		}

		push_ststack(stackp, *rpn_stack);
	}

	/* debugging */
	print_stack(*rpn_stack);

	free_stackm(infix_stack, &op_stack);
	return to_error_code(SUCCESS, -1);
} /* infix_to_rpnstack() */

/* functions' whose input needs to be in radians */
static char *angle_infunc_list[] = {
	"sin",
	"cos",
	"tan",
	NULL
};

/* convert from set mode to radians */
static void settings_to_rad(synge_t out, synge_t in) {
	switch(active_settings.mode) {
		case degrees:
			deg_to_rad(out, in, SYNGE_ROUND);
			break;
		case gradians:
			grad_to_rad(out, in, SYNGE_ROUND);
			break;
		case radians:
			mpfr_set(out, in, SYNGE_ROUND);
			break;
	}
} /* settings_to_rad() */

/* functions' whose output is in radians */
static char *angle_outfunc_list[] = {
	"asin",
	"acos",
	"atan",
	NULL
};

/* convert radians to set mode */
static void rad_to_settings(synge_t out, synge_t in) {
	switch(active_settings.mode) {
		case degrees:
			rad_to_deg(out, in, SYNGE_ROUND);
			break;
		case gradians:
			rad_to_grad(out, in, SYNGE_ROUND);
			break;
		case radians:
			mpfr_set(out, in, SYNGE_ROUND);
			break;
	}
} /* rad_to_settings() */

static error_code eval_word(char *str, int pos, synge_t *result) {
	if(ohm_search(variable_list, str, strlen(str) + 1)) {
		synge_t *value = ohm_search(variable_list, str, strlen(str) + 1);
		mpfr_set(*result, *value, SYNGE_ROUND);
	}

	else if(ohm_search(expression_list, str, strlen(str) + 1)) {
		/* recursively evaluate a user function's value */
		char *expression = ohm_search(expression_list, str, strlen(str) + 1);
		error_code tmpecode = synge_internal_compute_string(expression, result, str, pos);

		/* error was encountered */
		if(!synge_is_success_code(tmpecode.code)) {
			if(active_settings.error == traceback)
				/* return real error code for traceback */
				return tmpecode;
			else
				/* return relative error code for all other error formats */
				return to_error_code(tmpecode.code, pos);
		}
	}

	else {
		/* unknown variable or function */
		return to_error_code(UNKNOWN_TOKEN, pos);
	}

	/* is the result a nan? */
	if(mpfr_nan_p(*result))
		return to_error_code(UNDEFINED, pos);

	return to_error_code(SUCCESS, -1);
} /* eval_word() */

static error_code eval_expression(char *exp, char *caller, int pos, synge_t *result) {
	error_code ret = synge_internal_compute_string(exp, result, caller, pos);

	/* error was encountered */
	if(!synge_is_success_code(ret.code)) {
		if(active_settings.error == traceback)
			/* return real error code for traceback */
			return ret;
		else
			/* return relative error code for all other error formats */
			return to_error_code(ret.code, pos);
	}

	return to_error_code(SUCCESS, -1);
}

/* evaluate an rpn stack */
static error_code synge_eval_rpnstack(stack **rpn, synge_t *output) {
	stack *evalstack = malloc(sizeof(stack));
	init_stack(evalstack);

	debug("--\nEvaluate\n--\n");

	int i, tmp = 0, size = stack_size(*rpn);
	synge_t *result = NULL, arg[3];
	error_code ecode[2];

	/* initialise operators */
	mpfr_inits2(SYNGE_PRECISION, arg[0], arg[1], arg[2], NULL);

	for(i = 0; i < size; i++) {
		/* shorthand variables */
		s_content stackp = (*rpn)->content[i];
		int pos = stackp.position;

		/* debugging */
		switch(stackp.tp) {
			case number:
				debug("%" SYNGE_FORMAT "\n", SYNGE_T(stackp.val));
				break;
			case func:
				debug("%s\n", FUNCTION(stackp.val)->name);
				break;
			default:
				debug("%s\n", stackp.val);
				break;
		}

		print_stack(evalstack);

		switch(stackp.tp) {
			case number:
				/* just push it onto the final stack */
				push_valstack(num_dup(SYNGE_T(stackp.val)), number, true, synge_clear, pos, evalstack);
				break;
			case expression:
			case setword:
				/* just push it onto the final stack */
				push_valstack(str_dup(stackp.val), stackp.tp, true, NULL, pos, evalstack);
				break;
			case setop:
				{
					if(stack_size(evalstack) < 2) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(OPERATOR_WRONG_ARGC, pos);
					}

					char *tmpexp = NULL;

					/* get new value for word */
					if(top_stack(evalstack)->tp == number)
						/* variable value */
						mpfr_set(arg[0], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);

					else if(top_stack(evalstack)->tp == expression)
						/* function expression value */
						tmpexp = str_dup(top_stack(evalstack)->val);

					else {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					free_scontent(pop_stack(evalstack));

					/* get word */
					if(top_stack(evalstack)->tp != setword) {
						free(tmpexp);
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					char *tmpstr = str_dup(top_stack(evalstack)->val);
					free_scontent(pop_stack(evalstack));

					/* set variable or function */
					switch(get_op(stackp.val).tp) {
						case op_var_set:
							set_variable(tmpstr, arg[0]);
							break;
						case op_func_set:
							set_function(tmpstr, tmpexp);
							break;
						default:
							free(tmpstr);
							free(tmpexp);
							free_stackm(&evalstack, rpn);
							mpfr_clears(arg[0], arg[1], arg[2], NULL);
							return to_error_code(INVALID_LEFT_OPERAND, pos);
							break;
					}

					free(tmpexp);

					/* evaulate and push the value of set word */
					result = malloc(sizeof(synge_t));
					mpfr_init2(*result, SYNGE_PRECISION);

					ecode[0] = eval_word(tmpstr, pos, result);
					if(!synge_is_success_code(ecode[0].code)) {
						mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
						free(result);
						free(tmpstr);
						free_stackm(&evalstack, rpn);

						/* when setting functions, we ignore any errors (and any errors with setting ...
						 ... a variable would have already been reported) */
						return to_error_code(ERROR_FUNC_ASSIGNMENT, pos);
					}
					push_valstack(result, number, true, synge_clear, pos, evalstack);
					free(tmpstr);
				}
				break;
			case modop:
				{
					if(stack_size(evalstack) < 2) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(OPERATOR_WRONG_ARGC, pos);
					}

					if(top_stack(evalstack)->tp != number) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_RIGHT_OPERAND, pos);
					}

					/* get value to modify variable by */
					mpfr_set(arg[1], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
					free_scontent(pop_stack(evalstack));

					/* get variable to modify */
					if(top_stack(evalstack)->tp != setword) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					/* get variable name */
					char *tmpstr = str_dup(top_stack(evalstack)->val);
					free_scontent(pop_stack(evalstack));

					/* check if it really is a variable */
					if(!ohm_search(variable_list, tmpstr, strlen(tmpstr) + 1)) {
						free(tmpstr);
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					/* get current value of variable */
					mpfr_set(arg[0], SYNGE_T(ohm_search(variable_list, tmpstr, strlen(tmpstr) + 1)), SYNGE_ROUND);

					/* evaluate changed variable */
					result = malloc(sizeof(synge_t));
					mpfr_init2(*result, SYNGE_PRECISION);

					switch(get_op(stackp.val).tp) {
						case op_ca_add:
							mpfr_add(*result, arg[0], arg[1], SYNGE_ROUND);
							break;
						case op_ca_subtract:
							mpfr_sub(*result, arg[0], arg[1], SYNGE_ROUND);
							break;
						case op_ca_multiply:
							mpfr_mul(*result, arg[0], arg[1], SYNGE_ROUND);
							break;
						case op_ca_int_divide:
							/* division, but the result ignores the decimals */
							tmp = 1;

							/* fall-through */
						case op_ca_divide:
							if(iszero(arg[1])) {
								/* the 11th commandment -- thoust shalt not divide by zero */
								mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
								free(tmpstr);
								free(result);
								free_stackm(&evalstack, rpn);
								return to_error_code(DIVIDE_BY_ZERO, pos);
							}

							mpfr_div(*result, arg[0], arg[1], SYNGE_ROUND);

							/* integer division? */
							if(tmp)
								mpfr_trunc(*result, *result);
							break;
						case op_ca_modulo:
							if(iszero(arg[1])) {
								/* the 11.5th commandment -- thoust shalt not modulo by zero */
								mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
								free(tmpstr);
								free(result);
								free_stackm(&evalstack, rpn);
								return to_error_code(MODULO_BY_ZERO, pos);
							}

							mpfr_fmod(*result, arg[0], arg[1], SYNGE_ROUND);
							break;
						case op_ca_index:
							mpfr_pow(*result, arg[0], arg[1], SYNGE_ROUND);
							break;
						case op_ca_band:
							{
								/* initialise gmp integer types */
								mpz_t final, op1, op2;
								mpz_init2(final, SYNGE_PRECISION);
								mpz_init2(op1, SYNGE_PRECISION);
								mpz_init2(op2, SYNGE_PRECISION);

								/* copy over operators to gmp integers */
								mpfr_get_z(op1, arg[0], SYNGE_ROUND);
								mpfr_get_z(op2, arg[1], SYNGE_ROUND);

								/* do binary and, and set result */
								mpz_and(final, op1, op2);
								mpfr_set_z(*result, final, SYNGE_ROUND);

								/* clean up */
								mpz_clears(final, op1, op2, NULL);
							}
							break;
						case op_ca_bor:
							{
								/* initialise gmp integer types */
								mpz_t final, op1, op2;
								mpz_init2(final, SYNGE_PRECISION);
								mpz_init2(op1, SYNGE_PRECISION);
								mpz_init2(op2, SYNGE_PRECISION);

								/* copy over operators to gmp integers */
								mpfr_get_z(op1, arg[0], SYNGE_ROUND);
								mpfr_get_z(op2, arg[1], SYNGE_ROUND);

								/* do binary or, and set result */
								mpz_ior(final, op1, op2);
								mpfr_set_z(*result, final, SYNGE_ROUND);

								/* clean up */
								mpz_clears(final, op1, op2, NULL);
							}
							break;
						case op_ca_bxor:
							{
								/* initialise gmp integer types */
								mpz_t final, op1, op2;
								mpz_init2(final, SYNGE_PRECISION);
								mpz_init2(op1, SYNGE_PRECISION);
								mpz_init2(op2, SYNGE_PRECISION);

								/* copy over operators to gmp integers */
								mpfr_get_z(op1, arg[0], SYNGE_ROUND);
								mpfr_get_z(op2, arg[1], SYNGE_ROUND);

								/* do binary xor, and set result */
								mpz_xor(final, op1, op2);
								mpfr_set_z(*result, final, SYNGE_ROUND);

								/* clean up */
								mpz_clears(final, op1, op2, NULL);
							}
							break;
						case op_ca_bshiftl:
							{
								/* bitshifting is an integer operation */
								mpfr_trunc(arg[1], arg[1]);
								mpfr_trunc(arg[0], arg[0]);

								/* x << y === x * 2^y */
								mpfr_ui_pow(arg[1], 2, arg[1], SYNGE_ROUND);
								mpfr_mul(*result, arg[0], arg[1], SYNGE_ROUND);

								/* again, integer operation */
								mpfr_trunc(*result, *result);
							}
							break;
						case op_ca_bshiftr:
							{
								/* bitshifting is an integer operation */
								mpfr_trunc(arg[1], arg[1]);
								mpfr_trunc(arg[0], arg[0]);

								/* x >> y === x / 2^y */
								mpfr_ui_pow(arg[1], 2, arg[1], SYNGE_ROUND);
								mpfr_div(*result, arg[0], arg[1], SYNGE_ROUND);

								/* again, integer operation */
								mpfr_trunc(*result, *result);
							}
							break;
						default:
							/* catch-all -- unknown token */
							mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
							free(tmpstr);
							free(result);
							free_stackm(&evalstack, rpn);
							return to_error_code(UNKNOWN_TOKEN, pos);
							break;
					}

					/* set variable to new value */
					set_variable(tmpstr, *result);

					/* push new value of variable */
					push_valstack(result, number, true, synge_clear, pos, evalstack);
					free(tmpstr);
				}
				break;
			case premod:
				tmp = 1;
				/* pass-through */
			case postmod:
				{
					if(stack_size(evalstack) < 1) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(OPERATOR_WRONG_ARGC, pos);
					}

					/* get variable to modify */
					if(top_stack(evalstack)->tp != setword) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					char *tmpstr = str_dup(top_stack(evalstack)->val);
					free_scontent(pop_stack(evalstack));

					/* check if it really is a variable */
					if(!ohm_search(variable_list, tmpstr, strlen(tmpstr) + 1)) {
						free(tmpstr);
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					/* get current value of variable */
					mpfr_set(arg[0], SYNGE_T(ohm_search(variable_list, tmpstr, strlen(tmpstr) + 1)), SYNGE_ROUND);

					/* evaluate changed variable */
					result = malloc(sizeof(synge_t));
					mpfr_init2(*result, SYNGE_PRECISION);

					switch(get_op(stackp.val).tp) {
						case op_ca_increment:
							mpfr_add_si(*result, arg[0], 1, SYNGE_ROUND);
							break;
						case op_ca_decrement:
							mpfr_sub_si(*result, arg[0], 1, SYNGE_ROUND);
							break;
						default:
							/* catch-all -- unknown token */
							mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
							free(tmpstr);
							free(result);
							free_stackm(&evalstack, rpn);
							return to_error_code(UNKNOWN_TOKEN, pos);
							break;
					}

					/* set variable to new value */
					set_variable(tmpstr, *result);

					/* push value of variable (depending on pre/post) */
					push_valstack(tmp ? result : num_dup(arg[0]), number, true, synge_clear, pos, evalstack);

					if(!tmp) {
						mpfr_clear(*result);
						free(result);
					}
					free(tmpstr);
				}
				break;
			case preop:
				{
					if(stack_size(evalstack) < 1) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(OPERATOR_WRONG_ARGC, pos);
					}

					if(top_stack(evalstack)->tp != number) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					result = malloc(sizeof(synge_t));
					mpfr_init2(*result, SYNGE_PRECISION);

					mpfr_set(arg[0], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
					free_scontent(pop_stack(evalstack));

					switch(get_op(stackp.val).tp) {
						case op_bnot:
							{
								/* !a => a == 0 */
								mpfr_set_si(*result, iszero(arg[0]), SYNGE_ROUND);
							}
							break;
						case op_binv:
							{
								mpfr_round(*result, arg[0]);

								/* ~a => -(a+1) */
								mpfr_add_si(*result, *result, 1, SYNGE_ROUND);
								mpfr_neg(*result, *result, SYNGE_ROUND);
							}
							break;
						default:
							break;
					}

					/* push result of evaluation onto the stack */
					push_valstack(result, number, true, synge_clear, pos, evalstack);
				}
				break;
			case delop:
				{
					if(stack_size(evalstack) < 1) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(OPERATOR_WRONG_ARGC, pos);
					}

					/* get word */
					if(top_stack(evalstack)->tp != setword) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_DELETE, pos);
					}

					char *tmpstr = str_dup(top_stack(evalstack)->val);
					free_scontent(pop_stack(evalstack));

					/* get value of word */
					result = malloc(sizeof(synge_t));
					mpfr_init2(*result, SYNGE_PRECISION);

					ecode[0] = eval_word(tmpstr, pos, result); /* ignore eval error for now (since word must be deleted) */

					/* delete word */
					ecode[1] = del_word(tmpstr, pos);

					/* delete error check */
					if(!synge_is_success_code(ecode[1].code)) {
						mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
						free(result);
						free(tmpstr);
						free_stackm(&evalstack, rpn);
						return ecode[1];
					}

					/* eval error check */
					if(!synge_is_success_code(ecode[0].code)) {
						mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
						free(result);
						free(tmpstr);
						free_stackm(&evalstack, rpn);
						return to_error_code(ERROR_DELETE, pos);
					}

					push_valstack(result, number, true, synge_clear, pos, evalstack);
					free(tmpstr);
				}
				break;
			case userword:
				{
					char *tmpstr = stackp.val;

					/* initialise result */
					result = malloc(sizeof(synge_t));
					mpfr_init2(*result, SYNGE_PRECISION);

					/* get word */
					ecode[0] = eval_word(tmpstr, pos, result);
					if(!synge_is_success_code(ecode[0].code)) {
						mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
						free(result);
						free_stackm(&evalstack, rpn);
						return ecode[0];
					}

					/* push result of evaluation onto the stack */
					push_valstack(result, number, true, synge_clear, pos, evalstack);
				}
				break;
			case func:
				/* check if there is enough numbers for function arguments */
				if(stack_size(evalstack) < 1) {
					free_stackm(&evalstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(FUNCTION_WRONG_ARGC, pos);
				}

				/* get the first (and, for now, only) argument */
				mpfr_set(arg[0], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
				free_scontent(pop_stack(evalstack));

				/* does the input need to be converted? */
				if(get_from_ch_list(FUNCTION(stackp.val)->name, angle_infunc_list)) /* convert settings angles to radians */
					settings_to_rad(arg[0], arg[0]);

				/* allocate result and evaluate it */
				result = malloc(sizeof(synge_t));
				mpfr_init2(*result, SYNGE_PRECISION);

				FUNCTION(stackp.val)->get(*result, arg[0], SYNGE_ROUND);

				/* does the output need to be converted? */
				if(get_from_ch_list(FUNCTION(stackp.val)->name, angle_outfunc_list)) /* convert radians to settings angles */
					rad_to_settings(*result, *result);

				/* push result of evaluation onto the stack */
				push_valstack(result, number, true, synge_clear, pos, evalstack);
				break;
			case elseop:
				{
					i++; /* skip past the if conditional */

					if(stack_size(evalstack) < 3) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(OPERATOR_WRONG_ARGC, pos);
					}

					if((*rpn)->content[i].tp != ifop) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(MISSING_IF, pos);
					}

					/* get else value */
					if(top_stack(evalstack)->tp != expression) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(UNKNOWN_ERROR, pos);
					}

					char *tmpelse = str_dup(top_stack(evalstack)->val);
					free_scontent(pop_stack(evalstack));

					/* get if value */
					if(top_stack(evalstack)->tp != expression) {
						free(tmpelse);
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(UNKNOWN_ERROR, pos);
					}

					char *tmpif = str_dup(top_stack(evalstack)->val);
					free_scontent(pop_stack(evalstack));

					/* get if condition */
					if(top_stack(evalstack)->tp != number) {
						free(tmpif);
						free(tmpelse);
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(UNKNOWN_ERROR, pos);
					}

					mpfr_set(arg[0], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
					free_scontent(pop_stack(evalstack));

					result = malloc(sizeof(synge_t));
					mpfr_init2(*result, SYNGE_PRECISION);

					error_code tmpecode;

					/* set correct value */
					if(!iszero(arg[0]))
						/* if expression */
						tmpecode = eval_expression(tmpif, SYNGE_IF, (*rpn)->content[i].position, result);
					else
						/* else expression */
						tmpecode = eval_expression(tmpelse, SYNGE_ELSE, (*rpn)->content[i-1].position, result);

					free(tmpif);
					free(tmpelse);

					if(!synge_is_success_code(tmpecode.code)) {
						mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
						free(result);
						free_stackm(&evalstack, rpn);
						return tmpecode;
					}

					push_valstack(result, number, true, synge_clear, pos, evalstack);
				}
				break;
			case ifop:
				/* ifop should never be found -- elseop always comes first in rpn stack */
				free_stackm(&evalstack, rpn);
				mpfr_clears(arg[0], arg[1], arg[2], NULL);
				return to_error_code(MISSING_ELSE, pos);
				break;
			case signop:
				/* check if there is enough numbers for operator "arguments" */
				if(stack_size(evalstack) < 1) {
					free_stackm(&evalstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				/* only numbers can be signed */
				if(top_stack(evalstack)->tp != number) {
					free_stackm(&evalstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(INVALID_LEFT_OPERAND, pos);
				}

				/* get argument */
				mpfr_set(arg[0], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
				free_scontent(pop_stack(evalstack));

				result = malloc(sizeof(synge_t));
				mpfr_init2(*result, SYNGE_PRECISION);

				/* find correct evaluation and do it */
				switch(get_op(stackp.val).tp) {
					case op_add:
						/* just copy value */
						mpfr_set(*result, arg[0], SYNGE_ROUND);
						break;
					case op_subtract:
						/* negate value */
						mpfr_neg(*result, arg[0], SYNGE_ROUND);
						break;
					default:
						/* catch-all -- unknown token */
						mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
						free(result);
						free_stackm(&evalstack, rpn);
						return to_error_code(UNKNOWN_TOKEN, pos);
						break;
				}

				/* push result onto stack */
				push_valstack(result, number, true, synge_clear, pos, evalstack);
				break;
			case bitop:
			case compop:
			case addop:
			case multop:
			case expop:
				/* check if there is enough numbers for operator "arguments" */
				if(stack_size(evalstack) < 2) {
					free_stackm(&evalstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				/* get second argument */
				mpfr_set(arg[1], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
				free_scontent(pop_stack(evalstack));

				/* get first argument */
				mpfr_set(arg[0], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
				free_scontent(pop_stack(evalstack));

				result = malloc(sizeof(synge_t));
				mpfr_init2(*result, SYNGE_PRECISION);

				/* find correct evaluation and do it */
				switch(get_op(stackp.val).tp) {
					case op_add:
						mpfr_add(*result, arg[0], arg[1], SYNGE_ROUND);
						break;
					case op_subtract:
						mpfr_sub(*result, arg[0], arg[1], SYNGE_ROUND);
						break;
					case op_multiply:
						mpfr_mul(*result, arg[0], arg[1], SYNGE_ROUND);
						break;
					case op_int_divide:
						/* division, but the result ignores the decimals */
						tmp = 1;
					case op_divide:
						if(iszero(arg[1])) {
							/* the 11th commandment -- thoust shalt not divide by zero */
							mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
							free(result);
							free_stackm(&evalstack, rpn);
							return to_error_code(DIVIDE_BY_ZERO, pos);
						}
						mpfr_div(*result, arg[0], arg[1], SYNGE_ROUND);

						if(tmp) {
							mpfr_trunc(*result, *result);
						}
						break;
					case op_modulo:
						if(iszero(arg[1])) {
							/* the 11.5th commandment -- thoust shalt not modulo by zero */
							mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
							free(result);
							free_stackm(&evalstack, rpn);
							return to_error_code(MODULO_BY_ZERO, pos);
						}
						mpfr_fmod(*result, arg[0], arg[1], SYNGE_ROUND);
						break;
					case op_index:
						mpfr_pow(*result, arg[0], arg[1], SYNGE_ROUND);
						break;
					case op_gt:
						{
							int cmp = mpfr_cmp(arg[0], arg[1]);

							/* equality => abs(arg[0] - arg[1]) < epsilon */
							synge_t eq;
							mpfr_init2(eq, SYNGE_PRECISION);
							mpfr_sub(eq, arg[0], arg[1], SYNGE_ROUND);

							mpfr_set_si(*result, cmp > 0 && !iszero(eq), SYNGE_ROUND);
							mpfr_clear(eq);
						}
						break;
					case op_gteq:
						{
							int cmp = mpfr_cmp(arg[0], arg[1]);

							/* equality => abs(arg[0] - arg[1]) < epsilon */
							synge_t eq;
							mpfr_init2(eq, SYNGE_PRECISION);
							mpfr_sub(eq, arg[0], arg[1], SYNGE_ROUND);

							mpfr_set_si(*result, cmp > 0 || iszero(eq), SYNGE_ROUND);
							mpfr_clear(eq);
						}
						break;
					case op_lt:
						{
							int cmp = mpfr_cmp(arg[0], arg[1]);

							/* equality => abs(arg[0] - arg[1]) < epsilon */
							synge_t eq;
							mpfr_init2(eq, SYNGE_PRECISION);
							mpfr_sub(eq, arg[0], arg[1], SYNGE_ROUND);

							mpfr_set_si(*result, cmp < 0 && !iszero(eq), SYNGE_ROUND);
							mpfr_clear(eq);
						}
						break;
					case op_lteq:
						{
							int cmp = mpfr_cmp(arg[0], arg[1]);

							/* equality => abs(arg[0] - arg[1]) < epsilon */
							synge_t eq;
							mpfr_init2(eq, SYNGE_PRECISION);
							mpfr_sub(eq, arg[0], arg[1], SYNGE_ROUND);

							mpfr_set_si(*result, cmp < 0 || iszero(eq), SYNGE_ROUND);
							mpfr_clear(eq);
						}
						break;
					case op_neq:
						{
							/* equality => abs(arg[0] - arg[1]) < epsilon */
							synge_t eq;
							mpfr_init2(eq, SYNGE_PRECISION);
							mpfr_sub(eq, arg[0], arg[1], SYNGE_ROUND);

							mpfr_set_si(*result, !iszero(eq), SYNGE_ROUND);
							mpfr_clear(eq);
						}
						break;
					case op_eq:
						{
							/* equality => abs(arg[0] - arg[1]) < epsilon */
							synge_t eq;
							mpfr_init2(eq, SYNGE_PRECISION);
							mpfr_sub(eq, arg[0], arg[1], SYNGE_ROUND);

							mpfr_set_si(*result, iszero(eq), SYNGE_ROUND);
							mpfr_clear(eq);
						}
						break;
					case op_band:
						{
							/* initialise gmp integer types */
							mpz_t final, op1, op2;
							mpz_init2(final, SYNGE_PRECISION);
							mpz_init2(op1, SYNGE_PRECISION);
							mpz_init2(op2, SYNGE_PRECISION);

							/* copy over operators to gmp integers */
							mpfr_get_z(op1, arg[0], SYNGE_ROUND);
							mpfr_get_z(op2, arg[1], SYNGE_ROUND);

							/* do binary and, and set result */
							mpz_and(final, op1, op2);
							mpfr_set_z(*result, final, SYNGE_ROUND);

							/* clean up */
							mpz_clears(final, op1, op2, NULL);
						}
						break;
					case op_bor:
						{
							/* initialise gmp integer types */
							mpz_t final, op1, op2;
							mpz_init2(final, SYNGE_PRECISION);
							mpz_init2(op1, SYNGE_PRECISION);
							mpz_init2(op2, SYNGE_PRECISION);

							/* copy over operators to gmp integers */
							mpfr_get_z(op1, arg[0], SYNGE_ROUND);
							mpfr_get_z(op2, arg[1], SYNGE_ROUND);

							/* do binary or, and set result */
							mpz_ior(final, op1, op2);
							mpfr_set_z(*result, final, SYNGE_ROUND);

							/* clean up */
							mpz_clears(final, op1, op2, NULL);
						}
						break;
					case op_bxor:
						{
							/* initialise gmp integer types */
							mpz_t final, op1, op2;
							mpz_init2(final, SYNGE_PRECISION);
							mpz_init2(op1, SYNGE_PRECISION);
							mpz_init2(op2, SYNGE_PRECISION);

							/* copy over operators to gmp integers */
							mpfr_get_z(op1, arg[0], SYNGE_ROUND);
							mpfr_get_z(op2, arg[1], SYNGE_ROUND);

							/* do binary xor, and set result */
							mpz_xor(final, op1, op2);
							mpfr_set_z(*result, final, SYNGE_ROUND);

							/* clean up */
							mpz_clears(final, op1, op2, NULL);
						}
						break;
					case op_bshiftl:
						{
							/* bitshifting is an integer operation */
							mpfr_trunc(arg[1], arg[1]);
							mpfr_trunc(arg[0], arg[0]);

							/* x << y === x * 2^y */
							mpfr_ui_pow(arg[1], 2, arg[1], SYNGE_ROUND);
							mpfr_mul(*result, arg[0], arg[1], SYNGE_ROUND);

							/* again, integer operation */
							mpfr_trunc(*result, *result);
						}
						break;
					case op_bshiftr:
						{
							/* bitshifting is an integer operation */
							mpfr_trunc(arg[1], arg[1]);
							mpfr_trunc(arg[0], arg[0]);

							/* x >> y === x / 2^y */
							mpfr_ui_pow(arg[1], 2, arg[1], SYNGE_ROUND);
							mpfr_div(*result, arg[0], arg[1], SYNGE_ROUND);

							/* again, integer operation */
							mpfr_trunc(*result, *result);
						}
						break;
					default:
						/* catch-all -- unknown token */
						mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
						free(result);
						free_stackm(&evalstack, rpn);
						return to_error_code(UNKNOWN_TOKEN, pos);
						break;
				}

				/* push result onto stack */
				push_valstack(result, number, true, synge_clear, pos, evalstack);
				break;
			default:
				/* catch-all -- unknown token */
				free_stackm(&evalstack, rpn);
				mpfr_clears(arg[0], arg[1], arg[2], NULL);
				return to_error_code(UNKNOWN_TOKEN, pos);
				break;
		}
	}
	mpfr_clears(arg[0], arg[1], arg[2], NULL);

	/* if there is not one item on the stack, there are too many values on the stack */
	if(stack_size(evalstack) != 1) {
		free_stackm(&evalstack, rpn);
		return to_error_code(TOO_MANY_VALUES, -1);
	}

	print_stack(evalstack);

	/* otherwise, the last item is the result */
	mpfr_set(*output, SYNGE_T(evalstack->content[0].val), SYNGE_ROUND);

	free_stackm(&evalstack, rpn);
	return to_error_code(SUCCESS, -1);
} /* synge_eval_rpnstack() */

static char *get_trace(link_t *link) {
	char *ret = str_dup(""), *current = NULL;
	link_iter *ii = link_iter_init(link);

	int len = 0;
	do {
		/* get current function traceback information */
		current = (char *) link_iter_content(ii);
		if(!current)
			continue;

		len += strlen(current);

		/* append current function traceback */
		ret = realloc(ret, len + 1);
		sprintf(ret, "%s%s", ret, current);
	} while(!link_iter_next(ii));

	link_iter_free(ii);
	return ret;
} /* get_trace() */

static char *get_error_type(error_code error) {
	switch(error.code) {
		case DIVIDE_BY_ZERO:
		case MODULO_BY_ZERO:
		case NUM_OVERFLOW:
		case UNDEFINED:
			return "MathError";
			break;
		case UNMATCHED_LEFT_PARENTHESIS:
		case UNMATCHED_RIGHT_PARENTHESIS:
		case FUNCTION_WRONG_ARGC:
		case OPERATOR_WRONG_ARGC:
		case MISSING_IF:
		case MISSING_ELSE:
		case EMPTY_STACK:
		case TOO_MANY_VALUES:
		case INVALID_LEFT_OPERAND:
		case INVALID_RIGHT_OPERAND:
		case INVALID_DELETE:
			return "SyntaxError";
			break;
		case UNKNOWN_TOKEN:
		case UNKNOWN_WORD:
		case RESERVED_VARIABLE:
			return "NameError";
			break;
		case TOO_DEEP:
		default:
			return "OtherError";
			break;

	}

	return "IHaveNoIdea";
} /* get_error_type() */

char *synge_error_msg(error_code error) {
	char *msg = NULL;

	/* get correct printf string */
	switch(error.code) {
		case DIVIDE_BY_ZERO:
			cheeky("The 11th Commandment: Thou shalt not divide by zero.\n");
			msg = "Cannot divide by zero";
			break;
		case MODULO_BY_ZERO:
			cheeky("The 11th Commandment: Thou shalt not modulo by zero.\n");
			msg = "Cannot modulo by zero";
			break;
		case UNMATCHED_LEFT_PARENTHESIS:
			msg = "Missing closing bracket for opening bracket";
			break;
		case UNMATCHED_RIGHT_PARENTHESIS:
			msg = "Missing opening bracket for closing bracket";
			break;
		case UNKNOWN_TOKEN:
			msg = "Unknown token or function in expression";
			break;
		case INVALID_LEFT_OPERAND:
			msg = "Invalid left operand of assignment";
			break;
		case INVALID_RIGHT_OPERAND:
			msg = "Invalid right operand of assignment";
			break;
		case INVALID_DELETE:
			msg = "Invalid word to delete";
			break;
		case UNKNOWN_WORD:
			msg = "Unknown word to delete";
			break;
		case FUNCTION_WRONG_ARGC:
			msg = "Not enough arguments for function";
			break;
		case OPERATOR_WRONG_ARGC:
			msg = "Not enough values for operator";
			break;
		case MISSING_IF:
			msg = "Missing if operator for else";
			break;
		case MISSING_ELSE:
			msg = "Missing else operator for if";
			break;
		case EMPTY_IF:
			msg = "Missing if block for else";
			break;
		case EMPTY_ELSE:
			msg = "Missing else block for if";
			break;
		case TOO_MANY_VALUES:
			msg = "Too many values in expression";
			break;
		case EMPTY_STACK:
			cheeky("There's an expression missing here...\n");
			msg = "Expression was empty";
			break;
		case UNDEFINED:
			cheeky("This is not the value you are looking for.\n");
			msg = "Result is undefined";
			break;
		case TOO_DEEP:
			cheeky("We have delved too deep and too greedily and have awoken a being of shadow, flame and infinite loops.\n");
			msg = "Delved too deep";
			break;
		default:
			cheeky("We dun goofed... :(\n");
			msg = "An unknown error has occured";
			break;
	}

	free(error_msg_container);

	char *trace = NULL;

	/* allocates memory and sets error_msg_container to correct (printf'd) string */
	switch(active_settings.error) {
		case traceback:
			if(!synge_is_success_code(error.code)) {
				char *fulltrace = get_trace(traceback_list);

				trace = malloc(lenprintf(SYNGE_TRACEBACK_FORMAT, fulltrace, get_error_type(error), msg));
				sprintf(trace, SYNGE_TRACEBACK_FORMAT, fulltrace, get_error_type(error), msg);

				free(fulltrace);
			}
		case position:
			if(error.position > 0) {
				error_msg_container = malloc(lenprintf("%s @ %d", trace ? trace : msg, error.position));
				sprintf(error_msg_container, "%s @ %d", trace ? trace : msg, error.position);
			} else {
		case simple:
				error_msg_container = malloc(lenprintf("%s", trace ? trace : msg));
				sprintf(error_msg_container, "%s", trace ? trace : msg);
			}
			break;
	}

	free(trace);

	debug("position of error: %d\n", error.position);
	return error_msg_container;
} /* get_error_msg() */

char *synge_error_msg_pos(int code, int pos) {
	return synge_error_msg(to_error_code(code, pos));
} /* get_error_msg_pos() */

enum {
	MODULE,
	CONDITIONAL,
	FUNCTION
};

static int synge_call_type(char *caller) {
	char first = *caller;
	char last = *(caller + strlen(caller) - 1);

	/* conditionals are SYNGE_IF and SYNGE_ELSE */
	if(!strcmp(caller, SYNGE_IF) || !strcmp(caller, SYNGE_ELSE))
		return CONDITIONAL;

	/* modules are in the format <____> */
	else if(first == '<' && last == '>')
		return MODULE;

	/* resort to function */
	return FUNCTION;
} /* synge_call_type() */

error_code synge_internal_compute_string(char *string, synge_t *result, char *caller, int position) {
	assert(synge_started == true, "synge must be initialised");

	/* "dynamically" resize hashmap to keep efficiency up */
	if(ohm_count(variable_list) > ohm_size(variable_list))
		variable_list = ohm_resize(variable_list, ohm_size(variable_list) * 2);

	ohm_t *backup_var = ohm_dup(variable_list);
	ohm_t *backup_func = ohm_dup(expression_list);

	/* duplicate all variables */
	ohm_iter i = ohm_iter_init(variable_list);
	for(; i.key; ohm_iter_inc(&i)) {
		synge_t new, *old = i.value;

		mpfr_init2(new, SYNGE_PRECISION);
		mpfr_set(new, *old, SYNGE_ROUND);

		ohm_insert(backup_var, i.key, i.keylen, new, sizeof(synge_t));
	}

	/* intiialise result to zero */
	mpfr_set_si(*result, 0, SYNGE_ROUND);

	/*
	 * We have delved too greedily and too deep.
	 * We have awoken a creature in the darkness of recursion.
	 * A creature of shadow, flame and segmentation faults.
	 * YOU SHALL NOT PASS!
	 */

	static int depth = -1;
	if(++depth >= SYNGE_MAX_DEPTH) {
		ohm_free(backup_func);

		ohm_iter ii = ohm_iter_init(backup_var);
		for(; ii.key; ohm_iter_inc(&ii)) {
			synge_t *val = ii.value;
			mpfr_clear(*val);
		}

		ohm_free(backup_var);
		return to_error_code(TOO_DEEP, -1);
	}

	/* reset traceback */
	if(!strcmp(caller, SYNGE_MAIN)) {
		link_free(traceback_list);
		traceback_list = link_init();
		depth = -1;
	}

	/* get traceback format from caller type */
	char *format = NULL;
	switch(synge_call_type(caller)) {
		case MODULE:
			format = SYNGE_TRACEBACK_MODULE;
			break;
		case CONDITIONAL:
			format = SYNGE_TRACEBACK_CONDITIONAL;
			break;
		case FUNCTION:
		default:
			format = SYNGE_TRACEBACK_FUNCTION;
			break;
	}

	/* add level to traceback */
	char *to_add = malloc(lenprintf(format, caller, position));
	sprintf(to_add, format, caller, position);

	link_append(traceback_list, to_add, strlen(to_add) + 1);
	free(to_add);

	debug("depth %d with caller %s\n", depth, caller);
	debug(" - expression '%s'\n", string);

	/* initialise all local variables */
	stack *rpn_stack = malloc(sizeof(stack)), *infix_stack = malloc(sizeof(stack));
	init_stack(rpn_stack);
	init_stack(infix_stack);
	error_code ecode = to_error_code(SUCCESS, -1);

	/* generate infix stack */
	if((ecode = synge_tokenise_string(string, &infix_stack)).code == SUCCESS)
		/* convert to postfix (or RPN) stack */
		if((ecode = synge_infix_parse(&infix_stack, &rpn_stack)).code == SUCCESS)
			/* evaluate postfix (or RPN) stack */
			if((ecode = synge_eval_rpnstack(&rpn_stack, result)).code == SUCCESS)
				/* fix up negative zeros */
				if(iszero(*result))
					mpfr_abs(*result, *result, SYNGE_ROUND);

	/* measure depth, not length */
	depth--;

	/* is it a nan? */
	if(mpfr_nan_p(*result))
		ecode = to_error_code(UNDEFINED, -1);

	/* if some error occured, revert variables and functions back to previous good state */
	if(!synge_is_success_code(ecode.code) && !synge_is_ignore_code(ecode.code)) {
		/* mpfr_clear variable list */
		ohm_iter i = ohm_iter_init(variable_list);
		for(; i.key != NULL; ohm_iter_inc(&i))
			mpfr_clear(i.value);

		ohm_cpy(variable_list, backup_var);
		ohm_cpy(expression_list, backup_func);
	}

	/* no error -- clear backup variable list */
	else {
		ohm_iter j = ohm_iter_init(backup_var);
		for(; j.key; ohm_iter_inc(&j))
			mpfr_clear(j.value);
	}

	/* make sure user hasn't done something like set '_' to a variable or deleted it */
	ohm_remove(variable_list, SYNGE_PREV_EXPRESSION, strlen(SYNGE_PREV_EXPRESSION) + 1);
	if(!ohm_search(expression_list, SYNGE_PREV_EXPRESSION, strlen(SYNGE_PREV_EXPRESSION) + 1))
			ohm_insert(expression_list, SYNGE_PREV_EXPRESSION, strlen(SYNGE_PREV_EXPRESSION) + 1, "", 1);

	/* if everything went well, set the answer variable (and remove current depth from traceback) */
	if(synge_is_success_code(ecode.code)) {
		mpfr_set(prev_answer, *result, SYNGE_ROUND);
		link_pend(traceback_list);

		/* if the expression doesn't contain '_', set '_' to the expression*/
		char *stripped = trim_spaces(string);

		if(!contains_word(stripped, SYNGE_PREV_EXPRESSION, SYNGE_WORD_CHARS))
			ohm_insert(expression_list, SYNGE_PREV_EXPRESSION, strlen(SYNGE_PREV_EXPRESSION) + 1, stripped, strlen(stripped) + 1);

		free(stripped);
	}

	/* free memory */
	ohm_free(backup_var);
	ohm_free(backup_func);

	free_stackm(&infix_stack, &rpn_stack);
	return ecode;
} /* synge_internal_compute_string() */

/* wrapper for above function */
error_code synge_compute_string(char *expression, synge_t *result) {
	return synge_internal_compute_string(expression, result, SYNGE_MAIN, 0);
} /* synge_compute_string() */

synge_settings synge_get_settings(void) {
	return active_settings;
} /* get_synge_settings() */

void synge_set_settings(synge_settings new_settings) {
	active_settings = new_settings;

	/* sanitise precision */
	if(new_settings.precision > SYNGE_MAX_PRECISION)
		active_settings.precision = SYNGE_MAX_PRECISION;
} /* set_synge_settings() */

function *synge_get_function_list(void) {
	return func_list;
} /* get_synge_function_list() */

ohm_t *synge_get_variable_list(void) {
	return variable_list;
} /* synge_get_variable_list() */

ohm_t *synge_get_expression_list(void) {
	return expression_list;
} /* synge_get_expression_list() */

word *synge_get_constant_list(void) {
	word *ret = malloc(len(constant_list) * sizeof(word));

	unsigned int i;
	for(i = 0; i < len(constant_list); i++) {
		ret[i].name = constant_list[i].name;
		ret[i].description = constant_list[i].description;
	}

	return ret;
} /* synge_get_constant_list() */

void synge_seed(unsigned int seed) {
	assert(synge_started == true, "synge must be initialised");
	gmp_randseed_ui(synge_state, seed);
} /* synge_seed() */

void synge_start(void) {
	assert(synge_started == false, "synge mustn't be initialised");

	variable_list = ohm_init(2, NULL);
	expression_list = ohm_init(2, NULL);
	traceback_list = link_init();

	mpfr_init2(prev_answer, SYNGE_PRECISION);
	mpfr_set_si(prev_answer, 0, SYNGE_ROUND);

	ohm_insert(expression_list, SYNGE_PREV_EXPRESSION, strlen(SYNGE_PREV_EXPRESSION) + 1, "", 1);

	gmp_randinit_default(synge_state);
	synge_started = true;
} /* synge_end() */

void synge_end(void) {
	assert(synge_started == true, "synge must be initialised");

	/* mpfr_free variables */
	ohm_iter i = ohm_iter_init(variable_list);
	for(; i.key != NULL; ohm_iter_inc(&i))
		mpfr_clear(i.value);

	ohm_free(variable_list);
	ohm_free(expression_list);

	link_free(traceback_list);
	free(error_msg_container);

	mpfr_clears(prev_answer, NULL);
	mpfr_free_cache();

	gmp_randclear(synge_state);
	synge_started = false;
} /* synge_end() */

void synge_reset_traceback(void) {
	assert(synge_started == true, "synge must be initialised");

	/* free previous traceback and allocate new one */
	if(traceback_list)
		link_free(traceback_list);

	traceback_list = link_init();

	/* reset traceback to base notation */
	int len = lenprintf(SYNGE_TRACEBACK_MODULE, SYNGE_MAIN);
	char *tmp = malloc(len);
	sprintf(tmp, SYNGE_TRACEBACK_MODULE, SYNGE_MAIN);

	link_append(traceback_list, tmp, len);

	free(tmp);
} /* synge_reset_traceback() */
