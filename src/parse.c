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
#include <math.h>
#include <ctype.h>
#include <string.h>

#include "synge.h"
#include "version.h"
#include "common.h"
#include "global.h"
#include "stack.h"

static bool op_precedes(int op1, int op2) {
	/* returns whether or not op2 should precede op1 */
	switch(op2) {
		case ifop:
		case elseop:
		case bitop:
		case compop:
		case addop:
		case multop:
			/* left associated operator */
			return op1 >= op2;

		case delop:
		case signop:
		case setop:
		case modop:
		case expop:
		case preop:
			/* right associated operator */
			return op1 > op2;

		default:
			/* what the hell are you doing? */
			return false;
	}
} /* op_precedes() */

/* my implementation of Dijkstra's really cool shunting-yard algorithm */
struct synge_err synge_infix_parse(struct stack **infix_stack, struct stack **rpn_stack) {
	struct stack *op_stack = malloc(sizeof(struct stack));

	_debug("--\nParser\n--\n");

	init_stack(op_stack);
	init_stack(*rpn_stack);

	int i, size = stack_size(*infix_stack);

	/* reorder stack, in reverse (since we are poping from a full stack and pushing to an empty one) */
	for(i = 0; i < size; i++) {

		struct stack_cont stackp = (*infix_stack)->content[i]; /* pointer to current stack item (to make the code more readable) */
		int pos = stackp.position; /* shorthand for the position of errors */

		switch(stackp.tp) {
			case number:
			case constant:
				/* nothing to do, just push it onto the temporary stack */
				push_valstack(num_dup(SYNGE_T(stackp.val)), stackp.tp, true, synge_clear, pos, *rpn_stack);
				break;
			case expression:
			case userword:
				/* do nothing, just push it onto the stack */
				push_valstack(str_dup(stackp.val), stackp.tp, true, NULL, pos, *rpn_stack);
				break;
			case lparen:
			case func:
				/* again, nothing to do, push it onto the stack */
				push_ststack(stackp, op_stack);
				break;
			case rparen:
				{
					/* keep popping and pushing until you find an lparen, which isn't to be pushed  */
					int found = false;
					while(stack_size(op_stack)) {
						struct stack_cont *tmpstackp = pop_stack(op_stack);
						if(tmpstackp->tp == lparen) {
							found = true;
							break;
						}
						push_ststack(*tmpstackp, *rpn_stack); /* push it onto the stack */
					}

					/* push function pointer to ouput (if there is one) */
					if(top_stack(op_stack) && top_stack(op_stack)->tp == func)
						push_ststack(*pop_stack(op_stack), *rpn_stack);

					/* if no lparen was found, this is an unmatched right bracket*/
					if(!found) {
						free_stackm(infix_stack, &op_stack, rpn_stack);
						return to_error_code(UNMATCHED_RIGHT_PARENTHESIS, pos);
					}
				}
				break;
			case premod:
			case delop:
				{
					i++;
					struct stack_cont *tmpstackp = &(*infix_stack)->content[i];

					/* ensure that you are pushing a setword */
					if(i >= size || !isword(tmpstackp->tp)) {
						free_stackm(infix_stack, &op_stack, rpn_stack);
						return to_error_code(INVALID_LEFT_OPERAND, pos);
					}

					push_valstack(str_dup(tmpstackp->val), setword, true, NULL, tmpstackp->position, *rpn_stack);
					push_ststack(stackp, *rpn_stack);
				}
				break;
			case postmod:
				{
					if(top_stack(*rpn_stack) && top_stack(*rpn_stack)->tp == userword) {
						struct stack_cont *pop = pop_stack(*rpn_stack);
						push_valstack(pop->val, setword, true, NULL, pop->position, *rpn_stack);
					}

					push_ststack(stackp, *rpn_stack);
				}
				break;
			case setop:
			case modop:
				{
					if(top_stack(*rpn_stack) && top_stack(*rpn_stack)->tp == userword) {
						struct stack_cont *pop = pop_stack(*rpn_stack);
						push_valstack(pop->val, setword, true, NULL, pop->position, *rpn_stack);
					}
				}
				/* pass-through */
			case preop:
			case elseop:
			case ifop:
			case bitop:
			case compop:
			case signop:
			case addop:
			case multop:
			case expop:
				/* reorder operators to be in the correct order of precedence */
				while(stack_size(op_stack)) {
					struct stack_cont *tmpstackp = top_stack(op_stack);

					/* if the temporary item is an operator that precedes the current item, pop and push the temporary item onto the stack */
					if(isop(tmpstackp->tp) && op_precedes(tmpstackp->tp, stackp.tp))
						push_ststack(*pop_stack(op_stack), *rpn_stack);
					else
						break;
				}
				push_ststack(stackp, op_stack); /* finally, push the current item onto the stack */
				break;
			default:
				/* catchall -- unknown token */
				free_stackm(infix_stack, &op_stack, rpn_stack);
				return to_error_code(UNKNOWN_TOKEN, pos);
				break;
		}
	}

	/* re-reverse the stack again (so it's in the correct order) */
	while(stack_size(op_stack)) {
		struct stack_cont stackp = *pop_stack(op_stack);
		int pos = stackp.position;

		if(stackp.tp == lparen ||
		   stackp.tp == rparen) {
			/* if there is a left or right bracket, there is an unmatched left bracket */
			if(active_settings.strict >= strict) {
				free_stackm(infix_stack, &op_stack, rpn_stack);
				return to_error_code(UNMATCHED_LEFT_PARENTHESIS, pos);
			}
			else continue;
		}

		push_ststack(stackp, *rpn_stack);
	}

	/* debugging */
	print_stack(*rpn_stack);

	free_stackm(infix_stack, &op_stack);
	return to_error_code(SUCCESS, -1);
} /* synge_infix_parse() */
