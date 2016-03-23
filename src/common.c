/* Synge: A shunting-yard calculation "engine"
 * Copyright (C) 2013 Aleksa Sarai
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

void _print_stack(char *fname, struct stack *s) {
#if defined(SYNGE_DEBUG)
	int i, size = stack_size(s);
	fprintf(stderr, "%s: ", fname);

	for(i = 0; i < size; i++) {
		struct stack_cont tmp = s->content[i];

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
	fflush(stderr);
#endif /* SYNGE_DEBUG */
} /* _print_stack() */

void _debug(char *format, ...) {
#if defined(SYNGE_DEBUG)
	va_list ap;
	va_start(ap, format);
	synge_vfprintf(stderr, format, ap);
	va_end(ap);
#endif /* SYNGE_DEBUG */
} /* _debug() */

void cheeky(char *format, ...) {
#if defined(SYNGE_CHEEKY)
	va_list ap;
	va_start(ap, format);
	synge_vfprintf(stderr, format, ap);
	va_end(ap);
#endif /* SYNGE_CHEEKY */
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

struct synge_op get_op(char *ch) {
	int i;
	struct synge_op ret = {NULL, op_none};
	for(i = 0; op_list[i].str != NULL; i++)
		if((!ret.str || strlen(op_list[i].str) > strlen(ret.str)) && /* longest match */
			!strncmp(op_list[i].str, ch, strlen(op_list[i].str)))
				ret = op_list[i];

	return ret;
} /* get_op() */

struct synge_const get_special_num(char *s) {
	struct synge_const ret = {NULL, NULL, NULL};
	int i;

	for(i = 0; constant_list[i].name != NULL; i++)
		if(!strcmp(constant_list[i].name, s))
			return constant_list[i];

	return ret;
} /* get_special_num() */

char *get_word(char *string, char *list, char **endptr) {
	/* get word pointer */
	int len = strspn(string, list);

	/* no word found */
	if(!len)
		return NULL;

	/* copy over the word */
	char *ret = malloc(len + 1);
	memcpy(ret, string, len);
	ret[len] = '\0';

	*endptr = string + len;
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
	} while((p += strspn(p, list)) != cur || *(p++) != '\0');

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

struct synge_err to_error_code(int err, int pos) {
	/* fill a struct with the given values */
	struct synge_err ret = {
		.code = err,
		.position = pos
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
