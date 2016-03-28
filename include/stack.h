/* Synge: A shunting-yard calculation "engine"
 * Copyright (C) 2013, 2016 Aleksa Sarai
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
