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

#include "synge.h"
#include "common.h"
#include "stack.h"
#include "ohmic.h"
#include "linked.h"

#ifndef GLOBAL_H
#define GLOBAL_H

/* global state variables */
extern int synge_started;
extern gmp_randstate_t synge_state;

/* variables and functions */
extern ohm_t *variable_list;
extern ohm_t *expression_list;
extern synge_t prev_answer;

/* traceback */
extern char *error_msg_container;
extern link_t *traceback_list;

/* default settings */
extern synge_settings active_settings;

/* builtin lists */
extern function func_list[];
extern special_number constant_list[];
extern operator op_list[];

#endif /* GLOBAL_H */
