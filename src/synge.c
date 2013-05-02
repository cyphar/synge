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
#include <string.h>

#include "stack.h"
#include "synge.h"

/* Operator Precedence
 * 4 Parenthesis
 * 3 Exponents, Roots (right associative)
 * 2 Multiplication, Division, Modulo (left associative)
 * 1 Addition, Subtraction (left associative)
 */

#define PI 3.14159265358979323

double deg2rad(double deg) {
	return deg * (PI / 180.0);
} /* deg2rad() */

double rad2deg(double rad) {
	return rad * (180.0 / PI);
} /* rad2deg() */

typedef struct __function__ {
	char *name;
	double (*get)(double);
} function;

function func_list[] = {
	{"abs",		 fabs},
	{"sqrt",	 sqrt},
	{"cbrt",	 cbrt},

	{"floor",	floor},
	{"ceil",	 ceil},

	{"log10",	log10},
	{"ln",		  log},

	{"deg2rad",   deg2rad},
	{"rad2deg",   rad2deg},

	{"asin",	 asin},
	{"sin",		  sin},

	{"acos",	 acos},
	{"cos",		  cos},

	{"atan",	 atan},
	{"tan",		  tan}
};

typedef struct __special_number__ {
	char *name;
	double value;
} special_number;

special_number number_list[] = {
	{"pi",	3.14159265358979323},
	{"e",	2.71828182845904523},
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
	degrees
};

#define length(x) (sizeof(x) / sizeof(x[0]))

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

char *get_from_ch_list(char *ch, char **list) {
	int i;
	for(i = 0; list[i] != NULL; i++)
		if(!strncmp(list[i], ch, strlen(list[i]))) return list[i];
	return NULL;
} /* get_from_ch_list() */

double *double_dup(double num) {
	double *ret = malloc(sizeof(double));
	*ret = num;
	return ret;
} /* double_dup() */

int get_precision(double num) {
	double tmp;
	num = (1.0 + modf(num, &tmp)) * 10000000000000000;
	while(fmod(num, 10) == 0.0 && num != 0.0)
		num /= 10.0;
	return num ? floor(log10(fabs(num))) : 0;
} /* get_precision() */

bool isop(s_type type) {
	return (type == addop || type == multop || type == expop);
} /* isop() */

bool isspecialch(s_type type) {
	return (type == lparen || type == rparen);
} /* isspecialch() */

bool isspecialnum(char *s) {
	int i;
	for(i = 0; i < length(number_list); i++)
		if(!strncmp(number_list[i].name, s, strlen(number_list[i].name))) return true;
	return false;
} /* isspecialnum() */

special_number getspecialnum(char *s) {
	int i;
	special_number ret = {NULL, 0.0};
	for(i = 0; i < length(number_list); i++)
		if(!strncmp(number_list[i].name, s, strlen(number_list[i].name))) return number_list[i];
	return ret;
} /* getspecialnum() */

function *get_func(char *val) {
	int i;
	for(i = 0; i < length(func_list); i++)
		if(!strncmp(val, func_list[i].name, strlen(func_list[i].name))) return &func_list[i];
	return NULL;
} /* get_func() */

bool isnum(char *s) {
	char *endptr = NULL;
	strtold(s, &endptr);

	if(isspecialnum(s) || s != endptr) return true;
	else return false;
} /* isnum() */

char *function_paren_pad(char *string) {
	char *firstpass = replace(string, "(", "((");
	char *final = replace(firstpass, ")", "))");

	int i;
	for(i = 0; i < length(func_list); i++) {
		char *tmpfrom = malloc(strlen(func_list[i].name) + 2);
		strcpy(tmpfrom, func_list[i].name);
		strcat(tmpfrom, "((");

		char *tmpto = malloc(strlen(func_list[i].name) + 2);
		strcpy(tmpto, "(");
		strcat(tmpto, func_list[i].name);
		strcat(tmpto, "(");

		char *tmpfinal = replace(final, tmpfrom, tmpto);
		free(final);
		final = tmpfinal;

		free(tmpfrom);
		free(tmpto);
	}
	return final;
}

error_code tokenise_string(char *string, stack **ret) {
	char *tmps = replace(string, " ", "");
	char *s = function_paren_pad(tmps);
	free(tmps);
	init_stack(*ret);
	int i;
	for(i = 0; i < strlen(s); i++) {
		if(isnum(s+i) && (!i || (i > 0 && top_stack(*ret)->tp != number && top_stack(*ret)->tp != rparen))) {
			double *num = malloc(sizeof(double));
			if(isspecialnum(s+i)) {
				special_number stnum = getspecialnum(s+i);
				*num = stnum.value;
				i += strlen(stnum.name) - 1;
			} else {
				char *endptr;
				*num = strtold(s+i, &endptr);
				i += (endptr - (s + i)) - 1;
			}
			push_valstack(num, number, *ret);
		}
		else if(get_from_ch_list(s+i, op_list)) {
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
					return UNKNOWN_TOKEN;
			}
			push_valstack(get_from_ch_list(s+i, op_list), type, *ret);
		}
		else if(get_func(s+i)) {
			function *funcname = get_func(s+i);
			push_valstack(funcname, func, *ret);
			i += strlen(funcname->name) - 1;
		}
		else { /* unknown token */
			free(s);
			return UNKNOWN_TOKEN;
		}
	}
	free(s);
	if(!stack_size(*ret)) return EMPTY_STACK;
	/* debugging */
	print_stack(*ret);
	return SUCCESS;
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
	int i, found;
	for(i = 0; i < stack_size(*infix_stack); i++) {
		stackp = (*infix_stack)->content[i];
		switch(stackp.tp) {
			case number:
				push_valstack(double_dup(*(double *) stackp.val), number, *rpn_stack);
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
				if(!found) return safe_free_stack(UNMATCHED_PARENTHESIS, infix_stack, &op_stack, rpn_stack);
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
				return safe_free_stack(UNKNOWN_TOKEN, infix_stack, &op_stack, rpn_stack);
		}
	}

	while(stack_size(op_stack)) {
		stackp = *pop_stack(op_stack);
		if(isspecialch(stackp.tp))
			return safe_free_stack(UNMATCHED_PARENTHESIS, infix_stack, &op_stack, rpn_stack);
		push_ststack(stackp, *rpn_stack);
	}

	/* debugging */
	print_stack(*rpn_stack);
	return safe_free_stack(SUCCESS, infix_stack, &op_stack);
} /* infix_to_rpnstack() */


char *angle_infunc_list[] = {
	"sin",
	"cos",
	"tan",
	NULL
};

char *angle_outfunc_list[] = {
	"asin",
	"acos",
	"atan",
	NULL
};

double settings_to_rad(double in) {
	switch(active_settings.mode) {
		case degrees:
			return deg2rad(in);
		case radians:
			return in;
		default:
			return 0.0;
	}
} /* settings_to_rad() */

double rad_to_settings(double in) {
	switch(active_settings.mode) {
		case degrees:
			return rad2deg(in);
		case radians:
			return in;
		default:
			return 0.0;
	}
} /* rad_to_settings() */

error_code eval_rpnstack(stack **rpn, double *ret) {
	stack *tmpstack = malloc(sizeof(stack));
	init_stack(tmpstack);

	s_content stackp;
	int i;
	double *result = NULL, arg[2];
	for(i = 0; i < stack_size(*rpn); i++) {
		/* debugging */
		print_stack(tmpstack);
		stackp = (*rpn)->content[i];
		switch(stackp.tp) {
			case number:
				push_valstack(double_dup(*(double *) stackp.val), number, tmpstack);
				break;
			case func:
				if(stack_size(tmpstack) < 1)
					return safe_free_stack(WRONG_NUM_VALUES, &tmpstack, rpn);
				arg[0] = *(double *) top_stack(tmpstack)->val;
				free_scontent(pop_stack(tmpstack));

				if(get_from_ch_list(((function *)stackp.val)->name, angle_infunc_list)) /* convert settings angles to radians */
					arg[0] = settings_to_rad(arg[0]);

				result = malloc(sizeof(double));
				*result = ((function *)stackp.val)->get(arg[0]);

				if(get_from_ch_list(((function *)stackp.val)->name, angle_outfunc_list)) /* convert radians to settings angles */
					*result = rad_to_settings(*result);

				push_valstack(result, number, tmpstack);
				break;
			case addop:
			case multop:
			case expop:
				if(stack_size(tmpstack) < 2)
					return safe_free_stack(WRONG_NUM_VALUES, &tmpstack, rpn);
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
							return safe_free_stack(DIVIDE_BY_ZERO, &tmpstack, rpn);
						}
						*result = arg[0] / arg[1];
						break;
					case '%':
						if(!arg[1]) {
							free(result);
							return safe_free_stack(DIVIDE_BY_ZERO, &tmpstack, rpn);
						}
						*result = fmod(arg[0], arg[1]);
						break;
					case '^':
						*result = pow(arg[0], arg[1]);
						break;
					default:
						return safe_free_stack(UNKNOWN_TOKEN, &tmpstack, rpn);
				}
				push_valstack(result, number, tmpstack);
				break;
			default:
				return safe_free_stack(WRONG_NUM_VALUES, &tmpstack, rpn);
		}
	}
	if(stack_size(tmpstack) != 1)
				return safe_free_stack(WRONG_NUM_VALUES, &tmpstack, rpn);
	*ret = *(double *) top_stack(tmpstack)->val;
	return safe_free_stack(SUCCESS, &tmpstack, rpn);
} /* eval_rpnstack() */

char *get_error_msg(error_code error) {
	char *msg = NULL;
	switch(error) {
		case DIVIDE_BY_ZERO:
			msg = "Attempted to divide or modulo by zero.";
			break;
		case UNMATCHED_PARENTHESIS:
			msg = "Missing parenthesis in expression.";
			break;
		case UNKNOWN_TOKEN:
			msg = "Unknown token or function in expression.";
			break;
		case WRONG_NUM_VALUES:
			msg = "Incorrect number of values for operator or function.";
			break;
		case EMPTY_STACK:
			msg = "Expression was empty.";
			break;
		default:
			msg = "An unknown error has occured.";
			break;
	}
	return msg;
} /* get_error_error() */

error_code compute_infix_string(char *string, double *result) {
	stack *rpn_stack = malloc(sizeof(stack)), *infix_stack = malloc(sizeof(stack));
	init_stack(rpn_stack);
	init_stack(infix_stack);
	error_code ecode;
	/* generate infix stack */
	if((ecode = tokenise_string(string, &infix_stack)) == SUCCESS)
		/* convert to postfix (or RPN) stack */
		if((ecode = infix_stack_to_rpnstack(&infix_stack, &rpn_stack)) == SUCCESS)
			/* evaluate postfix (or RPN) stack */
			if((ecode = eval_rpnstack(&rpn_stack, result)) == SUCCESS);

	return safe_free_stack(ecode, &infix_stack, &rpn_stack);
} /* calculate_infix_string() */

synge_settings get_synge_settings(void) {
	return active_settings;
} /* get_synge_settings() */

void set_synge_settings(synge_settings new_settings) {
	active_settings = new_settings;
} /* set_synge_settings() */
