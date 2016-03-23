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
#include <stdarg.h>

#include "stack.h"

void init_stack(struct stack *s) {
	s->content = NULL;
	s->size = 0;
	s->top = -1;
} /* init_stack() */

void push_ststack(struct stack_cont con, struct stack *s) {
	if(s->top + 1 >= s->size) /* if stack is full */
		s->content = realloc(s->content, ++s->size * sizeof(struct stack_cont));

	s->top++;
	s->content[s->top] = (struct stack_cont) {
		.val = con.val,
		.tp = con.tp,
		.tofree = con.tofree,
		.freefunc = con.freefunc,
		.position = con.position
	};
} /* push_ststack() */

void push_valstack(void *val, int tp, int tofree, void (*freefunc)(void *), int pos, struct stack *s) {
	if(s->top + 1 >= s->size) /* if stack is full */
		s->content = realloc(s->content, ++s->size * sizeof(struct stack_cont));

	s->top++;
	s->content[s->top] = (struct stack_cont) {
		.val = val,
		.tp = tp,
		.tofree = tofree,
		.freefunc = freefunc,
		.position = pos
	};
} /* push_valstack() */

struct stack_cont *pop_stack(struct stack *s) {
	struct stack_cont *ret = NULL;

	if(s->top > -1 && s->content)
		ret = &s->content[s->top--];

	return ret;
} /* pop_stack() */

struct stack_cont *top_stack(struct stack *s) {
	struct stack_cont *ret = NULL;

	if(s->top > -1 && s->content)
		ret = &(s->content[s->top]);

	return ret;
} /* top_stack() */

void free_stack_cont(struct stack_cont *s) {
	if(!s)
		return;

	if(s->tofree) {
		if(s->freefunc && s->val)
			s->freefunc(s->val);

		free(s->val);
		s->val = NULL;
	}

	s->position = -1;
} /* free_stack_cont() */

void free_stack(struct stack *s) {
	if(!s || !s->content)
		return;

	int i;
	for(i = 0; i < s->size; i++)
		if(s->content[i].tofree)
			free_stack_cont(&s->content[i]);

	free(s->content);
	s->content = NULL;
	s->size = 0;
	s->top = -1;
} /* free_stack() */

void ufree_stackm(struct stack **s, ...) {
	va_list ap;

	va_start(ap, s);
	do {
		free_stack(*s);
		free(*s);
		*s = NULL;
	} while((s = va_arg(ap, struct stack **)) != NULL);
	va_end(ap);
} /* safe_free_stack() */
