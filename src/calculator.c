/* Synge: A shunting-yard calculator "engine"
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
#include "calculator.h"

/* Operator Precedence
 * 4 Parenthesis
 * 3 Exponents, Roots (right associative)
 * 2 Multiplication, Division, Modulus (left associative)
 * 1 Addition, Subtraction (left associative)
 */

typedef struct __function__ {
	char *name;
	double (*get)(double);
} function;

function func_list[] = {
	{"abs",		 fabs},
	{"sqrt",	 sqrt},

	{"floor",	floor},
	{"ceil",	 ceil},

	{"log10",	log10},
	{"log",		  log},

	{"sin",		  sin},
	{"cos",		  cos},
	{"tan",		  tan},

	{"asin",	 asin},
	{"acos",	 acos},
	{"atan",	 atan}
};

typedef struct __special_number__ {
	char *name;
	double value;
} special_number;

special_number number_list[] = {
	{"pi",	3.14159265359},
	{"e",	2.71828182845},
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
	","
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

bool isonechar(char *ch) {
	switch(*ch) {
		case '+':
		case '-':
		case '*':
		case '/':
		case '%':
		case '^':
		case '(':
		case ')':
		case ',':
			return true;
			break;
		default:
			return false;
			break;
	}
} /* isonechar() */

bool isop(s_type type) {
	return (type == addop || type == multop || type == expop);
} /* isop() */

bool isspecialch(s_type type) {
	return (type == lparen || type == rparen || type == argsep);
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

bool isnum(char *s) {
	if(isspecialnum(s)) return true;
	char *endptr = NULL;
	strtold(s, &endptr);
	if(s != endptr) return true;
	else return false;
} /* isnum() */

function *get_func(char *val) {
	int i;
	for(i = 0; i < length(func_list); i++)
		if(!strncmp(val, func_list[i].name, strlen(func_list[i].name))) return &func_list[i];
	return NULL;
} /* get_func() */

char *get_onechar(char *s) {
	int i;
	for(i = 0; i < length(op_list); i++)
		if(!strncmp(s, op_list[i], strlen(op_list[i]))) return op_list[i];
	return NULL;
} /* get_op() */

error_code tokenise_string(char *string, stack *ret) {
	char *s = replace(string, " ", "");
	init_stack(ret);
	int i;
	for(i = 0; i < strlen(s); i++) {
		if(isnum(s+i) && (!i || (i > 0 && top_stack(ret).tp != number && top_stack(ret).tp != rparen))) {
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
			push_valstack(num, number, ret);
		}
		else if(isonechar(s+i)) {
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
				case ',':
					type = argsep;
					break;
				default:
					return UNKNOWN_TOKEN;
			}
			push_valstack(get_onechar(s+i), type, ret);
		}	
		else if(get_func(s+i)) {
			function *funcname = get_func(s+i);
			push_valstack(funcname, func, ret);
			i += strlen(funcname->name) - 1;
		}
		else { /* unknown token */
			return UNKNOWN_TOKEN;
		}
	}
	if(!stack_size(ret)) return EMPTY_STACK;
	/* debugging */
	print_stack(ret);
	return SUCCESS;
} /* tokenise_string() */

error_code infix_stack_to_rpnstack(stack *infix_stack, stack *rpn_stack) {
	s_content stackp, tmpstackp;
	init_stack(rpn_stack);

	stack *op_stack = malloc(sizeof(stack));
	init_stack(op_stack);
	int i, found;
	for(i = 0; i < stack_size(infix_stack); i++) {
		stackp = infix_stack->content[i];
		switch(stackp.tp) {
			case number:
				push_ststack(stackp, rpn_stack);
				break;
			case lparen:
			case func:
				push_ststack(stackp, op_stack);
				break;
			case argsep:
			case rparen:
				found = false;
				while(stack_size(op_stack)) {
					tmpstackp = pop_stack(op_stack);
					if(tmpstackp.tp == lparen) {
						found = true;
						break;
					}
					push_ststack(tmpstackp, rpn_stack);
				}
				if(!found) return safe_free_stack(true, UNMATCHED_PARENTHESIS, infix_stack, op_stack, rpn_stack);
				break;
			case addop:
			case multop:
			case expop:
				if(stack_size(op_stack)) {
					while(stack_size(op_stack)) {
						tmpstackp = top_stack(op_stack);
						if(!isop(tmpstackp.tp)) break;
						if(op_precedes(tmpstackp.tp, stackp.tp)) push_ststack(pop_stack(op_stack), rpn_stack);
						else break;
					}
				}
				push_ststack(stackp, op_stack);
				break;
			default:
				return safe_free_stack(true, UNKNOWN_TOKEN, infix_stack, op_stack, rpn_stack);
		}
	}

	while(stack_size(op_stack)) {
		stackp = pop_stack(op_stack);
		if(isspecialch(stackp.tp))
			return safe_free_stack(true, UNMATCHED_PARENTHESIS, infix_stack, op_stack, rpn_stack);
		push_ststack(stackp, rpn_stack);
	}

	/* debugging */
	print_stack(rpn_stack);
	return safe_free_stack(false, SUCCESS, infix_stack, op_stack);
} /* populate_stack() */

error_code eval_rpnstack(stack *rpn, double *ret) {
	stack *tmpstack = malloc(sizeof(stack));
	init_stack(tmpstack);

	s_content stackp;
	int i;
	double *result = NULL, arg[2];
	for(i = 0; i < stack_size(rpn); i++) {
		/* debugging */
		print_stack(tmpstack);
		stackp = rpn->content[i];
		switch(stackp.tp) {
			case number:
				push_ststack(stackp, tmpstack);
				break;
			case func:
				if(stack_size(tmpstack) < 1)
					return safe_free_stack(true, WRONG_NUM_VALUES, tmpstack, rpn);
				arg[0] = *(double *) pop_stack(tmpstack).val;
				result = malloc(sizeof(double));

				*result = ((function *)stackp.val)->get(arg[0]);
				push_valstack(result, number, tmpstack);
				break;
			case addop:
			case multop:
			case expop:
				if(stack_size(tmpstack) < 2)
					return safe_free_stack(true, WRONG_NUM_VALUES, tmpstack, rpn);
				arg[1] = *(double *) pop_stack(tmpstack).val;
				arg[0] = *(double *) pop_stack(tmpstack).val;
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
						if(!arg[1]) return DIVIDE_BY_ZERO;
						*result = arg[0] / arg[1];
						break;
					case '%':
						if(!arg[1]) return DIVIDE_BY_ZERO;
						*result = fmod(arg[0], arg[1]);
						break;
					case '^':
						*result = pow(arg[0], arg[1]);
						break;
					default:
						return safe_free_stack(true, UNKNOWN_TOKEN, tmpstack, rpn);
				}
				push_valstack(result, number, tmpstack);
				break;
			default:
				return safe_free_stack(true, WRONG_NUM_VALUES, tmpstack, rpn);
		}
	}
	if(stack_size(tmpstack) != 1)
				return safe_free_stack(true, WRONG_NUM_VALUES, tmpstack, rpn);
	*ret = *(double *) top_stack(tmpstack).val;
	return safe_free_stack(false, SUCCESS, tmpstack);
} /* eval_rpnstack() */

char *get_error_msg(error_code error) {
	char *msg = NULL;
	switch(error) {
		case DIVIDE_BY_ZERO:
			msg = "Attempted to divide or modulo by zero.";
			break;
		case UNMATCHED_PARENTHESIS:
			msg = "Missing parenthesis in evalutation.";
			break;
		case UNKNOWN_TOKEN:
			msg = "Unknown token or function in evaluation.";
			break;
		case WRONG_NUM_VALUES:
			msg = "Incorrect number of values for operator or function.";
			break;
		case EMPTY_STACK:
			msg = "Evaluation was empty.";
			break;
		default:
			msg = "An unknown error has occured.";
			break;
	}
	return msg;
} /* print_error() */

error_code compute_infix_string(char *string, double *result) {
	stack *rpn_stack = malloc(sizeof(stack)), *infix_stack = malloc(sizeof(stack));
	error_code ecode;
	if((ecode = tokenise_string(string, infix_stack)) != SUCCESS) /* generate infix stack */
		return safe_free_stack(true, ecode, infix_stack);
	else if((ecode = infix_stack_to_rpnstack(infix_stack, rpn_stack)) != SUCCESS) /* convert to postfix (or RPN) stack */
		return ecode;
	else if((ecode = eval_rpnstack(rpn_stack, result)) != SUCCESS) /* evaluate postfix (or RPN) stack */
		return ecode;
	else
		return safe_free_stack(true, ecode, rpn_stack);
} /* calculate_infix_string() */
