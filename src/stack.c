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

#include <stdlib.h>
#include <stdarg.h>

#include "stack.h"
#include "calculator.h"

void init_stack(stack *s) {
	s->content = NULL;
	s->size = 0;
	s->top = -1;
} /* init_stack() */

void push_ststack(s_content con, stack *s) {
	if(s->top + 1 >= s->size) /* if stack is full */
		s->content = (s_content *) realloc(s->content, ++s->size * sizeof(s_content));
	s->top++;
	s->content[s->top].val = con.val;
	s->content[s->top].tp = con.tp;
} /* push_ststack() */

void push_valstack(void *val, s_type tp, stack *s) {
	if(s->top + 1 >= s->size) /* if stack is full */
		s->content = (s_content *) realloc(s->content, ++s->size * sizeof(s_content));
	s->top++;
	s->content[s->top].val = val;
	s->content[s->top].tp = tp;
} /* push_valstack() */

s_content pop_stack(stack *s) {
	s_content ret = {NULL, none};
	if(s->top > -1 && s->content) {
		ret = s->content[s->top--];
		s->size--;
	}
	return ret;
} /* pop_stack() */

s_content top_stack(stack *s) {
	s_content ret = {NULL, none};
	if(s->top > -1 && s->content) ret = s->content[s->top];
	return ret;
} /* top_stack() */

void free_stack(stack *s) {
	/*
	 *int i;
	 *for(i = 0; i < s->size; i++)
	 *        if(s->content[i].tp == number)
	 *                free(s->content[i].val);
	 */
	free(s->content);
	s->content = NULL;
	s->size = 0;
	s->top = -1;
} /* free_stack() */

error_code usafe_free_stack(bool full_free, error_code ecode, stack *s, ...) {
	va_list ap;
	int i;

	va_start(ap, s);
	do {
		if(full_free)
			for(i = 0; i < s->size; i++) if(s->content[i].tp == number) free(s->content[i].val);
		free_stack(s);
		free(s);
	} while((s = va_arg(ap, stack *)) != NULL);
	va_end(ap);

	return ecode;
} /* safe_free_stack() */
