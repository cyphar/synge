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

typedef enum __errcode__ {
	SUCCESS,
	DIVIDE_BY_ZERO,
	UNMATCHED_PARENTHESIS,
	UNKNOWN_TOKEN,
	WRONG_NUM_VALUES,
	EMPTY_STACK
} error_code;

typedef struct __synge_settings__ {
	enum {
		degrees,
		radians
	} angle;
} synge_settings;

int get_precision(double); /* returns minimum decimal precision needed to print number */

synge_settings get_synge_settings(void); /* returns active settings */
void set_synge_settings(synge_settings); /* set active settings to given settings */

char *get_error_msg(error_code); /* returns a string which describes the error code (DO NOT FREE) */
error_code compute_infix_string(char *, double *); /* takes an infix-style string and runs it through the "engine" */
