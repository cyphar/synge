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

#include "synge.h"
#include "version.h"
#include "global.h"
#include "common.h"
#include "stack.h"
#include "ohmic.h"
#include "linked.h"

/* greedy finder */
static char *get_from_ch_list(char *ch, char **list) {
	char *ret = NULL;

	int i;
	for(i = 0; list[i] != NULL; i++)
		/* checks against part or entire string against the given list */
		if((!ret || strlen(ret) < strlen(list[i])) && !strcmp(list[i], ch))
			ret = list[i];

	return ret;
} /* get_from_ch_list() */

static struct synge_err set_variable(char *str, synge_t val) {
	assert(synge_started == true, "synge must be initialised");
	char *endptr = NULL, *s = get_word(str, SYNGE_WORD_CHARS, &endptr);

	/* SYNGE_PREV_EXPRESSION is immutable */
	if(!strcmp(s, SYNGE_PREV_EXPRESSION)) {
		free(s);
		return to_error_code(INVALID_LEFT_OPERAND, -1);
	}

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

static struct synge_err set_function(char *str, char *exp) {
	assert(synge_started == true, "synge must be initialised");
	char *endptr = NULL, *s = get_word(str, SYNGE_WORD_CHARS, &endptr);

	/* SYNGE_PREV_EXPRESSION is immutable */
	if(!strcmp(s, SYNGE_PREV_EXPRESSION)) {
		free(s);
		return to_error_code(INVALID_LEFT_OPERAND, -1);
	}

	/* save the function */
	ohm_remove(variable_list, s, strlen(s) + 1); /* remove word from variable list (fake dynamic typing) */
	ohm_insert(expression_list, s, strlen(s) + 1, exp, strlen(exp) + 1);

	free(s);
	return to_error_code(SUCCESS, -1);
} /* set_function() */

static struct synge_err del_word(char *s, int pos) {
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

static struct synge_err eval_word(char *str, int pos, synge_t *result) {
	if(ohm_search(variable_list, str, strlen(str) + 1)) {
		synge_t *value = ohm_search(variable_list, str, strlen(str) + 1);
		mpfr_set(*result, *value, SYNGE_ROUND);
	} else if(ohm_search(expression_list, str, strlen(str) + 1)) {
		/* recursively evaluate a user function's value */
		char *expression = ohm_search(expression_list, str, strlen(str) + 1);
		struct synge_err tmpecode = synge_internal_compute_string(expression, result, str, pos);

		/* error was encountered */
		if(!synge_is_success_code(tmpecode.code)) {
			if(active_settings.error == traceback)
				/* return real error code for traceback */
				return tmpecode;
			else
				/* return relative error code for all other error formats */
				return to_error_code(tmpecode.code, pos);
		}
	} else {
		/* unknown variable or function */
		return to_error_code(UNKNOWN_TOKEN, pos);
	}

	/* is the result a nan? */
	if(mpfr_nan_p(*result))
		return to_error_code(UNDEFINED, pos);

	return to_error_code(SUCCESS, -1);
} /* eval_word() */

static struct synge_err eval_expression(char *exp, char *caller, int pos, synge_t *result) {
	struct synge_err ret = synge_internal_compute_string(exp, result, caller, pos);

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
struct synge_err synge_eval_rpnstack(struct stack **rpn, synge_t *output) {
	struct stack *evalstack = malloc(sizeof(struct stack));
	init_stack(evalstack);

	_debug("--\nEvaluator\n--\n");

	int i, tmp = 0, size = stack_size(*rpn);
	synge_t *result = NULL, arg[3];
	struct synge_err ecode[2];

	/* initialise operators */
	mpfr_inits2(SYNGE_PRECISION, arg[0], arg[1], arg[2], NULL);

	for(i = 0; i < size; i++) {
		/* shorthand variables */
		struct stack_cont stackp = (*rpn)->content[i];
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
					if(top_stack(evalstack)->tp == number) {
						/* variable value */
						mpfr_set(arg[0], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
					} else if(top_stack(evalstack)->tp == expression) {
						/* function expression value */
						tmpexp = str_dup(top_stack(evalstack)->val);
					} else {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					free_stack_cont(pop_stack(evalstack));

					/* get word */
					if(top_stack(evalstack)->tp != setword) {
						free(tmpexp);
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					char *tmpstr = str_dup(top_stack(evalstack)->val);
					free_stack_cont(pop_stack(evalstack));

					/* set variable or function */
					switch(get_op(stackp.val).tp) {
						case op_var_set:
							ecode[0] = set_variable(tmpstr, arg[0]);
							break;
						case op_func_set:
							ecode[0] = set_function(tmpstr, tmpexp);
							break;
						default:
							ecode[0] = to_error_code(UNKNOWN_ERROR, pos);
							break;
					}

					/* check if an error occured in the definitions */
					if(!synge_is_success_code(ecode[0].code)) {
						free(tmpstr);
						free(tmpexp);
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(ecode[0].code, pos);
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

						/* when setting functions, we ignore any errors
						 * and any errors with setting a variable would have already been reported */
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
					free_stack_cont(pop_stack(evalstack));

					/* get variable to modify */
					if(top_stack(evalstack)->tp != setword) {
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					/* get variable name */
					char *tmpstr = str_dup(top_stack(evalstack)->val);
					free_stack_cont(pop_stack(evalstack));

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
					free_stack_cont(pop_stack(evalstack));

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
					free_stack_cont(pop_stack(evalstack));

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
					free_stack_cont(pop_stack(evalstack));

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
				free_stack_cont(pop_stack(evalstack));

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
					free_stack_cont(pop_stack(evalstack));

					/* get if value */
					if(top_stack(evalstack)->tp != expression) {
						free(tmpelse);
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(UNKNOWN_ERROR, pos);
					}

					char *tmpif = str_dup(top_stack(evalstack)->val);
					free_stack_cont(pop_stack(evalstack));

					/* get if condition */
					if(top_stack(evalstack)->tp != number) {
						free(tmpif);
						free(tmpelse);
						free_stackm(&evalstack, rpn);
						mpfr_clears(arg[0], arg[1], arg[2], NULL);
						return to_error_code(UNKNOWN_ERROR, pos);
					}

					mpfr_set(arg[0], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
					free_stack_cont(pop_stack(evalstack));

					result = malloc(sizeof(synge_t));
					mpfr_init2(*result, SYNGE_PRECISION);

					struct synge_err tmpecode;

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
				free_stack_cont(pop_stack(evalstack));

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
				free_stack_cont(pop_stack(evalstack));

				/* get first argument */
				mpfr_set(arg[0], SYNGE_T(top_stack(evalstack)->val), SYNGE_ROUND);
				free_stack_cont(pop_stack(evalstack));

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

						if(tmp)
							mpfr_trunc(*result, *result);
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

	/* free temporary numbers */
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
