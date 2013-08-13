/* Synge: A shunting-yard calculation "engine"
 * Copyright (C) 2013 Cyphar
 *
 * Permission is hereby granted, tofree of charge, to any person obtaining a copy of
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
#include <stdarg.h>

#include "stack.h"
#include "internal.h"

void init_stack(stack *s) {
	s->content = NULL;
	s->size = 0;
	s->top = -1;
} /* init_stack() */

void push_ststack(s_content con, stack *s) {
	if(s->top + 1 >= s->size) /* if stack is full */
		s->content = realloc(s->content, ++s->size * sizeof(s_content));
	s->top++;
	s->content[s->top].val = con.val;
	s->content[s->top].tp = con.tp;
	s->content[s->top].tofree = con.tofree;
	s->content[s->top].freefunc = con.freefunc;
	s->content[s->top].position = con.position;
} /* push_ststack() */

void push_valstack(void *val, int tp, int tofree, void (*freefunc)(void *), int pos, stack *s) {
	if(s->top + 1 >= s->size) /* if stack is full */
		s->content = realloc(s->content, ++s->size * sizeof(s_content));
	s->top++;
	s->content[s->top].val = val;
	s->content[s->top].tp = tp;
	s->content[s->top].tofree = tofree;
	s->content[s->top].freefunc = freefunc;
	s->content[s->top].position = pos;
} /* push_valstack() */

s_content *pop_stack(stack *s) {
	s_content *ret = NULL;
	if(s->top > -1 && s->content)
		ret = &s->content[s->top--];
	return ret;
} /* pop_stack() */

s_content *top_stack(stack *s) {
	s_content *ret = NULL;
	if(s->top > -1 && s->content)
		ret = &(s->content[s->top]);
	return ret;
} /* top_stack() */

void free_scontent(s_content *s) {
	if(!s) return;
	if(s->tofree) {
		if(s->freefunc && s->val)
			s->freefunc(*(mpfr_t *) s->val);

		free(s->val);
		s->val = NULL;
	}
	s->position = -1;
} /* free_scontent() */

void free_stack(stack *s) {
	if(!s || !s->content) return;
	int i;
	for(i = 0; i < s->size; i++)
		if(s->content[i].tofree)
			free_scontent(&s->content[i]);
	free(s->content);
	s->content = NULL;
	s->size = 0;
	s->top = -1;
} /* free_stack() */

void ufree_stackm(stack **s, ...) {
	va_list ap;

	va_start(ap, s);
	do {
		free_stack(*s);
		free(*s);
		*s = NULL;
	} while((s = va_arg(ap, stack **)) != NULL);
	va_end(ap);
} /* safe_free_stack() */
