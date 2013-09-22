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

#include "synge.h"
#include "version.h"
#include "global.h"
#include "common.h"
#include "stack.h"
#include "ohmic.h"
#include "linked.h"

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

static bool valid_base_char(char digit, int base) {
	char valid_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

	int i;
	for(i = 0; i < base; i++)
		if(toupper(digit) == toupper(valid_digits[i]))
			return true;

	return false;
} /* valid_base_char() */

#define isstrop(str) (get_op(str).tp != op_none)

static error_code synge_strtofr(synge_t *num, char *str, char **endptr) {
	/* NOTE: all signops are operators now, so ignore them here */
	if(issignop(str)) {
		*endptr = str;
		return to_error_code(SUCCESS, -1);
	}

	int base = 10;

	/* all special bases begin with 0_ but 0. doesn't count */
	if(strlen(str) <= 2 || *str != '0' || isstrop(str + 1) || isstrop(str + 2)) {
		/* default to decimal */
		mpfr_strtofr(*num, str, endptr, base, SYNGE_ROUND);
		return to_error_code(SUCCESS, -1);
	} else {
		/* go past the first 0 */
		str++;
		switch(*str) {
			case 'x':
				/* hexadecimal */
				str++;
				base = 16;
				break;
			case 'd':
				/* decimal */
				str++;
				base = 10;
				break;
			case 'b':
				/* binary */
				str++;
				base = 2;
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
				base = 8;
				break;
			default:
				str--;
				base = 10;
				break;
		}
	}

	if(!valid_base_char(*str, base))
		return to_error_code(BASE_CHAR, -1);

	mpfr_strtofr(*num, str, endptr, base, SYNGE_ROUND);
	return to_error_code(SUCCESS, -1);
} /* synge_strtofr() */

static bool isnum(char *string) {
	/* get variable word */
	char *endptr = NULL, *s = get_word(string, SYNGE_WORD_CHARS, &endptr);
	endptr = NULL;

	/* get synge_t number from string */
	synge_t tmp;
	mpfr_init2(tmp, SYNGE_PRECISION);
	error_code tmpcode = synge_strtofr(&tmp, string, &endptr);
	mpfr_clears(tmp, NULL);

	/* all cases where word is a number */
	int ret = ((s && get_special_num(s).name) || string != endptr || tmpcode.code == BASE_CHAR);
	free(s);
	return ret;
} /* isnum() */

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

error_code synge_tokenise_string(char *string, stack **infix_stack) {
	assert(synge_started == true, "synge must be initialised");

	_debug("--\nTokenise\n--\n");
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
				error_code tmpcode = synge_strtofr(num, string + i, &endptr);

				if(!synge_is_success_code(tmpcode.code)) {
					mpfr_clear(*num);
					free(num);
					return to_error_code(tmpcode.code, pos);
				}

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
