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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "synge.h"
#include "version.h"
#include "global.h"
#include "common.h"
#include "stack.h"
#include "ohmic.h"
#include "linked.h"

void print_stack(stack *s) {
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

void debug(char *format, ...) {
#ifdef __SYNGE_DEBUG__
	va_list ap;
	va_start(ap, format);
	synge_vfprintf(stderr, format, ap);
	va_end(ap);
#endif
} /* debug() */

void cheeky(char *format, ...) {
#if defined(__SYNGE_CHEEKY__) && __SYNGE_CHEEKY__
	va_list ap;
	va_start(ap, format);
	synge_vfprintf(stderr, format, ap);
	va_end(ap);
#endif
} /* cheeky() */

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

void synge_clear(void *tofree) {
	synge_t *item = tofree;
	mpfr_clear(SYNGE_T(item));
} /* synge_clear() */

operator get_op(char *ch) {
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

special_number get_special_num(char *s) {
	special_number ret = {NULL, NULL, NULL};
	int i;

	for(i = 0; constant_list[i].name != NULL; i++)
		if(!strcmp(constant_list[i].name, s))
			return constant_list[i];

	return ret;
} /* get_special_num() */

char *get_word_ptr(char *string, char *list) {
	int lenstr = strlen(string), lenlist = strlen(list);

	/* go through string */
	int i, j, found = true;
	for(i = 0; i < lenstr; i++) {
		found = false;

		/* check current character against allowed character "list" */
		for(j = 0; j < lenlist && !found; j++)
			if(string[i] == list[j])
				found = true;

		/* current character not in string -- end of word */
		if(!found)
			break;
	}

	/* found end of word */
	return string + i;
} /* get_word_ptr() */

char *get_word(char *string, char *list, char **endptr) {
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

bool contains_word(char *string, char *word, char *list) {
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

char *trim_spaces(char *str) {
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

error_code to_error_code(int error, int position) {
	/* fill a struct with the given values */
	error_code ret = {
		error,
		position
	};
	return ret;
} /* to_error_code() */

int iszero(synge_t num) {
	synge_t epsilon;

	/* generate "epsilon" */
	mpfr_init2(epsilon, SYNGE_PRECISION);
	mpfr_set_str(epsilon, SYNGE_EPSILON, 10, SYNGE_ROUND);

	/* if abs(num) < epsilon then it is zero */
	int cmp = mpfr_cmpabs(num, epsilon);
	mpfr_clears(epsilon, NULL);

	return (cmp < 0 || mpfr_zero_p(num));
} /* iszero() */

int deg_to_rad(synge_t rad, synge_t deg, mpfr_rnd_t round) {
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

int deg_to_grad(synge_t grad, synge_t deg, mpfr_rnd_t round) {
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

int grad_to_deg(synge_t deg, synge_t grad, mpfr_rnd_t round) {
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

int grad_to_rad(synge_t rad, synge_t grad, mpfr_rnd_t round) {
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

int rad_to_deg(synge_t deg, synge_t rad, mpfr_rnd_t round) {
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

int rad_to_grad(synge_t grad, synge_t rad, mpfr_rnd_t round) {
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
