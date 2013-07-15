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

/* Operator Precedence
 * 4 Parenthesis
 * 3 Exponents, Roots (right associative)
 * 2 Multiplication, Division, Modulo (left associative)
 * 1 Addition, Subtraction (left associative)
 */

#define SYNGE_MAIN		"<main>"
#define SYNGE_IF_BLOCK		"<if>"
#define SYNGE_ELSE_BLOCK	"<else>"

#define SYNGE_MAX_PRECISION	64
#define SYNGE_MAX_DEPTH		2048

#define SYNGE_PREV_ANSWER	"ans"
#define SYNGE_VARIABLE_CHARS	"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ\'\"_"
#define SYNGE_FUNCTION_CHARS	"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ0123456789\'\"_"

#define SYNGE_TRACEBACK_FORMAT	"Synge Traceback (most recent call last):\n" \
				"%s" \
				"%s: %s"

#define SYNGE_TRACEBACK_TEMPLATE	"  Function %s, at %d\n"

#define strlower(x) do { char *p = x; for(; *p; ++p) *p = tolower(*p); } while(0)
#define strupper(x) do { char *p = x; for(; *p; ++p) *p = toupper(*p); } while(0)

/* to make changes in engine types smoother */
#define sy_fabs(...) fabsl(__VA_ARGS__)
#define sy_modf(...) modfl(__VA_ARGS__)
#define sy_fmod(...) fmodl(__VA_ARGS__)

/* my own assert() implementation */
#define assert(x) do { if(!x) { puts("synge: assertion '" #x "' failed -- aborting!"); exit(1); }} while(0)

/* -- useful macros -- */
#define isaddop(str) (get_op(str).tp == op_add || get_op(str).tp == op_subtract)
#define issetop(str) (get_op(str).tp == op_var_set || get_op(str).tp == op_func_set)
#define isdelop(str) (get_op(str).tp == op_del)

#define iscreop(str) (get_op(str).tp == op_ca_decrement || get_op(str).tp == op_ca_increment)

#define ismodop(str) (get_op(str).tp == op_ca_add || get_op(str).tp == op_ca_subtract || \
		      get_op(str).tp == op_ca_multiply || get_op(str).tp == op_ca_divide || get_op(str).tp == op_ca_int_divide || get_op(str).tp == op_ca_modulo || \
		      get_op(str).tp == op_ca_index || \
		      get_op(str).tp == op_ca_band || get_op(str).tp == op_ca_bor || get_op(str).tp == op_ca_bxor || \
		      get_op(str).tp == op_ca_bshiftl || get_op(str).tp == op_ca_bshiftr)

#define isop(type) (type == addop || type == multop || type == expop || type == compop || type == bitop || type == setop)
#define isnumword(type) (type == number || type == userword || type == setword)

/* when a floating point number has a rounding error, weird stuff starts to happen -- reliable bug */
#define has_rounding_error(number) ((number + 1) == number || (number - 1) == number)

/* hack to get amount of memory needed to store a sprintf() */
#define lenprintf(...) (synge_snprintf(NULL, 0, __VA_ARGS__) + 1)

/* macros for casting void stack pointers */
#define SYNGE_T(x) (*(synge_t *) x)
#define FUNCTION(x) ((function *) x)

static int synge_started = false; /* I REALLY recommend you leave this false, as this is used to ensure that synge_start has been run */

int iszero(synge_t num) {
	synge_t epsilon;

	/* generate "epsilon" */
	mpfr_init2(epsilon, SYNGE_PRECISION);
	mpfr_set_str(epsilon, "1e-65", 10, SYNGE_ROUND);

	/* if abs(num) < epsilon then it is zero */
	int cmp = mpfr_cmpabs(num, epsilon);
	mpfr_clears(epsilon, NULL);

	return cmp < 0 || mpfr_zero_p(num);
} /* iszero() */

int deg2rad(synge_t rad, synge_t deg, mpfr_rnd_t round) {
	/* get pi */
	synge_t pi;
	mpfr_init2(pi, SYNGE_PRECISION);
	mpfr_const_pi(pi, round);

	/* get conversion for deg -> rad */
	synge_t from_deg;
	mpfr_init2(from_deg, SYNGE_PRECISION);
	mpfr_div_si(from_deg, pi, 180, SYNGE_PRECISION);

	/* convert it */
	mpfr_mul(rad, deg, from_deg, round);

	/* free memory associated with values */
	mpfr_clears(pi, from_deg, NULL);
	return 0;
} /* deg2rad() */

int rad2deg(synge_t deg, synge_t rad, mpfr_rnd_t round) {
	/* get pi */
	synge_t pi;
	mpfr_init2(pi, SYNGE_PRECISION);
	mpfr_const_pi(pi, round);

	/* get conversion for deg -> rad */
	synge_t from_rad;
	mpfr_init2(from_rad, SYNGE_PRECISION);
	mpfr_si_div(from_rad, 180, pi, SYNGE_PRECISION);

	/* convert it */
	mpfr_mul(deg, rad, from_rad, round);

	/* free memory associated with values */
	mpfr_clears(pi, from_rad, NULL);
	return 0;
} /* rad2deg() */

int sy_rand(synge_t to, synge_t number, mpfr_rnd_t round) {
	/* round input */
	mpfr_round(number, number);
	int max = mpfr_get_si(number, round);
	int min = 0;

	/* better than the standard skewed (rand() % max + min + 1) range */
	int random = (rand() % (max + 1 - min)) + min;

	/* set input */
	mpfr_set_si(to, random, round);
	return 0;
} /* sy_rand() */

int sy_factorial(synge_t to, synge_t number, mpfr_rnd_t round) {
	/* round input */
	mpfr_round(number, number);
	unsigned int num = mpfr_get_ui(number, round);

	/* get factorial */
	mpfr_fac_ui(to, num, round);
	return 0;
} /* sy_factorial() */

int sy_series(synge_t to, synge_t number, mpfr_rnd_t round) {
	/* round input */
	mpfr_round(number, number);
	int num = mpfr_get_si(number, round);

	/* (x * (x + 1)) / 2 */
	mpfr_mul_si(to, number, num + 1, round);
	mpfr_div_si(to, to, 2, round);
	return 0;
} /* sy_series() */

int sy_assert(synge_t to, synge_t check, mpfr_rnd_t round) {
	return mpfr_set_si(to, iszero(check) ? 0 : 1, round);
} /* sy_assert */

static function func_list[] = {
	{"abs",		mpfr_abs,	"abs(x)",	"Absolute value of x"},
	{"sqrt",	mpfr_sqrt,	"sqrt(x)",	"Square root of x"},
	{"cbrt",	mpfr_cbrt,	"cbrt(x)",	"Cubic root of x"},

	{"floor",	mpfr_floor,	"floor(x)",	"Largest integer not greater than x"},
	{"round",	mpfr_round,	"round(x)",	"Closest integer to x"},
	{"ceil",	mpfr_ceil,	"ceil(x)",	"Smallest integer not smaller than x"},

	{"log10",	mpfr_log10,	"log10(x)",	"Base 10 logarithm of x"},
	{"log",		mpfr_log2,	"log(x)",	"Base 2 logarithm of x"},
	{"ln",		mpfr_log,	"ln(x)",	"Base e logarithm of x"},

	{"rand",	sy_rand,	"rand(x)",	"Generate a psedu-random integer between 0 and round(x)"},
	{"fact",	sy_factorial,	"fact(x)",	"Factorial of round(x)"},
	{"series",	sy_series,	"series(x)",	"Gives addition of all integers up to round(x)"},
	{"assert",	sy_assert,	"assert(x)",	"Returns 0 is x is 0, and returns 1 otherwise"},

	{"deg2rad",   	deg2rad,	"deg2rad(x)",	"Convert x degrees to radians"},
	{"rad2deg",   	rad2deg,	"rad2deg(x)",	"Convert x radians to degrees"},

	{"sinhi",	mpfr_asinh,	"asinh(x)",	"Inverse hyperbolic sine of x"},
	{"coshi",	mpfr_acosh,	"acosh(x)",	"Inverse hyperbolic cosine of x"},
	{"tanhi",	mpfr_atanh,	"atanh(x)",	"Inverse hyperbolic tangent of x"},
	{"sinh",	mpfr_sinh,	"sinh(x)",	"Hyperbolic sine of x"},
	{"cosh",	mpfr_cosh,	"cosh(x)",	"Hyperbolic cosine of x"},
	{"tanh",	mpfr_tanh,	"tanh(x)",	"Hyperbolic tangent of x"},

	{"sini",	mpfr_asin,	"asin(x)",	"Inverse sine of x"},
	{"cosi",	mpfr_acos,	"acos(x)",	"Inverse cosine of x"},
	{"tani",	mpfr_atan,	"atan(x)",	"Inverse tangent of x"},
	{"sin",		mpfr_sin,	"sin(x)",	"Sine of x"},
	{"cos",		mpfr_cos,	"cos(x)",	"Cosine of x"},
	{"tan",		mpfr_tan,	"tan(x)",	"Tangent of x"},
	{NULL,		NULL,		NULL,		NULL}
};

static function_alias alias_list[] = {
	{"asinh",	"sinhi"},
	{"acosh",	"coshi"},
	{"atanh",	"tanhi"},
	{"asin",	 "sini"},
	{"acos",	 "cosi"},
	{"atan",	 "tani"},
	{NULL,		   NULL},
};

static ohm_t *variable_list = NULL;
static ohm_t *function_list = NULL;

static synge_t prev_answer;

int synge_pi(synge_t num, mpfr_rnd_t round) {
	mpfr_const_pi(num, round);
	return 0;
} /* synge_pi() */

int synge_phi(synge_t num, mpfr_rnd_t round) {
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

int synge_euler(synge_t num, mpfr_rnd_t round) {
	/* get one */
	synge_t one;
	mpfr_init2(one, SYNGE_PRECISION);
	mpfr_set_si(one, 1, round);

	/* e^1 */
	mpfr_exp(num, one, round);

	mpfr_clears(one, NULL);
	return 0;
} /* synge_euler() */

int synge_life(synge_t num, mpfr_rnd_t round) {
	mpfr_set_si(num, 42, round);
	return 0;
} /* synge_life() */

int synge_true(synge_t num, mpfr_rnd_t round) {
	mpfr_set_si(num, 1, round);
	return 0;
} /* synge_true() */

int synge_false(synge_t num, mpfr_rnd_t round) {
	mpfr_set_si(num, 0, round);
	return 0;
} /* synge_false() */

int synge_ans(synge_t num, mpfr_rnd_t round) {
	mpfr_set(num, prev_answer, round);
	return 0;
} /* synge_ans() */

static special_number constant_list[] = {
	{"pi",			synge_pi},
	{"phi",			synge_phi},
	{"e",			synge_euler},
	{"life",		synge_life}, /* Sorry, I couldn't resist */

	{"true",		synge_true},
	{"false",		synge_false},

	{SYNGE_PREV_ANSWER,	synge_ans},
	{NULL,			NULL},
};
/* used for when a (char *) is needed, but needn't be freed */
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

static char *error_msg_container = NULL;
static link_t *traceback_list = NULL;

/* __DEBUG__ FUNCTIONS */

void print_stack(stack *s) {
#ifdef __SYNGE_DEBUG__
	int i, size = stack_size(s);
	for(i = 0; i < size; i++) {
		s_content tmp = s->content[i];
		if(!tmp.val) continue;

		if(tmp.tp == number)
			synge_printf("%.*" SYNGE_FORMAT " ", synge_get_precision(SYNGE_T(tmp.val)), SYNGE_T(tmp.val));
		else if(tmp.tp == func)
			printf("%s ", FUNCTION(tmp.val)->name);
		else
			printf("'%s' ", (char *) tmp.val);
	}
	printf("\n");
#endif
} /* print_stack() */

void debug(char *format, ...) {
#ifdef __SYNGE_DEBUG__
	va_list ap;
	va_start(ap, format);
	synge_vprintf(format, ap);
	va_end(ap);
#endif
} /* debug() */

/* END __DEBUG__ FUNCTIONS */

char *replace(char *str, char *old, char *new) {
	char *ret, *s = str;
	int i, count = 0;
	int newlen = strlen(new), oldlen = strlen(old);

	for(i = 0; s[i] != '\0'; i++) {
		if(strstr(&s[i], old) == &s[i]) {
			count++; /* found occurrence of old string */
			i += oldlen - 1; /* jump forward in string */
		}
	}

	ret = malloc(i + count * (newlen - oldlen) + 1);
	if (ret == NULL) return NULL;

	i = 0;
	while(*s != '\0') {
		if(strstr(s, old) == s) { /* is *s position at occurrence of old? */
			strcpy(&ret[i], new); /* copy over new to replace this occurrence */
			i += newlen; /* update iterator ... */
			s += oldlen; /* ... to new offsets  */
		} else ret[i++] = *s++; /* otherwise, copy over current character */
	}
	ret[i] = '\0'; /* null terminate ret */

	return ret;
} /* replace() */

char *get_word(char *s, char *list, char **endptr) {
	int lenstr = strlen(s), lenlist = strlen(list);
	int i, j, found;
	for(i = 0; i < lenstr; i++) { /* for the entire string */
		found = false; /* reset found variable */
		for(j = 0; j < lenlist; j++)
			if(s[i] == list[j]) {
				found = true; /* found a match! */
				break; /* gtfo. */
			}
		if(!found) break; /* current character not in string -- end of word */
	}

	/* copy over the word */
	char *ret = malloc(i + 1);
	memcpy(ret, s, i);
	ret[i] = '\0';

	strlower(ret); /* make the word lowercase -- since everything is case insensitive */

	*endptr = s+i;
	return ret;
} /* get_word() */

/* get number of times a character occurs in the given string */
int strnchr(char *str, char ch, int len) {
	if(len < 0)
		len = strlen(str);

	int i, ret = 0;
	for(i = 0; i < len; i++)
		if(str[i] == ch)
			ret++; /* found a match */
	return ret;
} /* strnchr() */

char *trim_spaces(char *str) {
	while(isspace(*str))
		str++; /* move starting pointer to first non-space character */

	if(*str == '\0')
		return NULL; /* return null if entire string was spaces */

	char *end = str + strlen(str) - 1;
	while(end > str && isspace(*end))
		end--; /* move end pointer back to last non-space character */

	/* get the length and allocate memory */
	int len = ++end - str;
	char *ret = malloc(len + 1);

	strncpy(ret, str, len); /* copy stripped section */
	ret[len] = '\0'; /* ensure null termination */

	return ret;
} /* trim_spaces() */

/* greedy finder */
char *get_from_ch_list(char *ch, char **list) {
	int i;
	char *ret = "";
	for(i = 0; list[i] != NULL; i++)
		/* checks against part or entire string against the given list */
		if(!strcmp(list[i], ch) && strlen(ret) < strlen(list[i]))
			ret = list[i];

	return strlen(ret) ? ret : 0;
} /* get_from_ch_list() */

operator get_op(char *ch) {
	int i;
	operator ret = {NULL, op_none};
	for(i = 0; op_list[i].str != NULL; i++)
		/* checks against part or entire string against the given list */
		if(!strncmp(op_list[i].str, ch, strlen(op_list[i].str)))
			if(!ret.str || strlen(ret.str) < strlen(op_list[i].str))
				ret = op_list[i];

	return ret;
}

synge_t *num_dup(synge_t num) {
	synge_t *ret = malloc(sizeof(synge_t));

	mpfr_init2(*ret, SYNGE_PRECISION);
	mpfr_set(*ret, num, SYNGE_ROUND);

	return ret;
} /* num_dup() */

char *str_dup(char *s) {
	char *ret = malloc(strlen(s) + 1);
	memcpy(ret, s, strlen(s) + 1);
	return ret;
} /* str_dup() */

int synge_get_precision(synge_t num) {
	/* use the current settings' precision if given */
	if(active_settings.precision >= 0)
		return active_settings.precision;

	/* printf knows how to fix rounding errors -- WARNING: here be dragons! */
	int tmpsize = lenprintf("%.*" SYNGE_FORMAT, SYNGE_MAX_PRECISION, num); /* get the amount of memory needed to store this printf*/
	char *tmp = malloc(tmpsize);
	synge_sprintf(tmp, "%.*" SYNGE_FORMAT, SYNGE_MAX_PRECISION, num); /* sprintf it */

	char *p = tmp + tmpsize - 2;
	int precision = SYNGE_MAX_PRECISION;
	while(*p == '0') { /* find all trailing zeros */
		precision--;
		p--;
	}

	free(tmp);
	return precision;
} /* get_precision() */

error_code to_error_code(int error, int position) {
	/* fill a struct with the given values */
	error_code ret = {
		error,
		position
	};
	return ret;
} /* to_error_code() */

/* linear search functions -- cuz honeybadger don't give a f*** */

special_number get_special_num(char *s) {
	int i;
	special_number ret = {NULL, NULL};
	for(i = 0; constant_list[i].name != NULL; i++)
		if(!strcmp(constant_list[i].name, s))
			return constant_list[i];
	return ret;
} /* get_special_num() */

function *get_func(char *val) {
	int i;
	char *endptr = NULL, *word = get_word(val, SYNGE_FUNCTION_CHARS, &endptr);
	function *ret = NULL;

	for(i = 0; func_list[i].name != NULL; i++)
		if(!strcmp(word, func_list[i].name)) {
			ret = &func_list[i];
			break;
		}

	free(word);
	return ret;
} /* get_func() */

/* end linear search functions */

void synge_strtofr(synge_t *num, char *str, char **endptr) {
	int sign = 0, sign_len = 0;

	/* deal with operators */
	if(isaddop(str)) {
		switch(get_op(str).tp) {
			case op_add:
				sign = 1;
				break;
			case op_subtract:
				sign = -1;
				break;
			default:
				break;
		}

		/* update iterator to after sign */
		sign_len = strlen(get_op(str).str);
		str += sign_len;
	}

	/* all special bases begin with 0_ */
	if(*str != '0' || *(str + 1) == '.') {
		/* go back to sign */
		str -= sign_len;

		/* default to decimal */
		mpfr_strtofr(*num, str, endptr, 10, SYNGE_ROUND);
		return;
	}

	/* go past the first 0 */
	str++;
	switch(*str) {
		case 'x':
			/* Hexadecimal */
			mpfr_strtofr(*num, str + 1, endptr, 16, SYNGE_ROUND);
			break;
		case 'd':
			/* Decimal */
			mpfr_strtofr(*num, str + 1, endptr, 10, SYNGE_ROUND);
			break;
		case 'b':
			/* Binary */
			mpfr_strtofr(*num, str + 1, endptr, 2, SYNGE_ROUND);
			break;
		case 'o':
			/* Octal */
			str++;
		default:
			/* Just a leading zero -> Octal */
			mpfr_strtofr(*num, str, endptr, 8, SYNGE_ROUND);
			break;
	}

	/* negate number, if sign is negative */
	if(sign < 0)
		mpfr_neg(*num, *num, SYNGE_ROUND);
} /* synge_strtofr() */

bool isnum(char *string) {
	int ret = false;
	char *endptr = NULL;
	char *s = get_word(string, SYNGE_VARIABLE_CHARS, &endptr);

	endptr = NULL;

	synge_t tmp;
	mpfr_init2(tmp, SYNGE_PRECISION);
	synge_strtofr(&tmp, string, &endptr);
	mpfr_clears(tmp, NULL);

	/* all cases where word is a number */
	ret = (get_special_num(s).name || string != endptr);
	free(s);
	return ret;
} /* isnum() */

error_code set_variable(char *str, synge_t val) {
	assert(synge_started);

	error_code ret = to_error_code(SUCCESS, -1);
	char *endptr = NULL, *s = get_word(str, SYNGE_FUNCTION_CHARS, &endptr);

	synge_t tosave;
	mpfr_init2(tosave, SYNGE_PRECISION);
	mpfr_set(tosave, val, SYNGE_ROUND);

	ohm_remove(function_list, s, strlen(s) + 1); /* remove word from function list (fake dynamic typing) */
	ohm_insert(variable_list, s, strlen(s) + 1, tosave, sizeof(synge_t));

	free(s);
	return ret;
} /* set_variable() */

error_code set_function(char *str, char *exp) {
	assert(synge_started);

	error_code ret = to_error_code(SUCCESS, -1);
	char *endptr = NULL, *s = get_word(str, SYNGE_FUNCTION_CHARS, &endptr);

	ohm_remove(variable_list, s, strlen(s) + 1); /* remove word from variable list (fake dynamic typing) */
	ohm_insert(function_list, s, strlen(s) + 1, exp, strlen(exp) + 1);

	free(s);
	return ret;
} /* set_function() */

error_code del_word(char *s, int pos) {
	assert(synge_started);

	enum {
		tp_var,
		tp_func,
		tp_none
	};

	int type = tp_none;

	if(ohm_search(variable_list, s, strlen(s) + 1))
		type = tp_var;
	if(ohm_search(function_list, s, strlen(s) + 1))
		type = tp_func;

	if(type == tp_none)
		return to_error_code(UNKNOWN_WORD, pos);

	switch(type) {
		case tp_var:
			ohm_remove(variable_list, s, strlen(s) + 1);
			break;
		case tp_func:
			ohm_remove(function_list, s, strlen(s) + 1);
			break;
		default:
			return to_error_code(UNKNOWN_WORD, pos);
			break;
	}

	return to_error_code(SUCCESS, -1);
} /* del_word() */

/* XXX: This hack is... ugly and difficult to explain. Consider re-doing or simplifying - cyphar */
char *function_process_replace(char *string) {
	char *firstpass = NULL;

	int i;
	for(i = 0; alias_list[i].alias != NULL; i++) {
		/* replace all aliases*/
		char *tmppass = replace(firstpass ? firstpass : string, alias_list[i].alias, alias_list[i].function);
		free(firstpass);
		firstpass = tmppass;
	}

	/* a "hack" for function division i thought of in maths ...
	 * ... basically, double every open and close bracket ... */
	char *secondpass = replace(firstpass, "(", "((");
	char *final = replace(secondpass, ")", "))");

	/* ... then for each function name, turn f((x)) into (f(x)) */
	for(i = 0; func_list[i].name != NULL; i++) {
		/* make search string */
		char *tmpfrom = malloc(strlen(func_list[i].name) + 3); /* +2 for brackets, +1 for null terminator */
		strcpy(tmpfrom, func_list[i].name);
		strcat(tmpfrom, "((");

		/* make replacement string */
		char *tmpto = malloc(strlen(func_list[i].name) + 3); /* +2 for brackets, +1 for null terminator */
		strcpy(tmpto, "(");
		strcat(tmpto, func_list[i].name);
		strcat(tmpto, "(");

		char *tmpfinal = replace(final, tmpfrom, tmpto);

		free(final);
		free(tmpfrom);
		free(tmpto);

		final = tmpfinal;
	}

	free(firstpass);
	free(secondpass);
	return final;
} /* function_process_replace() */

int recalc_padding(char *str, int len) {
	int ret = 0, tmp;

	/* the number of open and close brackets is synge_t the amount in the original string (except for mismatched parens) */
	tmp = strnchr(str, ')', len) + strnchr(str, '(', len);
	ret += tmp / 2 + tmp % 2;

	return ret;
} /* recalc_padding() */

int next_offset(char *str, int offset) {
	int i, len = strlen(str);
	/* continue from given offset */
	for(i = offset; i < len; i++)
		if(str[i] != ' ' && str[i] != '\t')
			/* found a non-whitespace character */
			return i;
	/* nothing left */
	return -1;
} /* next_offset() */

char *get_expression_level(char *p, char end) {
	int num_paren = 0, len = 0;
	char *ret = NULL;

	while(*p && ((*p != ')') || (num_paren && (*p == ')')))) {
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

		if(!num_paren && *p == end) {
			break;
		}

		ret = realloc(ret, ++len);
		ret[len - 1] = *p;

		p++;
	}
	ret = realloc(ret, len + 1);
	ret[len] = '\0';

	char *stripped = trim_spaces(ret);

	free(ret);
	return stripped;
} /* get_expression_level() */

#define false_number(str, stack) (!(!top_stack(stack) || /* first token is a number */ \
				 ((((!isnumword(top_stack(stack)->tp) && !iscreop(top_stack(stack)->val)) || !isaddop(str))) && /* a +/- number preceeded by a number is not a number */ \
	 			 (top_stack(stack)->tp != rparen || (!isaddop(str) && !iscreop(top_stack(stack)->val)))))) /* a +/- number preceeded by a ')' is not a number */

error_code synge_tokenise_string(char *string, stack **infix_stack) {
	assert(synge_started);
	char *s = function_process_replace(string);

	debug("--\nTokenise\n--\n");
	debug("Input: %s\nProcessed: %s\n", string, s);

	init_stack(*infix_stack);
	int i, pos, len = strlen(s), nextpos = 0, tmpoffset = 0;
	char *word = NULL, *endptr = NULL;

	for(i = 0; i < len; i++) {
		pos = i - recalc_padding(s, (i ? i : 1) - 1) + 1;
		nextpos = next_offset(s, i + 1);
		tmpoffset = 0;

		/* ignore spaces */
		if(s[i] == ' ')
			continue;

		word = get_word(s + i, SYNGE_VARIABLE_CHARS, &endptr);

		if(isnum(s+i) && !false_number(s+i, *infix_stack)) {
			synge_t *num = malloc(sizeof(synge_t)); /* allocate memory to be pushed onto the stack */
			mpfr_init2(*num, SYNGE_PRECISION);

			/* if it is a "special" number */
			if(get_special_num(word).name) {
				special_number stnum = get_special_num(word);
				stnum.value(*num, SYNGE_ROUND);
				tmpoffset = strlen(stnum.name); /* update iterator to correct offset */

				/* implied multiplications, etc just like variables */
				if(top_stack(*infix_stack)) {
					s_content *tmppop, *tmpp;
					synge_t implied;
					/* make special numbers act more like numbers (and more like variables) */
					switch(top_stack(*infix_stack)->tp) {
						case addop:
							/* if there is a +/- in front of a number, it should set the sign of that variable (i.e. 1--pi is 1+pi) */
							tmppop = pop_stack(*infix_stack); /* the sign (to be saved for later) */
							tmpp = top_stack(*infix_stack);
							if(!tmpp || (tmpp->tp != number && tmpp->tp != rparen)) { /* sign is to be discarded */
								if(get_op(tmppop->val).tp == op_subtract) {
									mpfr_init2(implied, SYNGE_PRECISION);
									mpfr_set_si(implied, 0, SYNGE_ROUND);

									/* negate the variable? +0-pi negates it */
									if(tmpp && tmpp->tp != lparen)
										push_valstack("+", addop, false, pos, *infix_stack);

									push_valstack(num_dup(implied), number, true, pos, *infix_stack);
									push_valstack("-", addop, false, pos, *infix_stack);

									mpfr_clears(implied, NULL);
								}
								else if(tmpp && tmpp->tp != lparen) {
									/* otherwise, add the number */
									push_valstack("+", addop, false, pos, *infix_stack);
								}
							}
							else {
								/* whoops! didn't match criteria. push sign back. */
								push_ststack(*tmppop, *infix_stack);
							}
							break;
						case number:
							/* two numbers together have an impiled * (i.e 2x is 2*x) */
							push_valstack("*", multop, false, pos, *infix_stack);
							break;
						default:
							break;
					}
				}
			}
			else {
				char *endptr;

				/* set value */
				synge_strtofr(num, s+i, &endptr);
				tmpoffset = endptr - (s + i); /* update iterator to correct offset */
			}
			push_valstack(num, number, true, pos, *infix_stack); /* push given value */

			/* error detection (done per number to ensure numbers are 163% correct) */
			if(mpfr_nan_p(*num)) {
				free(s);
				free(word);
				return to_error_code(UNDEFINED, pos);
			}
		}

		else if(get_op(s+i).str) {
			int postpush = false, tmp = 0, oplen = strlen(get_op(s+i).str);
			s_type type;

			char *expr = NULL;
			s_content pop, top;

			/* find and set type appropriate to operator */
			switch(get_op(s+i).tp) {
				case op_add:
				case op_subtract:
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
					pos--;  /* "hack" to ensure error position is correct */
					/* every open paren with no operators before it has an implied * */

					if(top_stack(*infix_stack)) {
						synge_t implied;
						switch(top_stack(*infix_stack)->tp) {
							case addop:
								tmp = false;
								top = *pop_stack(*infix_stack);

								if(!top_stack(*infix_stack))
									tmp = true;

								else {
									pop = *pop_stack(*infix_stack);
									if(pop.tp != number && pop.tp != rparen && pop.tp != userword)
										tmp = true;

									push_ststack(pop, *infix_stack);
								}

								if(!tmp) {
									push_ststack(top, *infix_stack);
									break;
								}

								mpfr_init2(implied, SYNGE_PRECISION);

								if(get_op(top.val).tp == op_add)
									mpfr_set_si(implied, 1, SYNGE_ROUND);

								if(get_op(top.val).tp == op_subtract)
									mpfr_set_si(implied, -1, SYNGE_ROUND);

								push_valstack(num_dup(implied), number, true, pos, *infix_stack);
								mpfr_clears(implied, NULL);
								/* pass-through */
							case number:
							case rparen:
								push_valstack("*", multop, false, pos + 1, *infix_stack);
								break;
							default:
								break;
						}
					}
					break;
				case op_rparen:
					type = rparen;
					pos--;  /* "hack" to ensure error position is correct */
					if(nextpos > 0 && isnum(s + nextpos) && !get_op(s + nextpos).str)
						postpush = true;
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

					/* get expression and position of it */
					tmp = next_offset(s, i + oplen);
					expr = get_expression_level(s + i + oplen, ':');

					if(!expr) {
						free(s);
						free(word);
						return to_error_code(EMPTY_IF, pos);
					}

					/* push expression */
					push_valstack(expr, expression, true, tmp, *infix_stack);
					tmpoffset = strlen(expr);
					break;
				case op_else:
					type = elseop;

					/* get expression and position of it */
					tmp = next_offset(s, i + oplen);
					expr = get_expression_level(s + i + oplen, '\0');

					if(!expr) {
						free(s);
						free(word);
						return to_error_code(EMPTY_ELSE, pos);
					}

					/* push expression */
					push_valstack(expr, expression, true, tmp, *infix_stack);
					tmpoffset = strlen(expr);
					break;
				case op_var_set:
				case op_func_set:
					type = setop;
					break;
				case op_del:
					type = delop;
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
					/* a+++b === a++ + b (same as in C) */
					if(top_stack(*infix_stack) && top_stack(*infix_stack)->tp == setword)
						type = postmod;
					else
						type = premod;
					break;
				case op_none:
				default:
					free(s);
					free(word);
					return to_error_code(UNKNOWN_TOKEN, pos);
			}
			push_valstack(get_op(s+i).str, type, false, pos, *infix_stack); /* push operator onto stack */

			if(postpush)
				push_valstack("*", multop, false, pos, *infix_stack);

			if(get_op(s+i).tp == op_func_set) {
				char *func_expr = get_expression_level(s + i + oplen, '\0');

				push_valstack(func_expr, expression, true, next_offset(s, i + oplen), *infix_stack);
				tmpoffset = strlen(func_expr);
			}

			/* update iterator */
			tmpoffset += oplen;
		}
		else if(get_func(s+i)) {
			char *endptr = NULL, *funcword = get_word(s+i, SYNGE_FUNCTION_CHARS, &endptr); /* find the function word */

			function *funcname = get_func(funcword); /* get the function pointer, name, etc. */
			push_valstack(funcname, func, false, pos - 1, *infix_stack);
			free(funcword);

			tmpoffset = strlen(funcname->name); /* update iterator to correct offset */
		}
		else if(strlen(word) > 0) {
			/* is it a variable or user function? */
			if(top_stack(*infix_stack)) {
				s_content *tmppop, *tmpp;
				synge_t implied;
				/* make variables act more like numbers (and more like variables) */
				switch(top_stack(*infix_stack)->tp) {
					case addop:
						/* if there is a +/- in front of a variable, it should set the sign of that variable (i.e. 1--x is 1+x) */
						tmppop = pop_stack(*infix_stack); /* the sign (to be saved for later) */
						tmpp = top_stack(*infix_stack);
						if(!tmpp || (tmpp->tp != number && tmpp->tp != rparen)) { /* sign is to be discarded */
							if(get_op(tmppop->val).tp == op_subtract) {
								mpfr_init2(implied, SYNGE_PRECISION);
								mpfr_set_si(implied, 0, SYNGE_ROUND);

								/* negate the variable? +0-x negates it */
								if(tmpp && tmpp->tp != lparen)
									push_valstack("+", addop, false, pos, *infix_stack);

								push_valstack(num_dup(implied), number, true, pos, *infix_stack);
								push_valstack("-", addop, false, pos, *infix_stack);

								mpfr_clears(implied, NULL);
							}
							else if(tmpp && tmpp->tp != lparen) {
								/* otherwise, add the variable */
								push_valstack("+", addop, false, pos, *infix_stack);
							}
						}
						else {
							/* whoops! didn't match criteria. push sign back. */
							push_ststack(*tmppop, *infix_stack);
						}
						break;
					case number:
						/* two numbers together have an impiled * (i.e 2x is 2*x) */
						push_valstack("*", multop, false, pos, *infix_stack);
						break;
					default:
						break;
				}
			}

			int type = userword;

			/* is this word going to be set? */
			nextpos = next_offset(s, i + strlen(word));
			if(nextpos > 0 && (issetop(s + nextpos) || ismodop(s + nextpos) || iscreop(s + nextpos)))
				type = setword;

			if(top_stack(*infix_stack) && (isdelop(top_stack(*infix_stack)->val) || iscreop(top_stack(*infix_stack)->val)))
				type = setword;

			push_valstack(str_dup(word), type, true, pos, *infix_stack);
			tmpoffset = strlen(word); /* update iterator to correct offset */
		}
		else {
			/* catchall -- unknown token */
			free(word);
			free(s);
			return to_error_code(UNKNOWN_TOKEN, pos);
		}
		/* debugging */
		print_stack(*infix_stack);
		free(word);

		/* update iterator */
		i += tmpoffset - 1;
	}

	free(s);
	if(!stack_size(*infix_stack))
		return to_error_code(EMPTY_STACK, -1); /* stack was empty */

	/* debugging */
	print_stack(*infix_stack);
	return to_error_code(SUCCESS, -1);
} /* synge_tokenise_string() */

bool op_precedes(s_type op1, s_type op2) {
	/* returns true if:
	 * 	op1 > op2 (or op1 > op2 - 0)
	 * 	op2 is left associative and op1 >= op2 (or op1 > op2 - 1)
	 * returns false otherwise
	 */
	int lassoc;
	/* here be dragons! obscure integer hacks follow. */
	switch(op2) {
		case delop:
		case ifop:
		case elseop:
		case bitop:
		case compop:
		case addop:
		case multop:
			lassoc = 1;
			break;
		case setop:
		case modop:
		case expop:
			lassoc = 0;
			break;
		default:
			/* catchall -- i don't even ... */
			return false; /* what the hell are you doing? */
			break;
	}
	return op1 > (op2 - lassoc);
} /* op_precedes() */

/* my implementation of Dijkstra's really cool shunting-yard algorithm */
error_code synge_infix_parse(stack **infix_stack, stack **rpn_stack) {
	s_content stackp, *tmpstackp;
	stack *op_stack = malloc(sizeof(stack));

	debug("--\nParse\n--\n");

	init_stack(op_stack);
	init_stack(*rpn_stack);

	int i, found, pos, size = stack_size(*infix_stack);

	/* reorder stack, in reverse (since we are poping from a full stack and pushing to an empty one) */
	for(i = 0; i < size; i++) {
		stackp = (*infix_stack)->content[i]; /* pointer to current stack item (to make the code more readable) */
		pos = stackp.position; /* shorthand for the position of errors */

		switch(stackp.tp) {
			case number:
				/* nothing to do, just push it onto the temporary stack */
				push_valstack(num_dup(SYNGE_T(stackp.val)), number, true, pos, *rpn_stack);
				break;
			case expression:
			case userword:
			case setword:
				/* do nothing, just push it onto the stack */
				push_valstack(str_dup(stackp.val), stackp.tp, true, pos, *rpn_stack);
				break;
			case lparen:
			case func:
				/* again, nothing to do, push it onto the stack */
				push_ststack(stackp, op_stack);
				break;
			case rparen:
				/* keep popping and pushing until you find an lparen, which isn't to be pushed  */
				found = false;
				while(stack_size(op_stack)) {
					tmpstackp = pop_stack(op_stack);
					if(tmpstackp->tp == lparen) {
						found = true;
						break;
					}
					push_ststack(*tmpstackp, *rpn_stack); /* push it onto the stack */
				}

				/* if no lparen was found, this is an unmatched right bracket*/
				if(!found) {
					free_stackm(infix_stack, &op_stack, rpn_stack);
					return to_error_code(UNMATCHED_RIGHT_PARENTHESIS, pos + 1);
				}
				break;
			case premod:
				i++;
				tmpstackp = &(*infix_stack)->content[i];

				/* ensure that you are pushing a setword */
				if(tmpstackp->tp != setword) {
					free_stackm(infix_stack, &op_stack, rpn_stack);
					return to_error_code(INVALID_LEFT_OPERAND, pos);
				}

				push_valstack(str_dup(tmpstackp->val), tmpstackp->tp, true, tmpstackp->position, *rpn_stack);
				/* pass-through */
			case postmod:
				push_ststack(stackp, *rpn_stack);
				break;
			case setop:
			case modop:
			case elseop:
			case ifop:
			case bitop:
			case compop:
			case addop:
			case multop:
			case expop:
			case delop:
				/* reorder operators to be in the correct order of precedence */
				while(stack_size(op_stack)) {
					tmpstackp = top_stack(op_stack);
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
		stackp = *pop_stack(op_stack);
		pos = stackp.position;
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
char *angle_infunc_list[] = {
	"sin",
	"cos",
	"tan",
	NULL
};

/* convert from set mode to radians */
void settings_to_rad(synge_t out, synge_t in) {
	switch(active_settings.mode) {
		case degrees:
			deg2rad(out, in, SYNGE_ROUND);
			break;
		case radians:
			mpfr_set(out, in, SYNGE_ROUND);
			break;
	}
} /* settings_to_rad() */

/* functions' whose output is in radians */
char *angle_outfunc_list[] = {
	"sini",
	"cosi",
	"tani",
	NULL
};

/* convert radians to set mode */
void rad_to_settings(synge_t out, synge_t in) {
	switch(active_settings.mode) {
		case degrees:
			rad2deg(out, in, SYNGE_ROUND);
			break;
		case radians:
			mpfr_set(out, in, SYNGE_ROUND);
			break;
	}
} /* rad_to_settings() */

error_code eval_word(char *str, int pos, synge_t *result) {
	/* is it a legitamate variable or function? */
	if(ohm_search(variable_list, str, strlen(str) + 1)) {
		/* variable */
		mpfr_set(*result, SYNGE_T(ohm_search(variable_list, str, strlen(str) + 1)), SYNGE_ROUND);
	}
	else if(ohm_search(function_list, str, strlen(str) + 1)) {

		/* function */
		/* recursively evaluate a user function's value */
		char *expression = ohm_search(function_list, str, strlen(str) + 1);
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

error_code eval_expression(char *exp, char *caller, int pos, synge_t *result) {
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
error_code synge_eval_rpnstack(stack **rpn, synge_t *output) {
	stack *tmpstack = malloc(sizeof(stack));
	init_stack(tmpstack);

	debug("--\nEvaluate\n--\n");

	s_content stackp;

	char *tmpstr = NULL, *tmpexp = NULL;
	char *tmpif = NULL, *tmpelse = NULL;

	int i, pos = 0, tmp = 0, size = stack_size(*rpn);
	synge_t *result = NULL, arg[3];
	error_code ecode[2];

	/* initialise operators */
	mpfr_inits2(SYNGE_PRECISION, arg[0], arg[1], arg[2], NULL);

	for(i = 0; i < size; i++) {
		stackp = (*rpn)->content[i]; /* shorthand for current stack item */
		pos = stackp.position; /* shorthand for current error position */

		tmp = 0;
		tmpstr = tmpexp = NULL;

		/* debugging */
		if(stackp.tp == number)
			debug("%" SYNGE_FORMAT "\n", SYNGE_T(stackp.val));
		else
			debug("%s\n", stackp.val);

		print_stack(tmpstack);

		switch(stackp.tp) {
			case number:
				/* just push it onto the final stack */
				push_valstack(num_dup(SYNGE_T(stackp.val)), number, true, pos, tmpstack);
				break;
			case expression:
			case setword:
				/* just push it onto the final stack */
				push_valstack(str_dup(stackp.val), stackp.tp, true, pos, tmpstack);
				break;
			case setop:
				if(stack_size(tmpstack) < 2) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				/* get new value for word */
				if(top_stack(tmpstack)->tp == number)
					/* variable value */
					mpfr_set(arg[0], SYNGE_T(top_stack(tmpstack)->val), SYNGE_ROUND);

				else if(top_stack(tmpstack)->tp == expression)
					/* function expression value */
					tmpexp = str_dup(top_stack(tmpstack)->val);

				else {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(INVALID_LEFT_OPERAND, pos);
				}

				free_scontent(pop_stack(tmpstack));

				/* get word */
				if(top_stack(tmpstack)->tp != setword) {
					free(tmpexp);
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(INVALID_LEFT_OPERAND, pos);
				}

				tmpstr = str_dup(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

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
						free_stackm(&tmpstack, rpn);
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
					free_stackm(&tmpstack, rpn);

					/* when setting functions, we ignore any errors (and any errors with setting ...
					 ... a variable would have already been reported) */
					return to_error_code(ERROR_FUNC_ASSIGNMENT, pos);
				}
				push_valstack(result, number, true, pos, tmpstack);
				free(tmpstr);
				break;
			case modop:
				if(stack_size(tmpstack) < 2) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				if(top_stack(tmpstack)->tp != number) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(INVALID_RIGHT_OPERAND, pos);
				}

				/* get value to modify variable by */
				mpfr_set(arg[1], SYNGE_T(top_stack(tmpstack)->val), SYNGE_ROUND);
				free_scontent(pop_stack(tmpstack));

				/* get variable to modify */
				if(top_stack(tmpstack)->tp != setword) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(INVALID_LEFT_OPERAND, pos);
				}

				/* get variable name */
				tmpstr = str_dup(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				/* check if it really is a variable */
				if(!ohm_search(variable_list, tmpstr, strlen(tmpstr) + 1)) {
					free(tmpstr);
					free_stackm(&tmpstack, rpn);
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
							free_stackm(&tmpstack, rpn);
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
							free_stackm(&tmpstack, rpn);
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
						free_stackm(&tmpstack, rpn);
						return to_error_code(UNKNOWN_TOKEN, pos);
						break;
				}

				/* set variable to new value */
				set_variable(tmpstr, *result);

				/* push new value of variable */
				push_valstack(result, number, true, pos, tmpstack);
				free(tmpstr);
				break;
			case premod:
				tmp = 1;
				/* pass-through */
			case postmod:
				if(stack_size(tmpstack) < 1) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				/* get variable to modify */
				if(top_stack(tmpstack)->tp != setword) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(INVALID_LEFT_OPERAND, pos);
				}

				tmpstr = str_dup(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				/* check if it really is a variable */
				if(!ohm_search(variable_list, tmpstr, strlen(tmpstr) + 1)) {
					free(tmpstr);
					free_stackm(&tmpstack, rpn);
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
						free_stackm(&tmpstack, rpn);
						return to_error_code(UNKNOWN_TOKEN, pos);
						break;
				}

				/* set variable to new value */
				set_variable(tmpstr, *result);

				/* push value of variable (depending on pre/post) */
				push_valstack(tmp ? result : num_dup(arg[0]), number, true, pos, tmpstack);

				if(!tmp) {
					mpfr_clear(*result);
					free(result);
				}
				free(tmpstr);
				break;
			case delop:
				if(stack_size(tmpstack) < 1) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				/* get word */
				if(top_stack(tmpstack)->tp != setword) {
					free(tmpexp);
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(INVALID_DELETE, pos);
				}

				tmpstr = str_dup(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

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
					free_stackm(&tmpstack, rpn);
					return ecode[1];
				}

				/* eval error check */
				if(!synge_is_success_code(ecode[0].code)) {
					mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
					free(result);
					free(tmpstr);
					free_stackm(&tmpstack, rpn);
					return to_error_code(ERROR_DELETE, pos);
				}

				push_valstack(result, number, true, pos, tmpstack);
				free(tmpstr);
				break;
			case userword:
				/* get word */
				tmp = 0;
				tmpstr = stackp.val;
				result = malloc(sizeof(synge_t));
				mpfr_init2(*result, SYNGE_PRECISION);

				ecode[0] = eval_word(tmpstr, pos, result);
				if(!synge_is_success_code(ecode[0].code)) {
					mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
					free(result);
					free_stackm(&tmpstack, rpn);
					return ecode[0];
				}

				/* push result of evaluation onto the stack */
				push_valstack(result, number, true, pos, tmpstack);
				break;
			case func:
				/* check if there is enough numbers for function arguments */
				if(stack_size(tmpstack) < 1) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(FUNCTION_WRONG_ARGC, pos);
				}

				/* get the first (and, for now, only) argument */
				mpfr_set(arg[0], SYNGE_T(top_stack(tmpstack)->val), SYNGE_ROUND);
				free_scontent(pop_stack(tmpstack));

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
				push_valstack(result, number, true, pos, tmpstack);
				break;
			case elseop:
				i++; /* skip past the if conditional */

				if(stack_size(tmpstack) < 3) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				if((*rpn)->content[i].tp != ifop) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(MISSING_IF, pos);
				}

				/* get else value */
				tmpelse = str_dup(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				/* get if value */
				tmpif = str_dup(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				/* get if condition */
				mpfr_set(arg[0], SYNGE_T(top_stack(tmpstack)->val), SYNGE_ROUND);
				free_scontent(pop_stack(tmpstack));

				result = malloc(sizeof(synge_t));
				mpfr_init2(*result, SYNGE_PRECISION);

				error_code tmpecode;

				/* set correct value */
				if(!iszero(arg[0]))
					/* if expression */
					tmpecode = eval_expression(tmpif, SYNGE_IF_BLOCK, (*rpn)->content[i].position, result);
				else
					/* else expression */
					tmpecode = eval_expression(tmpelse, SYNGE_ELSE_BLOCK, (*rpn)->content[i-1].position, result);

				free(tmpif);
				free(tmpelse);

				if(!synge_is_success_code(tmpecode.code)) {
					mpfr_clears(*result, arg[0], arg[1], arg[2], NULL);
					free(result);
					free_stackm(&tmpstack, rpn);
					return tmpecode;
				}

				push_valstack(result, number, true, pos, tmpstack);
				break;
			case ifop:
				/* ifop should never be found -- elseop always comes first in rpn stack */
				free_stackm(&tmpstack, rpn);
				mpfr_clears(arg[0], arg[1], arg[2], NULL);
				return to_error_code(MISSING_ELSE, pos);
				break;
			case bitop:
			case compop:
			case addop:
			case multop:
			case expop:
				/* check if there is enough numbers for operator "arguments" */
				if(stack_size(tmpstack) < 2) {
					free_stackm(&tmpstack, rpn);
					mpfr_clears(arg[0], arg[1], arg[2], NULL);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				/* get second argument */
				mpfr_set(arg[1], SYNGE_T(top_stack(tmpstack)->val), SYNGE_ROUND);
				free_scontent(pop_stack(tmpstack));

				/* get first argument */
				mpfr_set(arg[0], SYNGE_T(top_stack(tmpstack)->val), SYNGE_ROUND);
				free_scontent(pop_stack(tmpstack));

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
							free_stackm(&tmpstack, rpn);
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
							free_stackm(&tmpstack, rpn);
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
							mpfr_set_si(*result, cmp > 0, SYNGE_ROUND);
						}
						break;
					case op_gteq:
						{
							int cmp = mpfr_cmp(arg[0], arg[1]);
							mpfr_set_si(*result, cmp >= 0, SYNGE_ROUND);
						}
						break;
					case op_lt:
						{
							int cmp = mpfr_cmp(arg[0], arg[1]);
							mpfr_set_si(*result, cmp < 0, SYNGE_ROUND);
						}
						break;
					case op_lteq:
						{
							int cmp = mpfr_cmp(arg[0], arg[1]);
							mpfr_set_si(*result, cmp <= 0, SYNGE_ROUND);
						}
						break;
					case op_neq:
						{
							int cmp = mpfr_cmp(arg[0], arg[1]);
							mpfr_set_si(*result, cmp != 0, SYNGE_ROUND);
						}
						break;
					case op_eq:
						{
							int cmp = mpfr_cmp(arg[0], arg[1]);
							mpfr_set_si(*result, cmp == 0, SYNGE_ROUND);
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
						free_stackm(&tmpstack, rpn);
						return to_error_code(UNKNOWN_TOKEN, pos);
						break;
				}

				/* push result onto stack */
				push_valstack(result, number, true, pos, tmpstack);
				break;
			default:
				/* catch-all -- unknown token */
				free_stackm(&tmpstack, rpn);
				mpfr_clears(arg[0], arg[1], arg[2], NULL);
				return to_error_code(UNKNOWN_TOKEN, pos);
				break;
		}
	}
	mpfr_clears(arg[0], arg[1], arg[2], NULL);

	/* if there is not one item on the stack, there are too many values on the stack */
	if(stack_size(tmpstack) != 1) {
		free_stackm(&tmpstack, rpn);
		return to_error_code(TOO_MANY_VALUES, -1);
	}

	print_stack(tmpstack);

	/* otherwise, the last item is the result */
	mpfr_set(*output, SYNGE_T(tmpstack->content[0].val), SYNGE_ROUND);

	free_stackm(&tmpstack, rpn);
	return to_error_code(SUCCESS, -1);
} /* synge_eval_rpnstack() */

char *get_trace(link_t *link) {
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

char *get_error_type(error_code error) {
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
			msg = "Cannot divide by zero";
			break;
		case MODULO_BY_ZERO:
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
			msg = "Expression was empty";
			break;
		case NUM_OVERFLOW:
			msg = "Number caused overflow";
			break;
		case UNDEFINED:
			msg = "Result is undefined";
			break;
		case TOO_DEEP:
			msg = "Delved too deep";
			break;
		default:
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

error_code synge_internal_compute_string(char *original_str, synge_t *result, char *caller, int position) {
	static int depth = -1;
	assert(synge_started);

	if(ohm_count(variable_list) > ohm_size(variable_list))
		/* "dynamically" resize hashmap to keep efficiency up */
		variable_list = ohm_resize(variable_list, ohm_size(variable_list) * 2);

	ohm_t *backup_var = ohm_dup(variable_list);
	ohm_t *backup_func = ohm_dup(function_list);

	/* intiialise result to zero */
	mpfr_set_si(*result, 0, SYNGE_ROUND);

	/*
	 * We have delved too greedily and too deep.
	 * We have awoken a creature in the darkness of recursion.
	 * A creature of shadow, flame and segmentation faults.
	 * YOU SHALL NOT PASS!
	 */

	if(++depth >= SYNGE_MAX_DEPTH)
		return to_error_code(TOO_DEEP, -1);

	if(!strcmp(caller, SYNGE_MAIN)) {
		/* reset traceback */
		link_free(traceback_list);
		traceback_list = link_init();

		depth = -1;
	}

	/* add current level to traceback */
	char *to_add = malloc(lenprintf(SYNGE_TRACEBACK_TEMPLATE, caller, position));
	sprintf(to_add, SYNGE_TRACEBACK_TEMPLATE, caller, position);

	link_append(traceback_list, to_add, strlen(to_add) + 1);

	free(to_add);

	debug("depth %d with caller %s\n", depth, caller);
	debug(" - expression '%s'\n", original_str);

	/* initialise all local variables */
	stack *rpn_stack = malloc(sizeof(stack)), *infix_stack = malloc(sizeof(stack));
	init_stack(rpn_stack);
	init_stack(infix_stack);
	error_code ecode = to_error_code(SUCCESS, -1);

	/* process string */
	char *final_pass_str = str_dup(original_str);
	char *string = final_pass_str;

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

	/* if some error occured, revert variables and functions back to a previous state */
	if(!synge_is_success_code(ecode.code) && !synge_is_ignore_code(ecode.code)) {
		ohm_cpy(variable_list, backup_var);
		ohm_cpy(function_list, backup_func);
	}

	/* if everythin went well, set the answer variable (and remove current depth from traceback) */
	if(synge_is_success_code(ecode.code)) {
		mpfr_set(prev_answer, *result, SYNGE_ROUND);
		link_pend(traceback_list);
	}

	/* free memory */
	ohm_free(backup_var);
	ohm_free(backup_func);

	free(final_pass_str);
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

void synge_start(void) {
	variable_list = ohm_init(2, NULL);
	function_list = ohm_init(2, NULL);
	traceback_list = link_init();

	mpfr_init2(prev_answer, SYNGE_PRECISION);
	mpfr_set_si(prev_answer, 0, SYNGE_ROUND);

	synge_started = true;
} /* synge_end() */

void synge_end(void) {
	assert(synge_started);

	/* mpfr_free variables */
	ohm_iter i = ohm_iter_init(variable_list);
	for(; i.key != NULL; ohm_iter_inc(&i))
		mpfr_clear(i.value);

	ohm_free(variable_list);
	ohm_free(function_list);
	link_free(traceback_list);
	free(error_msg_container);

	mpfr_clears(prev_answer, NULL);
	mpfr_free_cache();

	synge_started = false;
} /* synge_end() */

void synge_reset_traceback(void) {
	assert(synge_started);

	/* free previous traceback and allocate new one */
	if(traceback_list)
		link_free(traceback_list);

	traceback_list = link_init();

	/* reset traceback to base notation */
	int len = lenprintf(SYNGE_TRACEBACK_TEMPLATE, SYNGE_MAIN, 0);
	char *tmp = malloc(len);
	sprintf(tmp, SYNGE_TRACEBACK_TEMPLATE, SYNGE_MAIN, 0);

	link_append(traceback_list, tmp, len);

	free(tmp);
} /* synge_reset_traceback() */

int synge_is_success_code(int code) {
	return (code == SUCCESS);
} /* is_success_code() */

int synge_is_ignore_code(int code) {
	return (code == EMPTY_STACK || code == ERROR_FUNC_ASSIGNMENT || code == ERROR_DELETE);
} /* ignore_code() */
