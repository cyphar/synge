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
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>

#include "stack.h"
#include "synge.h"
#include "ohmic.h"
#include "linked.h"

/* Operator Precedence
 * 4 Parenthesis
 * 3 Exponents, Roots (right associative)
 * 2 Multiplication, Division, Modulo (left associative)
 * 1 Addition, Subtraction (left associative)
 */

#define SYNGE_EPSILON		10e-14
#define SYNGE_MAX_PRECISION	13
#define SYNGE_MAX_DEPTH		10

#define SYNGE_PREV_ANSWER	"ans"
#define SYNGE_VARIABLE_CHARS	"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ\'\"_"
#define SYNGE_FUNCTION_CHARS	"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ0123456789\'\"_"

#define SYNGE_TRACEBACK_FORMAT	"Synge Traceback (most recent call last):\n" \
				"%s" \
				"%s: %s"

#define SYNGE_TRACEBACK_TEMPLATE	"  Function %s, at %d\n"

#define PI 3.14159265358979323

#define strlower(x) do { char *p = x; for(; *p; ++p) *p = tolower(*p); } while(0)
#define strupper(x) do { char *p = x; for(; *p; ++p) *p = toupper(*p); } while(0)

/* to make changes in engine types smoother */
#define sy_fabs(...) fabsl(__VA_ARGS__)
#define sy_modf(...) modfl(__VA_ARGS__)
#define sy_fmod(...) fmodl(__VA_ARGS__)

/* my own assert() implementation */
#define assert(x) do { if(!x) { printf("synge: assertion '%s' failed -- aborting!\n", #x); exit(254); }} while(0)

/* -- useful macros -- */
#define isaddop(str) (get_op(str).tp == op_plus || get_op(str).tp == op_minus)
#define issetop(str) (get_op(str).tp == op_var_set || get_op(str).tp == op_func_set)

#define isop(type) (type == addop || type == multop || type == expop || type == compop || type == bitop || type == setop)
#define isnumword(type) (type == number || type == userword)

/* checks if a synge_t is technically zero */
#define iszero(x) (sy_fabs(x) <= SYNGE_EPSILON)

/* when a floating point number has a rounding error, weird stuff starts to happen -- reliable bug */
#define has_rounding_error(number) ((number + 1) == number || (number - 1) == number)

/* hack to get amount of memory needed to store a sprintf() */
#define lenprintf(...) (snprintf(NULL, 0, __VA_ARGS__) + 1)

/* macros for casting void stack pointers */
#define SYNGE_T(x) (*(synge_t *) x)
#define FUNCTION(x) ((function *) x)

static int synge_started = false; /* I REALLY recommend you leave this false, as this is used to ensure that synge_start has been run */

synge_t deg2rad(synge_t deg) {
	return deg * (PI / 180.0);
} /* deg2rad() */

synge_t rad2deg(synge_t rad) {
	return rad * (180.0 / PI);
} /* rad2deg() */

synge_t sy_rand(synge_t to) {
	int min = 0;
	int max = (int) floor(to);
	/* better than the standard skewed (rand() % max + min + 1) range */
	return (rand() % (max + 1 - min)) + min;
} /* sy_rand() */

synge_t sy_factorial(synge_t x) {
	synge_t number = floor(x);
	synge_t factorial = number;
	while(number > 1.0)
		factorial *= --number;
	return factorial;
} /* sy_factorial() */

synge_t sy_series(synge_t x) {
	x = floor(x);
	/* an epic formula I learnt in year 5 */
	return (x * (x+1)) / 2;
} /* sy_series() */

synge_t sy_assert(synge_t x) {
	return iszero(x) ? 0.0 : 1.0;
} /* sy_assert */

static function func_list[] = {
	{"abs",		fabsl,		"abs(x)",	"Absolute value of x"},
	{"sqrt",	sqrtl,		"sqrt(x)",	"Square root of x"},
	{"cbrt",	cbrtl,		"cbrt(x)",	"Cubic root of x"},

	{"floor",	floorl,		"floor(x)",	"Largest integer not greater than x"},
	{"round",	roundl,		"round(x)",	"Closest integer to x"},
	{"ceil",	ceill,		"ceil(x)",	"Smallest integer not smaller than x"},

	{"log10",	log10l,		"log10(x)",	"Base 10 logarithm of x"},
	{"log",		log2l,		"log(x)",	"Base 2 logarithm of x"},
	{"ln",		logl,		"ln(x)",	"Base e logarithm of x"},

	{"rand",	sy_rand,	"rand(x)",	"Generate a psedu-random integer between 0 and floor(x)"},
	{"fact",	sy_factorial,	"fact(x)",	"Factorial of floor(x)"},
	{"series",	sy_series,	"series(x)",	"Gives addition of all integers up to floor(x)"},
	{"assert",	sy_assert,	"assert(x)",	"Returns 0 is x is 0, and returns 1 otherwise"},

	{"deg2rad",   	deg2rad,	"deg2rad(x)",	"Convert x degrees to radians"},
	{"rad2deg",   	rad2deg,	"rad2deg(x)",	"Convert x radians to degrees"},

	{"sinhi",	asinhl,		"asinh(x)",	"Inverse hyperbolic sine of x"},
	{"coshi",	acoshl,		"acosh(x)",	"Inverse hyperbolic cosine of x"},
	{"tanhi",	atanhl,		"atanh(x)",	"Inverse hyperbolic tangent of x"},
	{"sinh",	sinhl,		"sinh(x)",	"Hyperbolic sine of x"},
	{"cosh",	coshl,		"cosh(x)",	"Hyperbolic cosine of x"},
	{"tanh",	tanhl,		"tanh(x)",	"Hyperbolic tangent of x"},

	{"sini",	asinl,		"asin(x)",	"Inverse sine of x"},
	{"cosi",	acosl,		"acos(x)",	"Inverse cosine of x"},
	{"tani",	atanl,		"atan(x)",	"Inverse tangent of x"},
	{"sin",		sinl,		"sin(x)",	"Sine of x"},
	{"cos",		cosl,		"cos(x)",	"Cosine of x"},
	{"tan",		tanl,		"tan(x)",	"Tangent of x"},
	{NULL,		NULL,		NULL,		NULL}
};

typedef struct __function_alias__ {
	char *alias;
	char *function;
} function_alias;

static function_alias alias_list[] = {
	{"asinh",	"sinhi"},
	{"acosh",	"coshi"},
	{"atanh",	"tanhi"},
	{"asin",	 "sini"},
	{"acos",	 "cosi"},
	{"atan",	 "tani"},
	{NULL,		   NULL},
};

typedef struct __special_number__ {
	char *name;
	synge_t value;
} special_number;

static ohm_t *variable_list = NULL;
static ohm_t *function_list = NULL;

static special_number constant_list[] = {
	{"pi",			3.14159265358979323},
	{"phi",			1.61803398874989484},
	{"e",			2.71828182845904523},
	{"life",			       42.0}, /* Sorry, I couldn't resist */

	{"true",				1.0},
	{"false",				0.0},

	{SYNGE_PREV_ANSWER,			0.0},
	{NULL,					0.0},
};

typedef struct operator {
	char *str;
	enum {
		op_plus,
		op_minus,
		op_mult,
		op_div,
		op_intdiv,
		op_mod,
		op_pow,
		op_lparen,
		op_rparen,
		op_greater,
		op_less,
		op_not,
		op_band,
		op_bor,
		op_bxor,
		op_if,
		op_else,
		op_var_set,
		op_func_set,
		op_none
	} tp;
} operator;

/* used for when a (char *) is needed, but needn't be freed */
static operator op_list[] = {
	{"+",	op_plus},
	{"-",	op_minus},
	{"*",	op_mult},
	{"/",	op_div},
	{"//",	op_intdiv}, /* integer division */
	{"%",	op_mod},
	{"^",	op_pow},
	{"(",	op_lparen},
	{")",	op_rparen},

	/* comparison operators */
	{">",	op_greater},
	{"<",	op_less},
	{"!",	op_not},

	/* bitwise operators */
	{"&",	op_band},
	{"|",	op_bor},
	{"^",	op_bxor},

	/* tertiary operators */
	{"?",	op_if},
	{":",	op_else},

	/* assignment operators */
	{"=",	op_var_set},
	{":=",	op_func_set},

	/* null terminator */
	{NULL,	op_none}
};

/* internal stack types */
typedef enum __stack_type__ {
	number,

	setop	= 1,
	ifop	= 2,
	elseop	= 3,
	bitop	= 4,
	compop	= 5,
	addop	= 6,
	multop	= 7,
	expop	= 8,

	func,
	userword, /* user function or variable */
	setword, /* user function or variable to be set */
	expression, /* saved expression */

	lparen,
	rparen,
} s_type;

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
#ifdef __DEBUG__
	int i, size = stack_size(s);
	for(i = 0; i < size; i++) {
		s_content tmp = s->content[i];
		if(!tmp.val) continue;

		if(tmp.tp == number)
			printf("%" SYNGE_FORMAT " ", SYNGE_T(tmp.val));
		else if(tmp.tp == func)
			printf("%s ", FUNCTION(tmp.val)->name);
		else
			printf("%s ", (char *) tmp.val);
	}
	printf("\n");
#endif
} /* print_stack() */

void debug(char *format, ...) {
#ifdef __DEBUG__
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
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

	debug("original: '%s'\nnew: '%s'\n", str, ret);
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
	*ret = num;
	return ret;
} /* num_dup() */

char *str_dup(char *s) {
	char *ret = malloc(strlen(s) + 1);
	memcpy(ret, s, strlen(s) + 1);
	return ret;
} /* str_dup() */

int get_precision(synge_t num) {
	/* use the current settings' precision if given */
	if(active_settings.precision >= 0)
		return active_settings.precision;

	/* printf knows how to fix rounding errors -- WARNING: here be dragons! */
	int tmpsize = lenprintf("%.*" SYNGE_FORMAT, SYNGE_MAX_PRECISION, num); /* get the amount of memory needed to store this printf*/
	char *tmp = malloc(tmpsize);
	sprintf(tmp, "%.*" SYNGE_FORMAT, SYNGE_MAX_PRECISION, num); /* sprintf it */

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
	special_number ret = {NULL, 0.0};
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

bool isnum(char *string) {
	int ret = false;
	char *endptr = NULL;
	char *s = get_word(string, SYNGE_VARIABLE_CHARS, &endptr);

	endptr = NULL;
	strtold(string, &endptr);

	/* all cases where word is a number */
	ret = (get_special_num(s).name || string != endptr);
	free(s);
	return ret;
} /* isnum() */

void set_special_number(char *s, synge_t val, special_number *list) {
	/* specifically made for the ans "variable" -- use the hashmap for real variables */
	int i;
	for(i = 0; list[i].name != NULL; i++)
		if(!strcmp(list[i].name, s)) list[i].value = val;
} /* set_special_number() */

error_code set_variable(char *str, synge_t val) {
	assert(synge_started);

	error_code ret = to_error_code(SUCCESS, -1);
	char *endptr = NULL, *s = get_word(str, SYNGE_FUNCTION_CHARS, &endptr);

	if(get_special_num(s).name) {
		/* name is reserved -- give error */
		ret = to_error_code(RESERVED_VARIABLE, -1);
	} else {
		ohm_remove(function_list, s, strlen(s) + 1); /* remove word from function list (fake dynamic typing) */
		ohm_insert(variable_list, s, strlen(s) + 1, &val, sizeof(val));
	}

	free(s);
	return ret;
} /* set_variable() */

error_code set_function(char *str, char *exp) {
	assert(synge_started);

	error_code ret = to_error_code(SUCCESS, -1);
	char *endptr = NULL, *s = get_word(str, SYNGE_FUNCTION_CHARS, &endptr);

	if(get_special_num(s).name) {
		/* name is reserved -- give error */
		ret = to_error_code(RESERVED_VARIABLE, -1);
	} else {
		ohm_remove(variable_list, s, strlen(s) + 1); /* remove word from variable list (fake dynamic typing) */
		ohm_insert(function_list, s, strlen(s) + 1, exp, strlen(exp) + 1);
	}

	free(s);
	return ret;
} /* set_variable() */

error_code del_word(char *str, bool variable) {
	assert(synge_started);

	char *endptr = NULL, *s = get_word(str, SYNGE_FUNCTION_CHARS, &endptr);
	error_code ret = to_error_code(variable ? DELETED_VARIABLE : DELETED_FUNCTION, -1);

	if(get_special_num(s).name)
		/* name is reserved -- give error */
		ret = to_error_code(RESERVED_VARIABLE, -1);

	else if(!ohm_search(variable_list, s, strlen(s) + 1) &&
		!ohm_search(function_list, s, strlen(s) + 1))
		/* word doesn't exist */
		ret = to_error_code(variable ? UNKNOWN_VARIABLE : UNKNOWN_FUNCTION, -1);

	else {
		/* try to remove it from both lists -- just to be sure */
		if(!ohm_remove(variable_list, s, strlen(s) + 1))
			ret.code = DELETED_VARIABLE;
		if(!ohm_remove(function_list, s, strlen(s) + 1))
			ret.code = DELETED_FUNCTION;
	}

	free(s);
	return ret;
} /* del_variable() */

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

		if(strcmp(final, tmpfinal))
			debug("(%s)\t%s => %s\n", func_list[i].name, final, tmpfinal);

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

#define false_number(str, stack)	(!(!top_stack(stack) || /* first token is a number */ \
					(((!isnumword(top_stack(stack)->tp)) || !isaddop(str)) && /* a number beginning with +/- and preceeded by a number is not a number */ \
					(top_stack(stack)->tp != rparen || !isaddop(str))))) /* a number beginning with +/- and preceeded by a ')' is not a number */

error_code tokenise_string(char *string, stack **infix_stack) {
	assert(synge_started);
	char *s = function_process_replace(string);

	debug("%s\n%s\n", string, s);

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

			/* if it is a "special" number */
			if(get_special_num(word).name) {
				special_number stnum = get_special_num(word);
				*num = stnum.value;
				tmpoffset = strlen(stnum.name); /* update iterator to correct offset */
			}
			else {
				char *endptr;
				*num = strtold(s+i, &endptr);
				tmpoffset = endptr - (s + i); /* update iterator to correct offset */
			}
			push_valstack(num, number, true, pos, *infix_stack); /* push given value */

			/* error detection (done per number to ensure numbers are 163% correct) */
			if(has_rounding_error(*num)) {
				free(s);
				free(word);
				return to_error_code(NUM_OVERFLOW, pos);
			}
			else if(*num != *num) {
				free(s);
				free(word);
				return to_error_code(UNDEFINED, pos);
			}
		}

		else if(get_op(s+i).str) {
			int postpush = false;
			s_type type;
			/* find and set type appropriate to operator */
			switch(get_op(s+i).tp) {
				case op_plus:
				case op_minus:
					type = addop;
					break;
				case op_mult:
				case op_div:
				case op_intdiv: /* integer division -- like in python */
				case op_mod:
					type = multop;
					break;
				case op_pow:
					type = expop;
					break;
				case op_lparen:
					type = lparen;
					pos--;  /* "hack" to ensure error position is correct */
					/* every open paren with no operators before it has an implied * */
					if(top_stack(*infix_stack) && (top_stack(*infix_stack)->tp == number || top_stack(*infix_stack)->tp == rparen))
						push_valstack("*", multop, false, pos + 1, *infix_stack);
					break;
				case op_rparen:
					type = rparen;
					pos--;  /* "hack" to ensure error position is correct */
					if(nextpos > 0 && isnum(s + nextpos) && !get_op(s + nextpos).str)
						postpush = true;
					break;
				case op_greater:
				case op_less:
				case op_not:
					type = compop;
					break;
				case op_band:
				case op_bor:
				case op_bxor:
					type = bitop;
					break;
				case op_if:
					type = ifop;
					break;
				case op_else:
					type = elseop;
					break;
				case op_var_set:
				case op_func_set:
					type = setop;
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

				char *p = s + i + strlen(get_op(s+i).str), *expr = NULL;
				int num_paren = 1, len = 0;

				while(*p && (*p != ')' || (num_paren && *p == ')'))) {
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

					expr = realloc(expr, ++len);
					expr[len - 1] = *p;

					p++;
				}
				expr = realloc(expr, len + 1);
				expr[len] = '\0';

				debug("expression: %s\n", expr);

				push_valstack(expr, expression, true, next_offset(s, i + strlen(get_op(s+i).str)), *infix_stack);
				tmpoffset = strlen(expr);
			}

			/* update iterator */
			tmpoffset += strlen(get_op(s+i).str);
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
				/* make variables act more like numbers (and more like variables) */
				switch(top_stack(*infix_stack)->tp) {
					case addop:
						/* if there is a +/- in front of a variable, it should set the sign of that variable (i.e. 1--x is 1+x) */
						tmppop = pop_stack(*infix_stack); /* the sign (to be saved for later) */
						tmpp = top_stack(*infix_stack);
						if(!tmpp || (tmpp->tp != number && tmpp->tp != rparen)) { /* sign is to be discarded */
							if(((char *) tmppop->val)[0] == '-') {
								/* negate the variable? */
								push_valstack("+", addop, false, pos, *infix_stack);
								push_valstack(num_dup(0), number, true, pos, *infix_stack);
								push_valstack("-", addop, false, pos, *infix_stack);
							} else {
								/* otherwise, add the variable */
								push_valstack("+", addop, false, pos, *infix_stack);
							}
						}
						else
							/* whoops! didn't match criteria. push sign back. */
							push_ststack(*tmppop, *infix_stack);
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
			if(nextpos > 0 && issetop(s + nextpos))
				type = setword;

			debug("found word '%s'\n", word);

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

		i += tmpoffset - 1;
	}

	free(s);
	if(!stack_size(*infix_stack))
		return to_error_code(EMPTY_STACK, -1); /* stack was empty */

	/* debugging */
	print_stack(*infix_stack);

	return to_error_code(SUCCESS, -1);
} /* tokenise_string() */

bool op_precedes(s_type op1, s_type op2) {
	/* returns true if:
	 * 	op1 > op2 (or op1 > op2 - 0)
	 * 	op2 is left associative and op1 >= op2 (or op1 > op2 - 1)
	 * returns false otherwise
	 */
	int lassoc;
	/* here be dragons! obscure integer hacks follow. */
	switch(op2) {
		case ifop:
		case elseop:
		case bitop:
		case compop:
		case addop:
		case multop:
			lassoc = 1;
			break;
		case setop:
		case expop:
			lassoc = 0;
			break;
		default:
			/* catchall -- i don't even ... */
			return false; /* what the hell are you doing? */
			break;
	}
	return (op1 > op2 - lassoc);
} /* op_precedes() */

/* my implementation of Dijkstra's really cool shunting-yard algorithm */
error_code shunting_yard_parse(stack **infix_stack, stack **rpn_stack) {
	s_content stackp, *tmpstackp;
	stack *op_stack = malloc(sizeof(stack));

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
			case setop:
			case elseop:
			case ifop:
			case bitop:
			case compop:
			case addop:
			case multop:
			case expop:
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
synge_t settings_to_rad(synge_t in) {
	switch(active_settings.mode) {
		case degrees:
			return deg2rad(in);
		case radians:
			return in;
	}

	/* catch-all -- wtf? */
	return 0.0;
} /* settings_to_rad() */

/* functions' whose output is in radians */
char *angle_outfunc_list[] = {
	"sini",
	"cosi",
	"tani",
	NULL
};

/* convert radians to set mode */
synge_t rad_to_settings(synge_t in) {
	switch(active_settings.mode) {
		case degrees:
			return rad2deg(in);
		case radians:
			return in;
	}

	/* catch-all -- wtf? */
	return 0.0;
} /* rad_to_settings() */

error_code eval_word(char *str, int pos, synge_t *result) {
	int tmp = 0;

	/* is it a legitamate variable or function? */
	if((ohm_search(variable_list, str, strlen(str) + 1) &&  (tmp = 1)) ||
	   (ohm_search(function_list, str, strlen(str) + 1) && !(tmp = 0))) {

		if(tmp) {
			/* variable */
			*result = SYNGE_T(ohm_search(variable_list, str, strlen(str) + 1));
		} else {
			/* function */
			/* recursively evaluate a user function's value */
			char *expression = ohm_search(function_list, str, strlen(str) + 1);
			error_code tmpecode = internal_compute_infix_string(expression, result, str, pos);

			/* error was encountered */
			if(!is_success_code(tmpecode.code)) {
				if(active_settings.error == traceback)
					/* return real error code for traceback */
					return tmpecode;
				else
					/* return relative error code for all other error formats */
					return to_error_code(tmpecode.code, pos);
			}
		}

		/* is the result a nan? */
		if(*result != *result)
			return to_error_code(UNDEFINED, pos);
	} else {
		/* unknown variable or function */
		return to_error_code(UNKNOWN_TOKEN, pos);
	}

	return to_error_code(SUCCESS, -1);
} /* eval_word() */

/* evaluate a rpn stack */
error_code eval_rpnstack(stack **rpn, synge_t *output) {
	stack *tmpstack = malloc(sizeof(stack));
	init_stack(tmpstack);

	s_content stackp;

	char *tmpstr = NULL, *tmpexp = NULL;
	int i, pos = 0, tmp = 0, size = stack_size(*rpn);
	synge_t *result = NULL, arg[3];
	error_code tmpecode;

	for(i = 0; i < size; i++) {
		/* debugging */
		print_stack(tmpstack);
		print_stack(*rpn);

		stackp = (*rpn)->content[i]; /* shorthand for current stack item */
		pos = stackp.position; /* shorthand for current error position */

		tmp = 0;
		tmpstr = tmpexp = NULL;

		switch(stackp.tp) {
			case number:
				/* just push it onto the final stack */
				push_valstack(num_dup(SYNGE_T(stackp.val)), number, true, pos, tmpstack);
				break;
			case expression:
			case setword:
				debug("setword '%s'\n", stackp.val);
				/* just push it onto the final stack */
				push_valstack(str_dup(stackp.val), stackp.tp, true, pos, tmpstack);
				break;
			case setop:
				if(stack_size(tmpstack) < 2) {
					free_stackm(&tmpstack, rpn);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				/* get new value for word */
				if(top_stack(tmpstack)->tp == number)
					arg[0] = SYNGE_T(top_stack(tmpstack)->val);
				else
					tmpexp = str_dup(top_stack(tmpstack)->val);

				free_scontent(pop_stack(tmpstack));

				/* get word */
				if(top_stack(tmpstack)->tp != setword) {
					free(tmpexp);
					free_stackm(&tmpstack, rpn);
					return to_error_code(INVALID_ASSIGNMENT, pos);
				}

				tmpstr = str_dup(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				/* set variable or function */
				switch(get_op(stackp.val).tp) {
					case op_var_set:
						debug("setting variable '%s' -> %" SYNGE_FORMAT "\n", tmpstr, arg[0]);
						set_variable(tmpstr, arg[0]);
						break;
					case op_func_set:
						debug("setting function '%s' -> '%s'\n", tmpstr, tmpexp);
						set_function(tmpstr, tmpexp);
						break;
					default:
						free(tmpstr);
						free(tmpexp);
						free_stackm(&tmpstack, rpn);
						return to_error_code(INVALID_ASSIGNMENT, pos);
						break;
				}

				free(tmpexp);

				/* evaulate and push the value of set word */
				result = malloc(sizeof(synge_t));
				tmpecode = eval_word(tmpstr, pos, result);
				if(!is_success_code(tmpecode.code)) {
					free(result);
					free(tmpstr);
					free_stackm(&tmpstack, rpn);
					return tmpecode;
				}
				push_valstack(result, number, true, pos, tmpstack);
				free(tmpstr);
				break;
			case userword:
				/* get word */
				tmp = 0;
				tmpstr = stackp.val;
				result = malloc(sizeof(synge_t));

				tmpecode = eval_word(tmpstr, pos, result);
				if(!is_success_code(tmpecode.code)) {
					free(result);
					free_stackm(&tmpstack, rpn);
					return tmpecode;
				}

				/* push result of evaluation onto the stack */
				push_valstack(result, number, true, pos, tmpstack);
				break;
			case func:
				/* check if there is enough numbers for function arguments */
				if(stack_size(tmpstack) < 1) {
					free_stackm(&tmpstack, rpn);
					return to_error_code(FUNCTION_WRONG_ARGC, pos);
				}

				/* get the first (and, for now, only) argument */
				arg[0] = SYNGE_T(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				/* does the input need to be converted? */
				if(get_from_ch_list(FUNCTION(stackp.val)->name, angle_infunc_list)) /* convert settings angles to radians */
					arg[0] = settings_to_rad(arg[0]);

				/* allocate result and evaluate it */
				result = malloc(sizeof(synge_t));
				*result = FUNCTION(stackp.val)->get(arg[0]);

				/* does the output need to be converted? */
				if(get_from_ch_list(FUNCTION(stackp.val)->name, angle_outfunc_list)) /* convert radians to settings angles */
					*result = rad_to_settings(*result);

				/* push result of evaluation onto the stack */
				push_valstack(result, number, true, pos, tmpstack);
				break;
			case elseop:
				i++; /* skip past the if conditional */

				if(stack_size(tmpstack) < 3) {
					free_stackm(&tmpstack, rpn);
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				if((*rpn)->content[i].tp != ifop) {
					free_stackm(&tmpstack, rpn);
					return to_error_code(MISSING_IF, pos);
				}


				/* get else value */
				arg[0] = SYNGE_T(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				/* get if value */
				arg[1] = SYNGE_T(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				/* get if condition */
				arg[2] = SYNGE_T(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				result = malloc(sizeof(synge_t));

				/* set correct value */
				if(iszero(arg[2]))
					*result = arg[0];
				else
					*result = arg[1];

				push_valstack(result, number, true, pos, tmpstack);
				break;
			case ifop:
				free_stackm(&tmpstack, rpn);
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
					return to_error_code(OPERATOR_WRONG_ARGC, pos);
				}

				/* get second argument */
				arg[1] = SYNGE_T(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				/* get first argument */
				arg[0] = SYNGE_T(top_stack(tmpstack)->val);
				free_scontent(pop_stack(tmpstack));

				result = malloc(sizeof(synge_t));
				/* find correct evaluation and do it */
				switch(get_op((char *) stackp.val).tp) {
					case op_plus:
						*result = arg[0] + arg[1];
						break;
					case op_minus:
						*result = arg[0] - arg[1];
						break;
					case op_mult:
						*result = arg[0] * arg[1];
						break;
					case op_intdiv:
						/* division, but the result ignores the decimals */
						tmp = 1;
					case op_div:
						if(iszero(arg[1])) {
							/* the 11th commandment -- thoust shalt not divide by zero */
							free(result);
							free_stackm(&tmpstack, rpn);
							return to_error_code(DIVIDE_BY_ZERO, pos);
						}
						*result = arg[0] / arg[1];
						if(tmp)
							sy_modf(*result, result);
						break;
					case op_mod:
						if(iszero(arg[1])) {
							/* the 11.5th commandment -- thoust shalt not modulo by zero */
							free(result);
							free_stackm(&tmpstack, rpn);
							return to_error_code(MODULO_BY_ZERO, pos);
						}
						*result = sy_fmod(arg[0], arg[1]);
						break;
					case op_pow:
						*result = pow(arg[0], arg[1]);
						break;
					case op_greater:
						*result = arg[0] > arg[1];
						break;
					case op_less:
						*result = arg[0] < arg[1];
						break;
					case op_not:
						*result = arg[0] != arg[1];
						break;
					case op_band:
						*result = (int) arg[0] & (int) arg[1];
						break;
					case op_bor:
						*result = (int) arg[0] | (int) arg[1];
						break;
					case op_bxor:
						*result = (int) arg[0] ^ (int) arg[1];
						break;
					default:
						/* catch-all -- unknown token */
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
				return to_error_code(UNKNOWN_TOKEN, pos);
				break;
		}

		if(top_stack(tmpstack) && top_stack(tmpstack)->tp == number) {
			/* check if a rounding error occured in above operation */
			synge_t tmp = SYNGE_T(top_stack(tmpstack)->val);
			if(has_rounding_error(tmp)) {
				free_stackm(&tmpstack, rpn);
				return to_error_code(NUM_OVERFLOW, pos);
			}
		}
	}

	/* if there is not one item on the stack, there are too many values on the stack */
	if(stack_size(tmpstack) != 1) {
		free_stackm(&tmpstack, rpn);
		return to_error_code(TOO_MANY_VALUES, -1);
	}

	/* otherwise, the last item is the result */
	*output = SYNGE_T(top_stack(tmpstack)->val);

	/* check for rounding errors */
	if(has_rounding_error(*output)) {
		free_stackm(&tmpstack, rpn);
		return to_error_code(NUM_OVERFLOW, -1);
	}

	free_stackm(&tmpstack, rpn);
	return to_error_code(SUCCESS, -1);
} /* eval_rpnstack() */

char *get_trace(link_t *link) {
	char *ret = str_dup(""), *current = NULL;
	link_iter *ii = link_iter_init(link);

	int len = 0;
	do {
		/* get current function traceback information */
		current = (char *) ii->content;
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

char *get_error_tp(error_code error) {
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
		case INVALID_ASSIGNMENT:
			return "SyntaxError";
			break;
		case UNKNOWN_TOKEN:
		case UNKNOWN_VARIABLE:
		case UNKNOWN_FUNCTION:
		case INVALID_VARIABLE_CHAR:
		case EMPTY_VARIABLE_NAME:
		case RESERVED_VARIABLE:
			return "NameError";
			break;
		case DELETED_VARIABLE:
		case DELETED_FUNCTION:
			return "FalsePositive";
			break;
		case TOO_DEEP:
		default:
			return "OtherError";
			break;

	}
	return "IHaveNoIdea";
}

char *get_error_msg(error_code error) {
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
		case INVALID_ASSIGNMENT:
			msg = "Invalid left operand of assignment";
			break;
		case UNKNOWN_VARIABLE:
			msg = "Unknown variable to delete";
			break;
		case UNKNOWN_FUNCTION:
			msg = "Unknown function to delete";
			break;
		case FUNCTION_WRONG_ARGC:
			msg = "Not enough arguments for function";
			break;
		case OPERATOR_WRONG_ARGC:
			msg = "Not enough values for operator";
			break;
		case MISSING_IF:
			msg = "Missing if conditional for else";
			break;
		case MISSING_ELSE:
			msg = "Missing else statement for if";
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
		case INVALID_VARIABLE_CHAR:
			msg = "Invalid character in variable name";
			break;
		case EMPTY_VARIABLE_NAME:
			msg = "Empty variable name";
			break;
		case RESERVED_VARIABLE:
			msg = "Variable name is reserved";
			break;
		case DELETED_VARIABLE:
			msg = "Variable deleted";
			break;
		case DELETED_FUNCTION:
			msg = "Function deleted";
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
			if(!is_success_code(error.code)) {
				char *fulltrace = get_trace(traceback_list);

				trace = malloc(lenprintf(SYNGE_TRACEBACK_FORMAT, fulltrace, get_error_tp(error), msg));
				sprintf(trace, SYNGE_TRACEBACK_FORMAT, fulltrace, get_error_tp(error), msg);

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

char *get_error_msg_pos(int code, int pos) {
	return get_error_msg(to_error_code(code, pos));
} /* get_error_msg_pos() */

error_code internal_compute_infix_string(char *original_str, synge_t *result, char *caller, int position) {
	static int depth = -1;
	assert(synge_started);

	if(variable_list->count > variable_list->size)
		/* "dynamically" resize hashmap to keep efficiency up */
		variable_list = ohm_resize(variable_list, variable_list->size * 2);

	/* intiialise result to 0.0 */
	*result = 0.0;

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

	/* initialise all local variables */
	stack *rpn_stack = malloc(sizeof(stack)), *infix_stack = malloc(sizeof(stack));
	init_stack(rpn_stack);
	init_stack(infix_stack);
	error_code ecode = to_error_code(SUCCESS, -1);

	/* process string */
	char *final_pass_str = str_dup(original_str);
	char *string = final_pass_str;

	if(ecode.code == SUCCESS) {
		/* generate infix stack */
		if((ecode = tokenise_string(string, &infix_stack)).code == SUCCESS)
			/* convert to postfix (or RPN) stack */
			if((ecode = shunting_yard_parse(&infix_stack, &rpn_stack)).code == SUCCESS)
				/* evaluate postfix (or RPN) stack */
				if((ecode = eval_rpnstack(&rpn_stack, result)).code == SUCCESS)
					/* fix up negative zeros */
					if(*result == abs(*result))
						*result = abs(*result);
	}

	/* measure depth, not length */
	depth--;

	/* is it a nan? */
	if(*result != *result)
		ecode = to_error_code(UNDEFINED, -1);

	/* FINALLY, set the answer variable */
	if(is_success_code(ecode.code)) {
		set_special_number(SYNGE_PREV_ANSWER, *result, constant_list);
		/* and remove last item from traceback - no errors occured */
		link_pend(traceback_list);
	}

	free(final_pass_str);
	free_stackm(&infix_stack, &rpn_stack);
	return ecode;
} /* internal_calculate_infix_string() */

synge_settings get_synge_settings(void) {
	return active_settings;
} /* get_synge_settings() */

void set_synge_settings(synge_settings new_settings) {
	active_settings = new_settings;

	/* sanitise precision */
	if(new_settings.precision > SYNGE_MAX_PRECISION)
		active_settings.precision = SYNGE_MAX_PRECISION;
} /* set_synge_settings() */

function *get_synge_function_list(void) {
	return func_list;
} /* get_synge_function_list() */

void synge_start(void) {
	variable_list = ohm_init(2, NULL);
	function_list = ohm_init(2, NULL);
	traceback_list = link_init();

	synge_started = true;
} /* synge_end() */

void synge_end(void) {
	assert(synge_started);

	if(variable_list)
		ohm_free(variable_list);
	if(function_list)
		ohm_free(function_list);
	if(traceback_list)
		link_free(traceback_list);

	free(error_msg_container);
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

int is_success_code(int code) {
	if(code == SUCCESS ||
	   code == DELETED_VARIABLE ||
	   code == DELETED_FUNCTION) return true;

	else return false;
} /* is_success_code() */

int ignore_code(int code) {
	if(code == EMPTY_STACK ||
	   code == ERROR_FUNC_ASSIGNMENT) return true;

	else return false;
} /* ignore_code() */
