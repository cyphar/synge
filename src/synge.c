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

/* Operator Precedence
 * 4 Parenthesis
 * 3 Exponents, Roots (right associative)
 * 2 Multiplication, Division, Modulo (left associative)
 * 1 Addition, Subtraction (left associative)
 */

#define EPSILON			10e-11
#define SYNGE_MAX_PRECISION	10
#define SYNGE_MAX_DEPTH		10

#define SYNGE_PREV_ANSWER	"ans"
#define SYNGE_VARIABLE_CHARS	"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ_"
#define SYNGE_FUNCTION_CHARS	"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ0123456789_"

#define SYNGE_TRACEBACK_FORMAT	"Synge Traceback:\n" \
				"%s: %s"

#define PI 3.14159265358979323

#define strlower(x) do { char *p = x; for(; *p; ++p) *p = tolower(*p); } while(0)
#define strupper(x) do { char *p = x; for(; *p; ++p) *p = toupper(*p); } while(0)

/* my own assert() implementation */
#define assert(x) do { if(!x) { printf("synge: assertion '%s' failed -- aborting!\n", #x); exit(254); }} while(0)

/* -- useful macros -- */
#define isaddop(ch) (ch == '+' || ch == '-')
#define ismultop(ch) (ch == '*' || ch == '/' || ch == '\\' || ch == '%')
#define isexpop(ch) (ch == '^')
#define isparen(ch) (ch == '(' || ch == ')')

#define isop(type) (type == addop || type == multop || type == expop)

#define iszero(x) (fabs(x) <= EPSILON)

/* when a floating point number has a rounding error, weird stuff starts to happen -- reliable bug */
#define has_rounding_error(number) (number + 1 == number || number - 1 == number)

static bool synge_started = false; /* i REALLY reccomend you leave this false, as this is used to ensure that synge_start has been run */

double deg2rad(double deg) {
	return deg * (PI / 180.0);
} /* deg2rad() */

double rad2deg(double rad) {
	return rad * (180.0 / PI);
} /* rad2deg() */

double sy_rand(double to) {
	int min = 0;
	int max = (int) floor(to);
	/* better than the standard skewed (rand() % max + min + 1) range */
	return (rand() % (max + 1 - min)) + min;
} /* sy_rand() */

double sy_factorial(double x) {
	double number = floor(x);
	double factorial = number;
	while(number > 1.0)
		factorial *= --number;
	return factorial;
} /* sy_factorial() */

double sy_series(double x) {
	x = floor(x);
	/* an epic formula i learnt in year 5 */
	return (x * (x+1)) / 2;
} /* sy_series() */

double sy_assert(double x) {
	return x ? 1.0 : 0.0;
} /* sy_assert */

function func_list[] = {
	{"abs",		fabs,		"abs(x)",	"Absolute value of x"},
	{"sqrt",	sqrt,		"sqrt(x)",	"Square root of x"},
	{"cbrt",	cbrt,		"cbrt(x)",	"Cubic root of x"},

	{"floor",	floor,		"floor(x)",	"Largest integer not greater than x"},
	{"round",	round,		"round(x)",	"Closest integer to x"},
	{"ceil",	ceil,		"ceil(x)",	"Smallest integer not smaller than x"},

	{"log10",	log10,		"log10(x)",	"Base 10 logarithm of x"},
	{"log",		log2,		"log(x)",	"Base 2 logarithm of x"},
	{"ln",		log,		"ln(x)",	"Base e logarithm of x"},

	{"rand",	sy_rand,	"rand(x)",	"Generate a psedu-random integer between 0 and floor(x)"},
	{"fact",	sy_factorial,	"fact(x)",	"Factorial of floor(x)"},
	{"series",	sy_series,	"series(x)",	"Gives addition of all integers up to floor(x)"},
	{"assert",	sy_assert,	"assert(x)",	"Returns 0 is x is 0, and returns 1 otherwise"},

	{"deg2rad",   	deg2rad,	"deg2rad(x)",	"Convert x degrees to radians"},
	{"rad2deg",   	rad2deg,	"rad2deg(x)",	"Convert x radians to degrees"},

	{"sinhi",	asinh,		"asinh(x)",	"Inverse hyperbolic sine of x"},
	{"coshi",	acosh,		"acosh(x)",	"Inverse hyperbolic cosine of x"},
	{"tanhi",	atanh,		"atanh(x)",	"Inverse hyperbolic tangent of x"},
	{"sinh",	sinh,		"sinh(x)",	"Hyperbolic sine of x"},
	{"cosh",	cosh,		"cosh(x)",	"Hyperbolic cosine of x"},
	{"tanh",	tanh,		"tanh(x)",	"Hyperbolic tangent of x"},

	{"sini",	asin,		"asin(x)",	"Inverse sine of x"},
	{"cosi",	acos,		"acos(x)",	"Inverse cosine of x"},
	{"tani",	atan,		"atan(x)",	"Inverse tangent of x"},
	{"sin",		sin,		"sin(x)",	"Sine of x"},
	{"cos",		cos,		"cos(x)",	"Cosine of x"},
	{"tan",		tan,		"tan(x)",	"Tangent of x"},
	{NULL,		NULL,		NULL,		NULL}
};

typedef struct __function_alias__ {
	char *alias;
	char *function;
} function_alias;

function_alias alias_list[] = {
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
	double value;
} special_number;

ohm_t *variable_list = NULL;
ohm_t *function_list = NULL;

special_number constant_list[] = {
	{"pi",			3.14159265358979323},
	{"e",			2.71828182845904523},
	{"life",			       42.0}, /* Sorry, I couldn't resist */

	{"true",				1.0},
	{"false",				0.0},

	{SYNGE_PREV_ANSWER,			0.0},
	{NULL,					0.0},
};

/* used for when a (char *) is needed, but needn't be freed */
char *op_list[] = {
	"+",
	"-",
	"*",
	"/",
	"\\", /* integer division */
	"%",
	"^",
	"(",
	")",
	NULL
};

synge_settings active_settings = {
	degrees,
	position,
	strict,
	-1
};

static char *error_msg_container = NULL; /* needs to be freed at program termination using synge_free() (if you want valgrind to be happy) */
#define lenprintf(...) (snprintf(NULL, 0, __VA_ARGS__) + 1) /* hack to get amount of memory needed to store a sprintf() */

/* __DEBUG__ FUNCTIONS */

void print_stack(stack *s) {
#ifdef __DEBUG__
	int i, size = stack_size(s);
	for(i = 0; i < size; i++) {
		s_content tmp = s->content[i];
		if(!tmp.val) continue;

		if(tmp.tp == number)
			printf("%f ", *(double *) tmp.val);
		else if(tmp.tp == func)
			printf("%s ", ((function *)tmp.val)->name);
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
			count++; /* found occurence of old string */
			i += oldlen - 1; /* jump forward in string */
		}
	}

	ret = malloc(i + count * (newlen - oldlen) + 1);
	if (ret == NULL) return NULL;

	i = 0;
	while(*s != '\0') {
		if(strstr(s, old) == s) { /* is *s position at occurence of old? */
			strcpy(&ret[i], new); /* copy over new to replace this occurence */
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
	memcpy(ret, s, i + 1);
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

char *get_from_ch_list(char *ch, char **list, bool delimit) {
	int i;
	for(i = 0; list[i] != NULL; i++)
		/* checks against part or entire string against the given list */
		if((delimit && !strncmp(list[i], ch, strlen(list[i]))) ||
		   (!delimit && !strcmp(list[i], ch))) return list[i];
	return NULL;
} /* get_from_ch_list() */

double *double_dup(double num) {
	double *ret = malloc(sizeof(double));
	*ret = num;
	return ret;
} /* double_dup() */

char *str_dup(char *s) {
	char *ret = malloc(strlen(s) + 1);

	memcpy(ret, s, strlen(s) + 1);
	ret[strlen(s)] = '\0'; /* ensure null termination */

	return ret;
} /* str_dup() */

int get_precision(double num) {
	/* use the current settings' precision if given */
	if(active_settings.precision >= 0)
		return active_settings.precision;

	/* printf knows how to fix rounding errors -- WARNING: here be dragons! */
	int tmpsize = lenprintf("%.*f", SYNGE_MAX_PRECISION, num); /* get the amount of memory needed to store this printf*/
	char *tmp = malloc(tmpsize);

	sprintf(tmp, "%.*f", SYNGE_MAX_PRECISION, num); /* sprintf it */
	tmp[tmpsize - 1] = '\0'; /* force null termination */

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
	ret = (get_special_num(s).name || ohm_search(variable_list, s, strlen(s) + 1) || ohm_search(function_list, s, strlen(s) + 1) || string != endptr);
	free(s);
	return ret;
} /* isnum() */

void set_special_number(char *s, double val, special_number *list) {
	/* specifically made for the ans "variable" -- use the hashmap for real variables */
	int i;
	for(i = 0; list[i].name != NULL; i++)
		if(!strcmp(list[i].name, s)) list[i].value = val;
} /* set_special_number() */

error_code set_variable(char *str, double val) {
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

char *function_process_replace(char *string) {
	char *firstpass = NULL;

	int i;
	for(i = 0; alias_list[i].alias != NULL; i++) {
		/* replace all aliases*/
		char *tmppass = replace(firstpass ? firstpass : string, alias_list[i].alias, alias_list[i].function);
		free(firstpass);
		firstpass = tmppass;
	}

	/* a hack for function division i thought of in maths ...
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
}

int recalc_padding(char *str, int len) {
	int ret = 0, tmp;

	/* the number of open and close brackets is double the amount in the original string (except for mismatched parens) */
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

error_code tokenise_string(char *string, int offset, stack **ret) {
	assert(synge_started);
	char *s = function_process_replace(string);
	error_code tmpecode = to_error_code(SUCCESS, -1);

	debug("%s\n%s\n", string, s);

	init_stack(*ret);
	int i, pos, tmp = 0, len = strlen(s);
	for(i = 0; i < len; i++) {
		pos = i + offset - recalc_padding(s, (i ? i : 1) - 1) + 1;
		if(s[i] == ' ') continue; /* ignore spaces */

		/* full number check */
		else if(isnum(s+i) && /* does it fit the description of a number? */
			/* if it is the first token in the string, it's a number */
		       (!top_stack(*ret) ||
			/* ensure a + or - is not an operator (the + in 1+2 is an operator - the + in 1++2 is part of the number) */
		       ((top_stack(*ret)->tp != number || !isaddop(*(s+i))) &&
			/* same as above, but for parenthesis */
		        (top_stack(*ret)->tp != rparen || !get_from_ch_list(s+i, op_list, true))))) {

			double *num = malloc(sizeof(double)); /* allocate memory to be pushed onto the stack */
			char *endptr = NULL, *word = get_word(s+i, SYNGE_VARIABLE_CHARS, &endptr);

			/* if it is a "special" number */
			if(get_special_num(word).name) {
				special_number stnum = get_special_num(word);
				*num = stnum.value;
				i += strlen(stnum.name) - 1; /* update iterator to correct offset */
			}

			/* is it a variable or user function? */
			else if((ohm_search(variable_list, word, strlen(word) + 1) && (tmp = 1)) ||
				ohm_search(function_list, word, strlen(word) + 1)) {

				if(tmp)
					/* variable */
					*num = *(double *) ohm_search(variable_list, word, strlen(word) + 1);
				else {
					/* function */

					/* recursively evaluate a user function's value */
					tmpecode = compute_infix_string((char *) ohm_search(function_list, word, strlen(word) + 1), num);
					if(!is_success_code(tmpecode.code)) {
						/* error was encountered */
						free(s);
						free(num);
						free(word);
						return to_error_code(tmpecode.code, pos);
					}
				}

				if(top_stack(*ret)) {
					s_content *tmppop, *tmpp;
					/* make variables act more like numbers (and more like variables) */
					switch(top_stack(*ret)->tp) {
						case addop:
							/* if there is a +/- in front of a variable, it should set the sign of that variable (i.e. 1--x is 1+x) */
							tmppop = pop_stack(*ret); /* the sign (to be saved for later) */
							tmpp = top_stack(*ret);
							if(!tmpp || (tmpp->tp != number && tmpp->tp != rparen)) { /* sign is to be discarded */
								if(((char *) tmppop->val)[0] == '-') /* negate the variable? */
									*num = -(*num);
							}
							else
								/* whoops! didn't match criteria. push sign back. */
								push_ststack(*tmppop, *ret);
							break;
						case number:
							/* two numbers together have an impiled * (i.e 2x is 2*x) */
							push_valstack("*", multop, pos, *ret);
							break;
						default:
							break;
					}
				}

				i += strlen(word) - 1; /* update iterator to correct offset */
			}

			else {
				char *endptr;
				*num = strtold(s+i, &endptr);
				i += (endptr - (s + i)) - 1; /* update iterator to correct offset */
			}
			push_valstack(num, number, pos, *ret); /* push given value */
			free(word);

			/* error detection (done per number to ensure numbers are 163% correct) */
			if(has_rounding_error(*num)) {
				free(s);
				return to_error_code(NUM_OVERFLOW, pos);
			}
			else if(*num != *num) {
				free(s);
				return to_error_code(UNDEFINED, pos);
			}
		}
		else if(get_from_ch_list(s+i, op_list, true)) {
			int postpush = false;
			s_type type;
			/* find and set type appropriate to operator */
			switch(s[i]) {
				case '+':
				case '-':
					type = addop;
					break;
				case '*':
				case '/':
				case '\\': /* integer division -- like in python */
				case '%':
					type = multop;
					break;
				case '^':
					type = expop;
					break;
				case '(':
					type = lparen;
					pos -= 1; /* 'hack' to ensure the error position is correct */
					/* every open paren with no operators before it has an implied * */
					if(top_stack(*ret) && (top_stack(*ret)->tp == number || top_stack(*ret)->tp == rparen))
						push_valstack("*", multop, pos + 1, *ret);
					break;
				case ')':
					type = rparen;
					pos -= 1; /* 'hack' to ensure the error position is correct */
					if(isnum(s+next_offset(s, i+1)) && !get_from_ch_list(s+next_offset(s, i+1), op_list, true))
						postpush = true;
					break;
				default:
					free(s);
					return to_error_code(UNKNOWN_TOKEN, pos);
			}
			push_valstack(get_from_ch_list(s+i, op_list, true), type, pos, *ret); /* push operator onto stack */

			if(postpush)
				push_valstack("*", multop, pos, *ret);

		}
		else if(get_func(s+i)) {
			char *endptr = NULL, *word = get_word(s+i, SYNGE_FUNCTION_CHARS, &endptr); /* find the function word */

			function *funcname = get_func(word); /* get the function pointer, name, etc. */
			push_valstack(funcname, func, pos - 1, *ret);
			i += strlen(funcname->name) - 1; /* update iterator to correct offset */

			free(word);
		}
		else {
			/* catchall -- unknown token */
			free(s);
			return to_error_code(UNKNOWN_TOKEN, pos);
		}
		/* debugging */
		print_stack(*ret);
	}

	free(s);
	if(!stack_size(*ret))
		return to_error_code(EMPTY_STACK, -1); /* stack was empty */

	/* debugging */
	print_stack(*ret);

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
		case addop:
		case multop:
			lassoc = 1;
			break;
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
error_code infix_stack_to_rpnstack(stack **infix_stack, stack **rpn_stack) {
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
				push_valstack(double_dup(*(double *) stackp.val), number, pos, *rpn_stack);
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
				if(!found)
					/* if no lparen was found, this is an unmatched right bracket*/
					return safe_free_stack(UNMATCHED_RIGHT_PARENTHESIS, pos + 1, infix_stack, &op_stack, rpn_stack);
				break;
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
				return safe_free_stack(UNKNOWN_TOKEN, pos, infix_stack, &op_stack, rpn_stack);
		}
	}

	/* re-reverse the stack again (so it's in the correct order) */
	while(stack_size(op_stack)) {
		stackp = *pop_stack(op_stack);
		pos = stackp.position;
		if(stackp.tp == lparen ||
		   stackp.tp == rparen) {
			/* if there is a left or right bracket, there is an unmatched left bracket */
			if(active_settings.strict >= strict)
				return safe_free_stack(UNMATCHED_LEFT_PARENTHESIS, pos, infix_stack, &op_stack, rpn_stack);
			else continue;
		}
		push_ststack(stackp, *rpn_stack);
	}

	/* debugging */
	print_stack(*rpn_stack);
	return safe_free_stack(SUCCESS, -1, infix_stack, &op_stack);
} /* infix_to_rpnstack() */

/* functions' whose input needs to be in radians */
char *angle_infunc_list[] = {
	"sin",
	"cos",
	"tan",
	NULL
};

/* convert from set mode to radians */
double settings_to_rad(double in) {
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
double rad_to_settings(double in) {
	switch(active_settings.mode) {
		case degrees:
			return rad2deg(in);
		case radians:
			return in;
	}

	/* catch-all -- wtf? */
	return 0.0;
} /* rad_to_settings() */

/* evaluate a rpn stack */
error_code eval_rpnstack(stack **rpn, double *ret) {
	stack *tmpstack = malloc(sizeof(stack));
	init_stack(tmpstack);

	s_content stackp;
	int i, pos = 0, tmp = 0, size = stack_size(*rpn);
	double *result = NULL, arg[2];
	for(i = 0; i < size; i++) {
		/* debugging */
		print_stack(tmpstack);

		stackp = (*rpn)->content[i]; /* shorthand for current stack item */
		pos = stackp.position; /* shorthand for current error position */
		switch(stackp.tp) {
			case number:
				/* just push it onto the final stack */
				push_valstack(double_dup(*(double *) stackp.val), number, pos, tmpstack);
				break;
			case func:
				/* check if there is enough numbers for function arguments */
				if(stack_size(tmpstack) < 1)
					return safe_free_stack(FUNCTION_WRONG_ARGC, pos < 1 ? pos + 1 : pos, &tmpstack, rpn);

				/* get the first (and, for now, only) argument */
				arg[0] = *(double *) top_stack(tmpstack)->val;
				free_scontent(pop_stack(tmpstack));

				/* does the input need to be converted? */
				if(get_from_ch_list(((function *)stackp.val)->name, angle_infunc_list, false)) /* convert settings angles to radians */
					arg[0] = settings_to_rad(arg[0]);

				/* allocate result and evaluate it */
				result = malloc(sizeof(double));
				*result = ((function *)stackp.val)->get(arg[0]);

				/* does the output need to be converted? */
				if(get_from_ch_list(((function *)stackp.val)->name, angle_outfunc_list, false)) /* convert radians to settings angles */
					*result = rad_to_settings(*result);

				/* push result of evaluation onto the stack */
				push_valstack(result, number, pos, tmpstack);
				break;
			case addop:
			case multop:
			case expop:
				/* check if there is enough numbers for operator "arguments" */
				if(stack_size(tmpstack) < 2)
					return safe_free_stack(OPERATOR_WRONG_ARGC, pos, &tmpstack, rpn);

				/* get second argument */
				arg[1] = *(double *) top_stack(tmpstack)->val;
				free_scontent(pop_stack(tmpstack));

				/* get first argument */
				arg[0] = *(double *) top_stack(tmpstack)->val;
				free_scontent(pop_stack(tmpstack));

				result = malloc(sizeof(double));
				/* find correct evaluation and do it */
				switch(*(char *) stackp.val) {
					case '+':
						*result = arg[0] + arg[1];
						break;
					case '-':
						*result = arg[0] - arg[1];
						break;
					case '*':
						*result = arg[0] * arg[1];
						break;
					case '\\':
						/* division, but the result ignores the decimals */
						tmp = 1;
					case '/':
						if(iszero(arg[1])) {
							/* the 11th commandment -- thoust shalt not divide by zero */
							free(result);
							return safe_free_stack(DIVIDE_BY_ZERO, pos, &tmpstack, rpn);
						}
						*result = arg[0] / arg[1];
						if(tmp) modf(*result, result);
						break;
					case '%':
						if(iszero(arg[1])) {
							/* the 11.5th commandment -- thoust shalt not modulo by zero */
							free(result);
							return safe_free_stack(MODULO_BY_ZERO, pos, &tmpstack, rpn);
						}
						*result = fmod(arg[0], arg[1]);
						break;
					case '^':
						*result = pow(arg[0], arg[1]);
						break;
					default:
						/* catch-all -- unknown token */
						return safe_free_stack(UNKNOWN_TOKEN, pos, &tmpstack, rpn);
				}

				/* push result onto stack */
				push_valstack(result, number, pos, tmpstack);
				break;
			default:
				/* catch-all -- unknown token */
				return safe_free_stack(UNKNOWN_TOKEN, pos, &tmpstack, rpn);
		}
	}

	/* if there is not one item on the stack, there are too many values on the stack */
	if(stack_size(tmpstack) != 1)
		return safe_free_stack(TOO_MANY_VALUES, -1, &tmpstack, rpn);

	/* otherwise, the last item is the result */
	*ret = *(double *) top_stack(tmpstack)->val;

	/* check for rounding errors */
	if(has_rounding_error(*ret))
		return safe_free_stack(NUM_OVERFLOW, -1, &tmpstack, rpn);

	return safe_free_stack(SUCCESS, -1, &tmpstack, rpn);
} /* eval_rpnstack() */

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
		case EMPTY_STACK:
		case TOO_MANY_VALUES:
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
				trace = malloc(lenprintf(SYNGE_TRACEBACK_FORMAT, get_error_tp(error), msg));
				sprintf(trace, SYNGE_TRACEBACK_FORMAT, get_error_tp(error), msg);
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

error_code compute_infix_string(char *original_str, double *result) {
	static int depth = 0;
	assert(synge_started);

	if(variable_list->count > variable_list->size)
		/* "dynamically" resize hashmap to keep efficiency up */
		variable_list = ohm_resize(variable_list, variable_list->size * 2);

	/* initialise result with a value of 0.0 */
	*result = 0.0;

	/*
	 * We have delved too greedily and too deep.
	 * We have awoken a creature in the darkness of recursion.
	 * A creature of shadow, flame and segmentation faults.
	 * YOU SHALL NOT PASS!
	 */

	if(depth++ > SYNGE_MAX_DEPTH)
		return to_error_code(TOO_DEEP, -1);

	/* initialise all local variables */
	stack *rpn_stack = malloc(sizeof(stack)), *infix_stack = malloc(sizeof(stack));
	init_stack(rpn_stack);
	init_stack(infix_stack);
	error_code ecode = to_error_code(SUCCESS, -1);

	/* process string */
	char *final_pass_str = str_dup(original_str);
	char *string = NULL, *var = NULL;

	/* find any variable assignments (=) in string */
	if(strchr(final_pass_str, '=')) {
		var = final_pass_str;
		string = strrchr(final_pass_str, '=');
		*string++ = '\0';

		char *endptr = NULL, *word = get_word(var, SYNGE_VARIABLE_CHARS " :=", &endptr); /* get variable name */

		if(strlen(word) != strlen(var))
			/* invalid characters */
			ecode = to_error_code(INVALID_VARIABLE_CHAR, endptr - var + 1);

		free(word);
	}
	else string = final_pass_str;

	if(ecode.code == SUCCESS)
		/* generate infix stack */
		if((ecode = tokenise_string(string, var ? strlen(var) + 1 : 0, &infix_stack)).code == SUCCESS)
			/* convert to postfix (or RPN) stack */
			if((ecode = infix_stack_to_rpnstack(&infix_stack, &rpn_stack)).code == SUCCESS)
				/* evaluate postfix (or RPN) stack */
				if((ecode = eval_rpnstack(&rpn_stack, result)).code == SUCCESS)
					/* fix up negative zeros */
					if(*result == abs(*result))
						*result = abs(*result);

	/* measure depth, not length */
	depth--;

	/* is it a nan? */
	if(*result != *result)
		ecode = to_error_code(UNDEFINED, -1);

	enum {
		VAR	= 1,
		FUNC	= 2,

		SET	= 1,
		DEL	= -1
	};

	int stackop = 0, operation = 0;

	/* check and set operation based on "success" code */
	if(var && ((ecode.code != EMPTY_STACK && (stackop = SET)) ||
		  (ecode.code == EMPTY_STACK && (stackop = DEL)))) {
		char *tmp = --var;

		/* dry run -- don't change any variables if there is an error later in the assignment */
		do {
			tmp++;

			char *spacetmp = trim_spaces(tmp);
			char *tmpp = NULL, *tmpword = get_word(spacetmp, SYNGE_VARIABLE_CHARS, &tmpp);
			error_code tmpecode = to_error_code(SUCCESS, -1);

			if(*tmpp == ':') {
				tmp++;
				operation = stackop * FUNC;
			} else operation = stackop * VAR;

			debug("operation: %s %s %s\n", tmpword, operation > 0 ? "set" : "del", abs(operation) == VAR ? "var" : "function");

			/* check if it is correct to edit a variable */
			if(strlen(tmp) < 1 || strlen(tmpword) < 1)
				tmpecode = to_error_code(EMPTY_VARIABLE_NAME, -1);
			else if(get_special_num(tmpword).name)
				tmpecode = to_error_code(RESERVED_VARIABLE, -1);

			if(tmpecode.code != SUCCESS) {
				ecode = tmpecode;
				operation = 0;
				free(tmpword);
				break;
			}
			free(tmpword);
			free(spacetmp);
		} while((tmp = strchr(tmp, '=')) != NULL);

		/* everything was good in the dry run -- let's do it for real */
		if(operation) {
			do {
				var++;

				char *spacevar = trim_spaces(var);
				char *tmpp = NULL, *tmpword = get_word(spacevar, SYNGE_VARIABLE_CHARS, &tmpp);
				operation = stackop * ((*tmpp == ':') ? FUNC : VAR);

				debug("var %s -> %s -> %s\n", var, spacevar, tmpword);

				switch(operation) {
					case 1:
						if(ecode.code != SUCCESS) break; /* we care if variables are assigned to errors */
						/* set/update variable */
						ecode = set_variable(tmpword, *result);
						break;
					case 2:
						/* set/update function/function */
						if(ecode.code != SUCCESS)
							ecode.code = ERROR_FUNC_ASSIGNMENT;
						set_function(tmpword, string); /* but functions should work despite any errors */
						break;
					/* my name's benny and i like to party when i go to class late they call me tardy */
					case -1:
					case -2:
						/* delete variable */
						ecode = del_word(tmpword, (operation % FUNC) ? true : false);
						break;
					default:
						ecode = to_error_code(UNKNOWN_ERROR, -1);
						break;
				}

				free(tmpword);
				free(spacevar);
			} while((var = strchr(var, '=')) != NULL);
		}
	}

	/* FINALLY, set the answer variable */
	if(is_success_code(ecode.code))
		set_special_number(SYNGE_PREV_ANSWER, *result, constant_list);

	free(final_pass_str);
	return safe_free_stack(ecode.code, ecode.position, &infix_stack, &rpn_stack);
} /* calculate_infix_string() */

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

	synge_started = true;
} /* synge_end() */

void synge_end(void) {
	assert(synge_started);

	if(variable_list)
		ohm_free(variable_list);
	if(function_list)
		ohm_free(function_list);

	free(error_msg_container);
	synge_started = false;
} /* synge_end() */

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
