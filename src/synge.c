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
#include <assert.h>

#include "stack.h"
#include "synge.h"
#include "ohmic.h"

/* Operator Precedence
 * 4 Parenthesis
 * 3 Exponents, Roots (right associative)
 * 2 Multiplication, Division, Modulo (left associative)
 * 1 Addition, Subtraction (left associative)
 */

#define SYNGE_MAX_PRECISION	10
#define SYNGE_PREV_ANSWER	"ans"
#define SYNGE_VARIABLE_CHARS	"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ_"
#define SYNGE_FUNCTION_CHARS	"abcdefghijklmnopqrstuvwxyzABCDEFHIJKLMNOPQRSTUVWXYZ0123456789_"

#define PI 3.14159265358979323

#define strlower(x) do { char *p = x; for(; *p; ++p) *p = tolower(*p); } while(0)
#define strupper(x) do { char *p = x; for(; *p; ++p) *p = toupper(*p); } while(0)

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
	return (rand() % (max + 1 - min)) + min;
} /* sy_rand() */

double sy_factorial(double x) {
	double number = floor(x);
	double factorial = number;
	while(number > 1.0)
		factorial *= --number;
	return factorial;
} /* sy_factorial() */

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

	{"deg2rad",   	deg2rad,	"deg2rad(x)",	"Convert x degrees to radians"},
	{"rad2deg",   	rad2deg,	"rad2deg(x)",	"Convert x radians to degrees"},

	{"sinhi",	asinh,		"asinh(x)",	"Inverse hyperbolic sine of x"},
	{"coshi",	acosh,		"acosh(x)",	"Inverse hyperbolic cosine of x"},
	{"tanhi",	atanh,		"atanh(x)",	"Inverse hyperbolic tangent of x"},
	{"sinh",	sinh,		"sinh(x)",	"Hyperbolic sine of x"},
	{"cosh",	cosh,		"cosh(x)",	"Hyperbolic cosine of x"},
	{"tanh",	tanh,		"tanh(x)",	"Hyperbolic tangent of x"},

	{"sini",	asin,		"sini(x)",	"Inverse sine of x"},
	{"cosi",	acos,		"cosi(x)",	"Inverse cosine of x"},
	{"tani",	atan,		"tani(x)",	"Inverse tangent of x"},
	{"sin",		sin,		"sin(x)",	"Sine of x"},
	{"cos",		cos,		"cos(x)",	"Cosine of x"},
	{"tan",		tan,		"tan(x)",	"Tangent of x"},
	{NULL,		NULL,		NULL}
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

special_number number_list[] = {
	{"pi",			3.14159265358979323},
	{"e",			2.71828182845904523},
	{"life",			       42.0}, /* Sorry, I couldn't resist */
	{SYNGE_PREV_ANSWER,			0.0},
	{NULL,					0.0},
};

char *op_list[] = {
	"+",
	"-",
	"*",
	"/",
	"%",
	"^",
	"(",
	")",
	NULL
};

synge_settings active_settings = {
	degrees,
	position
};

static char *error_msg_container = NULL; /* needs to be freed at program termination using synge_free() (if you want valgrind to be happy) */

#define lenprintf(...) (snprintf(NULL, 0, __VA_ARGS__) + 1) /* hack to get amount of memory needed to store a sprintf() */

void print_stack(stack *s) {
#ifdef __DEBUG__
	int i;
	for(i = 0; i < stack_size(s); i++) {
		s_content tmp = s->content[i];
		if(!tmp.val) continue;

		if(tmp.tp == number) printf("%f ", *(double *) tmp.val);
		else if(tmp.tp == func) printf("%s ", ((function *)tmp.val)->name);
		else printf("%s ", (char *) tmp.val);
	}
	printf("\n");
#endif
} /* print_stack() */

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
	int i, j, found;
	for(i = 0; i < strlen(s); i++) {
		found = false;
		for(j = 0; j < strlen(list); j++)
			if(s[i] == list[j]) {
				found = true;
				break;
			}
		if(!found) break;
	}

	char *ret = malloc(i + 1);
	strncpy(ret, s, i);
	ret[i] = '\0';

	strlower(ret);

	*endptr = s+i;
	return ret;
} /* get_word() */

char *get_from_ch_list(char *ch, char **list, bool delimit) {
	int i;
	for(i = 0; list[i] != NULL; i++) {
		if((delimit && !strncmp(list[i], ch, strlen(list[i]))) ||
		   (!delimit && !strcmp(list[i], ch))) return list[i];
	}
	return NULL;
} /* get_from_ch_list() */

double *double_dup(double num) {
	double *ret = malloc(sizeof(double));
	*ret = num;
	return ret;
} /* double_dup() */

int get_precision(double num) {
	int tmpsize = lenprintf("%.*f", SYNGE_MAX_PRECISION, num);
	char *tmp = malloc(tmpsize);
	sprintf(tmp, "%.*f", SYNGE_MAX_PRECISION, num);
	tmp[tmpsize - 1] = '\0';

	char *p = tmp + tmpsize - 2;
	int precision = SYNGE_MAX_PRECISION;
	while(*p == '0' && *p != '.') {
		precision--;
		p--;
	}

	free(tmp);
	return precision;
} /* get_precision() */

error_code to_error_code(int error, int position) {
	error_code ret = {
		error,
		position
	};
	return ret;
} /* to_error_code() */

bool isop(s_type type) {
	return (type == addop || type == multop || type == expop);
} /* isop() */

bool isspecialch(s_type type) {
	return (type == lparen || type == rparen);
} /* isspecialch() */

bool isspecialnum(char *s) {
	int i;
	for(i = 0; number_list[i].name != NULL; i++)
		if(!strcmp(number_list[i].name, s)) return true;
	return false;
} /* isspecialnum() */

special_number getspecialnum(char *s) {
	int i;
	special_number ret = {NULL, 0.0};
	for(i = 0; number_list[i].name != NULL; i++)
		if(!strcmp(number_list[i].name, s)) return number_list[i];
	return ret;
} /* getspecialnum() */

function *get_func(char *val) {
	int i;
	for(i = 0; func_list[i].name != NULL; i++)
		if(!strcmp(val, func_list[i].name)) return &func_list[i];
	return NULL;
} /* get_func() */

bool is_func(char *val) {
	char *endptr = NULL, *word = get_word(val, SYNGE_FUNCTION_CHARS, &endptr);
	bool ret = (get_func(word) ? true : false);

	free(word);
	return ret;
} /* is_func() */

bool isnum(char *string) {
	int ret = false;
	char *endptr = NULL;
	char *s = get_word(string, SYNGE_VARIABLE_CHARS, &endptr);

	endptr = NULL;
	strtold(string, &endptr);

	if(isspecialnum(s) || ohm_search(variable_list, s, strlen(s) + 1) || string != endptr) ret = true;
	else ret = false;

	free(s);
	return ret;
} /* isnum() */

void set_special_number(char *s, double val, special_number *list) {
	int i;
	for(i = 0; list[i].name != NULL; i++)
		if(!strcmp(list[i].name, s)) list[i].value = val;
} /* set_special_number() */

error_code set_variable(char *str, double val) {
	assert(synge_started);

	error_code ret = to_error_code(SUCCESS, -1);
	char *endptr = NULL, *s = get_word(str, SYNGE_FUNCTION_CHARS, &endptr);

	if(isspecialnum(s))
		ret = to_error_code(RESERVED_VARIABLE, -1);
	else
		ohm_insert(variable_list, s, strlen(s) + 1, &val, sizeof(val));

	free(s);
	return ret;
} /* set_variable() */

error_code del_variable(char *str) {
	assert(synge_started);

	error_code ret = to_error_code(SUCCESS, -1);
	char *endptr = NULL, *s = get_word(str, SYNGE_FUNCTION_CHARS, &endptr);

	if(!ohm_search(variable_list, s, strlen(s) + 1))
		ret = to_error_code(UNKNOWN_VARIABLE, -1);
	else
		ohm_remove(variable_list, s, strlen(s) + 1);

	free(s);
	return ret;
} /* del_variable() */

char *function_process_replace(char *string) {
	char *firstpass = NULL;

	int i;
	for(i = 0; alias_list[i].alias != NULL; i++) {
		char *tmppass = replace(firstpass ? firstpass : string, alias_list[i].alias, alias_list[i].function);
		free(firstpass);
		firstpass = tmppass;
	}

	char *secondpass = replace(firstpass, "(", "((");
	char *final = replace(secondpass, ")", "))");

	for(i = 0; func_list[i].name != NULL; i++) {
		char *tmpfrom = malloc(strlen(func_list[i].name) + 3);
		strcpy(tmpfrom, func_list[i].name);
		strcat(tmpfrom, "((");

		char *tmpto = malloc(strlen(func_list[i].name) + 3);
		strcpy(tmpto, "(");
		strcat(tmpto, func_list[i].name);
		strcat(tmpto, "(");

		char *tmpfinal = replace(final, tmpfrom, tmpto);

#ifdef __DEBUG__
		if(strcmp(final, tmpfinal)) {
			printf("(%s)\t%s => %s\n", func_list[i].name, final, tmpfinal);
		}
#endif
		free(final);
		final = tmpfinal;

		free(tmpfrom);
		free(tmpto);
	}

	free(firstpass);
	free(secondpass);
	return final;
}

bool has_rounding_error(double number) {
	if(number + 1 == number || number - 1 == number)
		return true;
	return false;
} /* has_rounding_error() */

error_code tokenise_string(char *string, stack **ret) {
	assert(synge_started);
	char *tmps = replace(string, " ", "");
	char *s = function_process_replace(tmps);

#ifdef __DEBUG__
	printf("%s\n%s\n", string, s);
#endif
	free(tmps);
	init_stack(*ret);
	int i, pos;
	for(i = 0; i < strlen(s); i++) {
		pos = i + 1;
		if(isnum(s+i) && (!i || (i > 0 && top_stack(*ret)->tp != number && top_stack(*ret)->tp != rparen))) {
			double *num = malloc(sizeof(double));
			char *endptr = NULL, *word = get_word(s+i, SYNGE_VARIABLE_CHARS, &endptr);
			if(isspecialnum(word)) {
				special_number stnum = getspecialnum(word);
				*num = stnum.value;
				i += strlen(stnum.name) - 1;
			}
			else if(ohm_search(variable_list, word, strlen(word) + 1)) {
				*num = *(double *) ohm_search(variable_list, word, strlen(word) + 1);
				i += strlen(word) - 1;
			}
			else {
				char *endptr;
				*num = strtold(s+i, &endptr);
				i += (endptr - (s + i)) - 1;
			}
			push_valstack(num, number, pos, *ret);
			free(word);
			if(has_rounding_error(*num)) return to_error_code(NUM_OVERFLOW, pos);
		}
		else if(get_from_ch_list(s+i, op_list, true)) {
			s_type type;
			switch(s[i]) {
				case '+':
				case '-':
					type = addop;
					break;
				case '*':
				case '/':
				case '%':
					type = multop;
					break;
				case '^':
					type = expop;
					break;
				case '(':
					type = lparen;
					break;
				case ')':
					type = rparen;
					break;
				default:
					free(s);
					return to_error_code(UNKNOWN_TOKEN, pos);
			}
			push_valstack(get_from_ch_list(s+i, op_list, true), type, pos, *ret);
		}
		else if(is_func(s+i)) {
			char *endptr = NULL, *word = get_word(s+i, SYNGE_FUNCTION_CHARS, &endptr);

			function *funcname = get_func(word);
			push_valstack(funcname, func, pos, *ret);
			i += strlen(funcname->name) - 1;

			free(word);
		}
		else { /* unknown token */
			free(s);
			return to_error_code(UNKNOWN_TOKEN, pos);
		}
	}
	free(s);
	if(!stack_size(*ret)) return to_error_code(EMPTY_STACK, -1);
	/* debugging */
	print_stack(*ret);
	return to_error_code(SUCCESS, -1);
} /* tokenise_string() */

bool op_precedes(s_type op1, s_type op2) {
	/* IMPORTANT:	returns true if:
	 * 			op1 > op2 (or op1 > op2 - 0)
	 * 			op2 is left associative and op1 >= op2 (or op1 > op2 - 1)
	 * 		returns false otherwise
	 */
	int lassoc;
	switch(op2) {
		case addop:
		case multop:
			lassoc = 1;
			break;
		case expop:
			lassoc = 0;
			break;
		default:
			return false; /* what the hell you doing? */
			break;
	}
	if(op1 > op2 - lassoc) return true;
	else return false;
} /* op_precedes() */


error_code infix_stack_to_rpnstack(stack **infix_stack, stack **rpn_stack) {
	s_content stackp, *tmpstackp;
	init_stack(*rpn_stack);

	stack *op_stack = malloc(sizeof(stack));
	init_stack(op_stack);
	int i, found, pos;
	for(i = 0; i < stack_size(*infix_stack); i++) {
		stackp = (*infix_stack)->content[i];
		pos = stackp.position;

		switch(stackp.tp) {
			case number:
				push_valstack(double_dup(*(double *) stackp.val), number, pos, *rpn_stack);
				break;
			case lparen:
			case func:
				push_ststack(stackp, op_stack);
				break;
			case rparen:
				found = false;
				while(stack_size(op_stack)) {
					tmpstackp = pop_stack(op_stack);
					if(tmpstackp->tp == lparen) {
						found = true;
						break;
					}
					push_ststack(*tmpstackp, *rpn_stack);
				}
				if(!found) return safe_free_stack(UNMATCHED_RIGHT_PARENTHESIS, pos, infix_stack, &op_stack, rpn_stack);
				break;
			case addop:
			case multop:
			case expop:
				if(stack_size(op_stack)) {
					while(stack_size(op_stack)) {
						tmpstackp = top_stack(op_stack);
						if(!isop(tmpstackp->tp)) break;
						if(op_precedes(tmpstackp->tp, stackp.tp)) push_ststack(*pop_stack(op_stack), *rpn_stack);
						else break;
					}
				}
				push_ststack(stackp, op_stack);
				break;
			default:
				return safe_free_stack(UNKNOWN_TOKEN, pos, infix_stack, &op_stack, rpn_stack);
		}
	}

	while(stack_size(op_stack)) {
		stackp = *pop_stack(op_stack);
		pos = stackp.position;
		if(isspecialch(stackp.tp))
			return safe_free_stack(UNMATCHED_LEFT_PARENTHESIS, pos, infix_stack, &op_stack, rpn_stack);
		push_ststack(stackp, *rpn_stack);
	}

	/* debugging */
	print_stack(*rpn_stack);
	return safe_free_stack(SUCCESS, -1, infix_stack, &op_stack);
} /* infix_to_rpnstack() */


char *angle_infunc_list[] = {
	"sin",
	"cos",
	"tan",
	NULL
};

char *angle_outfunc_list[] = {
	"sini",
	"cosi",
	"tani",
	NULL
};

double settings_to_rad(double in) {
	switch(active_settings.mode) {
		case degrees:
			return deg2rad(in);
		case radians:
			return in;
	}
	return 0.0;
} /* settings_to_rad() */

double rad_to_settings(double in) {
	switch(active_settings.mode) {
		case degrees:
			return rad2deg(in);
		case radians:
			return in;
	}
	return 0.0;
} /* rad_to_settings() */

error_code eval_rpnstack(stack **rpn, double *ret) {
	stack *tmpstack = malloc(sizeof(stack));
	init_stack(tmpstack);

	s_content stackp;
	int i, pos = 0;
	double *result = NULL, arg[2];
	for(i = 0; i < stack_size(*rpn); i++) {
		/* debugging */
		print_stack(tmpstack);
		stackp = (*rpn)->content[i];
		pos = stackp.position;
		switch(stackp.tp) {
			case number:
				push_valstack(double_dup(*(double *) stackp.val), number, pos, tmpstack);
				break;
			case func:
				if(stack_size(tmpstack) < 1)
					return safe_free_stack(FUNCTION_WRONG_ARGC, pos, &tmpstack, rpn);
				arg[0] = *(double *) top_stack(tmpstack)->val;
				free_scontent(pop_stack(tmpstack));

				if(get_from_ch_list(((function *)stackp.val)->name, angle_infunc_list, false)) /* convert settings angles to radians */
					arg[0] = settings_to_rad(arg[0]);

				result = malloc(sizeof(double));
				*result = ((function *)stackp.val)->get(arg[0]);

				if(get_from_ch_list(((function *)stackp.val)->name, angle_outfunc_list, false)) /* convert radians to settings angles */
					*result = rad_to_settings(*result);

				push_valstack(result, number, pos, tmpstack);
				break;
			case addop:
			case multop:
			case expop:
				if(stack_size(tmpstack) < 2)
					return safe_free_stack(OPERATOR_WRONG_ARGC, pos, &tmpstack, rpn);
				arg[1] = *(double *) top_stack(tmpstack)->val;
				free_scontent(pop_stack(tmpstack));

				arg[0] = *(double *) top_stack(tmpstack)->val;
				free_scontent(pop_stack(tmpstack));

				result = malloc(sizeof(double));
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
					case '/':
						if(!arg[1]) {
							free(result);
							return safe_free_stack(DIVIDE_BY_ZERO, pos, &tmpstack, rpn);
						}
						*result = arg[0] / arg[1];
						break;
					case '%':
						if(!arg[1]) {
							free(result);
							return safe_free_stack(MODULO_BY_ZERO, pos, &tmpstack, rpn);
						}
						*result = fmod(arg[0], arg[1]);
						break;
					case '^':
						*result = pow(arg[0], arg[1]);
						break;
					default:
						return safe_free_stack(UNKNOWN_TOKEN, pos, &tmpstack, rpn);
				}
				push_valstack(result, number, pos, tmpstack);
				break;
			default:
				return safe_free_stack(UNKNOWN_ERROR, pos, &tmpstack, rpn);
		}
	}
	if(stack_size(tmpstack) != 1)
		return safe_free_stack(TOO_MANY_VALUES, pos, &tmpstack, rpn);

	*ret = *(double *) top_stack(tmpstack)->val;

	if(has_rounding_error(*ret))
		return safe_free_stack(NUM_OVERFLOW, -1, &tmpstack, rpn);
	return safe_free_stack(SUCCESS, -1, &tmpstack, rpn);
} /* eval_rpnstack() */

char *get_error_msg(error_code error) {
	bool full_err = (error.position > 0 && active_settings.error >= position);
	char *msg = NULL;
	switch(error.code) {
		case DIVIDE_BY_ZERO:
			if(full_err)
				msg = "Cannot divide by zero @ %d.";
			else
				msg = "Cannot divide by zero.";
			break;
		case MODULO_BY_ZERO:
			if(full_err)
				msg = "Cannot modulo by zero @ %d.";
			else
				msg = "Cannot modulo by zero.";
			break;
		case UNMATCHED_LEFT_PARENTHESIS:
			if(full_err)
				msg = "Missing closing parenthesis for opening parenthesis @ %d.";
			else
				msg = "Missing closing parenthesis for opening parenthesis.";
			break;
		case UNMATCHED_RIGHT_PARENTHESIS:
			if(full_err)
				msg = "Missing opening parenthesis for closing parenthesis @ %d.";
			else
				msg = "Missing opening parenthesis for closing parenthesis.";
			break;
		case UNKNOWN_TOKEN:
			if(full_err)
				msg = "Unknown token or function in expression @ %d.";
			else
				msg = "Unknown token or function in expression.";
			break;
		case UNKNOWN_VARIABLE:
			if(full_err)
				msg = "Unknown variable to delete @ %d.";
			else
				msg = "Unknown variable to delete.";
			break;
		case FUNCTION_WRONG_ARGC:
			if(full_err)
				msg = "Not enough arguments for function @ %d.";
			else
				msg = "Not enough arguments for function.";
			break;
		case OPERATOR_WRONG_ARGC:
			if(full_err)
				msg = "Not enough values for operator @ %d.";
			else
				msg = "Not enough values for operator.";
			break;
		case TOO_MANY_VALUES:
			if(full_err)
				msg = "Too many values in expression @ %d.";
			else
				msg = "Too many values in expression.";
			break;
		case EMPTY_STACK:
			if(full_err)
				msg = "Expression was empty @ %d.";
			else
				msg = "Expression was empty.";
			break;
		case NUM_OVERFLOW:
			if(full_err)
				msg = "Number caused overflow @ %d.";
			else
				msg = "Number caused overflow.";
			break;
		case INVALID_VARIABLE_NAME:
			if(full_err)
				msg = "Variable name contains invalid characters @ %d.";
			else
				msg = "Variable name contains invalid characters.";
			break;
		case RESERVED_VARIABLE:
			if(full_err)
				msg = "Variable name is reserved @ %d.";
			else
				msg = "Variable name is reserved.";
			break;
		default:
			if(full_err)
				msg = "An unknown error has occured @ %d.";
			else;
				msg = "An unknown error has occured.";
			break;
	}

	free(error_msg_container);
	if(full_err) {
		error_msg_container = malloc(lenprintf(msg, error.position));
		sprintf(error_msg_container, msg, error.position);
	} else {
		error_msg_container = malloc(strlen(msg) + 1);
		strcpy(error_msg_container, msg);
	}

#ifdef __DEBUG__
	printf("position of error: %d\n", error.position);
#endif

	return error_msg_container;
} /* get_error_msg() */

char *get_error_msg_pos(int code, int pos) {
	return get_error_msg(to_error_code(code, pos));
} /* get_error_msg_pos() */

error_code compute_infix_string(char *original_str, double *result) {
	assert(synge_started);
	stack *rpn_stack = malloc(sizeof(stack)), *infix_stack = malloc(sizeof(stack));
	init_stack(rpn_stack);
	init_stack(infix_stack);
	error_code ecode = to_error_code(SUCCESS, -1);

	char *final_pass_str = replace(original_str, " ", "");
	char *string = NULL, *var = NULL;

	if(strchr(final_pass_str, '=')) {
		var = final_pass_str;
		string = strrchr(final_pass_str, '=');
		*string++ = '\0';

		char *endptr = NULL, *word = get_word(var, SYNGE_VARIABLE_CHARS "=", &endptr);
		if(strlen(word) != strlen(var))
			ecode = to_error_code(INVALID_VARIABLE_NAME, -1);
		free(word);
	}
	else string = final_pass_str;

	if(ecode.code == SUCCESS)
		/* generate infix stack */
		if((ecode = tokenise_string(string, &infix_stack)).code == SUCCESS)
			/* convert to postfix (or RPN) stack */
			if((ecode = infix_stack_to_rpnstack(&infix_stack, &rpn_stack)).code == SUCCESS)
				/* evaluate postfix (or RPN) stack */
				if((ecode = eval_rpnstack(&rpn_stack, result)).code == SUCCESS)
					/* set the answer variable */
					set_special_number(SYNGE_PREV_ANSWER, *result, number_list);

	int operation = 0;
	if(var && ((ecode.code == SUCCESS && ++operation) || (ecode.code == EMPTY_STACK && --operation))) {
		char *tmp = --var;
		do {
			tmp++;
			char *tmpp = NULL, *tmpword = get_word(tmp, SYNGE_VARIABLE_CHARS, &tmpp);
			if(strlen(tmp) < 1 || strlen(tmpword) < 1 || isspecialnum(tmpword)) {
				ecode = to_error_code(INVALID_VARIABLE_NAME, -1);
				operation = 0;
				free(tmpword);
				break;
			}
			free(tmpword);
		} while((tmp = strchr(tmp, '=')) != NULL);

		if(operation) {
			do {
				var++;
				if(operation == 1) ecode = set_variable(var, *result);
				else if(operation == -1) ecode = del_variable(var);
#ifdef __DEBUG__
				char *tmpp, *tmp = get_word(var, SYNGE_VARIABLE_CHARS, &tmpp);
				printf("%s -> %s\n", tmp, string);
				free(tmp);
#endif
			} while((var = strchr(var, '=')) != NULL);
		}
	}

	free(final_pass_str);
	return safe_free_stack(ecode.code, ecode.position, &infix_stack, &rpn_stack);
} /* calculate_infix_string() */

synge_settings get_synge_settings(void) {
	return active_settings;
} /* get_synge_settings() */

void set_synge_settings(synge_settings new_settings) {
	active_settings = new_settings;
} /* set_synge_settings() */

function *get_synge_function_list(void) {
	return func_list;
} /* get_synge_function_list() */

void synge_start(void) {
	variable_list = ohm_init(2, NULL);
	synge_started = true;
} /* synge_end() */

void synge_end(void) {
	if(variable_list) ohm_free(variable_list);
	free(error_msg_container);
	synge_started = false;
} /* synge_end() */
