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
#include "common.h"
#include "global.h"
#include "stack.h"
#include "ohmic.h"
#include "linked.h"

#define SYNGE_HM_SIZE 42

/* for windows, define strcasecmp and strncasecmp */
#if defined(_WINDOWS)
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
#endif /* _WINDOWS */

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

static char *get_trace(struct link_t *link) {
	char *ret = NULL, *current = NULL;
	struct link_iter*ii = link_iter_init(link);

	ret = malloc(1);
	*ret = '\0';

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

static char *get_error_type(struct synge_err error) {
	switch(error.code) {
		case BASE_CHAR:
			return "BaseError";
			break;
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

char *synge_error_msg(struct synge_err error) {
	char *msg = NULL;

	/* get correct printf string */
	switch(error.code) {
		case BASE_CHAR:
			cheeky("Base Z? Every base is base 10.");
			msg = "Invalid base character";
			break;
		case DIVIDE_BY_ZERO:
			cheeky("The 11th Commandment: Thou shalt not divide by zero.\n");
			msg = "Cannot divide by zero";
			break;
		case MODULO_BY_ZERO:
			cheeky("The 11th Commandment: Thou shalt not modulo by zero.\n");
			msg = "Cannot modulo by zero";
			break;
		case UNMATCHED_LEFT_PARENTHESIS:
			cheeky("(an unmatched left parenthesis creates tension that will stay with you all day");
			msg = "Missing closing bracket for opening bracket";
			break;
		case UNMATCHED_RIGHT_PARENTHESIS:
			cheeky("Error: Smilies are not supported ;)");
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
			cheeky("Error: delete(universe) is not a command.");
			msg = "Invalid word to delete";
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
			cheeky("There's an expression missing here.\n");
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
			cheeky("Synge dun goofed.\n");
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

struct synge_err synge_internal_compute_string(char *string, synge_t *result, char *caller, int position) {
	assert(synge_started == true, "synge must be initialised");

	/* "dynamically" resize hashmap to keep efficiency up */
	if(ohm_count(variable_list) > ohm_size(variable_list))
		variable_list = ohm_resize(variable_list, ohm_size(variable_list) * 2);

	/* backup variable and function hashmaps are
	 * used to "roll back" the maps to a known good
	 * state if an error occurs */
	struct ohm_t *backup_var = ohm_dup(variable_list);
	struct ohm_t *backup_func = ohm_dup(expression_list);

	/* duplicate all variables */
	struct ohm_iter i = ohm_iter_init(variable_list);
	for(; i.key; ohm_iter_inc(&i)) {
		synge_t new, *old = i.value;

		mpfr_init2(new, SYNGE_PRECISION);
		mpfr_set(new, *old, SYNGE_ROUND);

		ohm_insert(backup_var, i.key, i.keylen, new, sizeof(synge_t));
	}

	/* intiialise result to zero */
	mpfr_set_si(*result, 0, SYNGE_ROUND);

	/* We have delved too greedily and too deeply.
	 * We have awoken a creature in the darkness of recursion.
	 * A creature of shadow, flame and segmentation faults.
	 * YOU SHALL NOT PASS! */

	static int depth = -1;
	if(++depth >= SYNGE_MAX_DEPTH) {
		ohm_free(backup_func);

		struct ohm_iter ii = ohm_iter_init(backup_var);
		for(; ii.key; ohm_iter_inc(&ii)) {
			synge_t *val = ii.value;
			mpfr_clear(*val);
		}

		ohm_free(backup_var);

		cheeky("YOU SHALL NOT PASS!\n");
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
	debug("expression '%s'\n", string);

	/* initialise all local variables */
	struct stack *rpn_stack = malloc(sizeof(struct stack)), *infix_stack = malloc(sizeof(struct stack));
	init_stack(rpn_stack);
	init_stack(infix_stack);
	struct synge_err ecode = to_error_code(SUCCESS, -1);

	/* generate infix stack */
	if(ecode.code == SUCCESS)
		ecode = synge_lex_string(string, &infix_stack);

	/* convert to postfix (or RPN) stack */
	if(ecode.code == SUCCESS)
		ecode = synge_infix_parse(&infix_stack, &rpn_stack);

	/* evaluate postfix (or RPN) stack */
	if(ecode.code == SUCCESS)
		ecode = synge_eval_rpnstack(&rpn_stack, result);

	/* measure depth, not length */
	depth--;

	/* fix up negative zeros */
	if(iszero(*result))
		mpfr_abs(*result, *result, SYNGE_ROUND);

	/* is it a nan? */
	if(mpfr_nan_p(*result))
		ecode = to_error_code(UNDEFINED, -1);

	/* if some error occured, revert variables and functions back to previous good state */
	if(!synge_is_success_code(ecode.code) && !synge_is_ignore_code(ecode.code)) {
		/* mpfr_clear variable list */
		struct ohm_iter i = ohm_iter_init(variable_list);
		for(; i.key != NULL; ohm_iter_inc(&i))
			mpfr_clear(i.value);

		ohm_cpy(variable_list, backup_var);
		ohm_cpy(expression_list, backup_func);
	}

	/* no error -- clear backup variable list */
	else {
		struct ohm_iter j = ohm_iter_init(backup_var);
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

/* public "exported" interface for above function */
struct synge_err synge_compute_string(char *expression, synge_t *result) {
	return synge_internal_compute_string(expression, result, SYNGE_MAIN, 0);
} /* synge_compute_string() */

struct synge_settings synge_get_settings(void) {
	return active_settings;
} /* get_synge_settings() */

void synge_set_settings(struct synge_settings new_settings) {
	active_settings = new_settings;

	/* sanitise precision */
	if(new_settings.precision > SYNGE_MAX_PRECISION)
		active_settings.precision = SYNGE_MAX_PRECISION;
} /* set_synge_settings() */

struct synge_func *synge_get_function_list(void) {
	return func_list;
} /* get_synge_function_list() */

struct ohm_t *synge_get_variable_list(void) {
	return variable_list;
} /* synge_get_variable_list() */

struct ohm_t *synge_get_expression_list(void) {
	return expression_list;
} /* synge_get_expression_list() */

static int size_list(struct synge_const *list) {
	int i, len = 0;
	for(i = 0; list[i].name != NULL; i++)
		len++;
	return len;
} /* size_list() */

struct synge_word *synge_get_constant_list(void) {
	int len = size_list(constant_list);
	struct synge_word *ret = malloc(len * sizeof(struct synge_word));

	int i;
	for(i = 0; i < len; i++) {
		ret[i] = (struct synge_word) {
			.name = constant_list[i].name,
			.description = constant_list[i].description
		};
	}

	return ret;
} /* synge_get_constant_list() */

void synge_seed(unsigned int seed) {
	assert(synge_started == true, "synge must be initialised");
	gmp_randseed_ui(synge_state, seed);
} /* synge_seed() */

void synge_start(void) {
	assert(synge_started == false, "synge mustn't be initialised");

	variable_list = ohm_init(SYNGE_HM_SIZE, NULL);
	expression_list = ohm_init(SYNGE_HM_SIZE, NULL);
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
	struct ohm_iter i = ohm_iter_init(variable_list);
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

struct synge_ver synge_get_version(void) {
	/* default "version" */
	struct synge_ver ret = {
		.version = SYNGE_VERSION,
		.compiled = __TIME__ ", " __DATE__,
		.revision = "<unknown>"
	};

#if defined(SYNGE_REVISION)
	ret.revision = SYNGE_REVISION;
#endif /* SYNGE_REVISION */

	return ret;
} /* synge_version() */
