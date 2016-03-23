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
extern struct ohm_t *variable_list;
extern struct ohm_t *expression_list;
extern synge_t prev_answer;

/* traceback */
extern char *error_msg_container;
extern struct link_t *traceback_list;

/* default settings */
extern struct synge_settings active_settings;

/* builtin lists */
extern struct synge_func func_list[];
extern struct synge_const constant_list[];
extern struct synge_op op_list[];

#endif /* GLOBAL_H */
