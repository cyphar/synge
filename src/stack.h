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

#pragma once

#include <stdbool.h>
#include "synge.h"

/* stack types */

typedef enum __stack_type__ {
	number,

	addop  = 1,
	multop = 2,
	expop  = 3,

	func,
	arg,

	lparen,
	rparen,
	none
} s_type;

typedef struct __stack_content__ {
	void *val;
	s_type tp;
} s_content;

typedef struct __stack__ {
	s_content *content;
	int size;
	int top;
} stack;

/* stack function prototypes */

void init_stack(stack *); /* initialize the stack */

void push_valstack(void *, s_type, stack *); /* push value and type to the top of the stack */
void push_ststack(s_content, stack *); /* push struct to the top of the stack */

s_content *pop_stack(stack *); /* pops the top value on the stack */
s_content *top_stack(stack *); /* returns the top value on the stack */

void free_scontent(s_content *); /* frees and clears the stack content struct */
void free_stack(stack *); /* frees and clears the stack */

/* wrapper function for free_stack - frees malloc'd memory and free_stacks it */
error_code usafe_free_stack(error_code ecode, stack **s, ...);
#define safe_free_stack(...) usafe_free_stack(__VA_ARGS__, NULL)

#define stack_size(x) ((x)->top + 1)
