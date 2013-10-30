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

#ifndef SYNGE_STACK_H
#define SYNGE_STACK_H

/* stack types */

struct stack_cont {
	void *val;
	int tp;
	int tofree;
	void (*freefunc)(void *);
	int position;
};

struct stack {
	struct stack_cont *content;
	int size;
	int top;
};

/* stack function prototypes */

void init_stack(struct stack *); /* initialize the struct stack */

void push_valstack(void *, int, int, void (*)(void *), int, struct stack *); /* push value and type to the top of the struct stack */
void push_ststack(struct stack_cont, struct stack *); /* push struct to the top of the struct stack */

struct stack_cont *pop_stack(struct stack *); /* pops the top value on the struct stack */
struct stack_cont *top_stack(struct stack *); /* returns the top value on the struct stack */

void free_stack_cont(struct stack_cont *); /* frees and clears the stack content struct */
void free_stack(struct stack *); /* frees and clears the struct stack */

/* frees several stack structures in one function call*/
void ufree_stackm(struct stack **s, ...);
#define free_stackm(...) ufree_stackm(__VA_ARGS__, NULL)

#define stack_size(x) ((x)->top + 1)

#endif /* SYNGE_STACK_H */
