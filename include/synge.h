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

#include <stdarg.h>
#include <stdio.h>
#include <mpfr.h>

#include "ohmic.h"

#ifndef SYNGE_H
#define SYNGE_H

#define SYNGE_FORMAT		"Rf"
#define SYNGE_PRECISION		1024
#define SYNGE_ROUND		GMP_RNDN

#define synge_printf(...)	mpfr_printf(__VA_ARGS__)
#define synge_fprintf(...)	mpfr_fprintf(__VA_ARGS__)
#define synge_sprintf(...)	mpfr_sprintf(__VA_ARGS__)
#define synge_snprintf(...)	mpfr_snprintf(__VA_ARGS__)
#define synge_vprintf(...)	mpfr_vprintf(__VA_ARGS__)
#define synge_vfprintf(...)	mpfr_vfprintf(__VA_ARGS__)
#define synge_vnprintf(...)	mpfr_vnprintf(__VA_ARGS__)

#if !defined(true) || !defined(false) || !defined(bool)
#	define bool int
#	define true 1
#	define false 0
#endif /* !bool */

/* define os */
#if defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
#	define _WINDOWS
#else
#	define _UNIX
#endif /* _WIN16 || _WIN32 || _WIN64 */

/* define symbol exports */
#if defined(_WINDOWS)
#	if defined(BUILD_LIB)
#		define __EXPORT __declspec(dllexport)
#	else
#		define __EXPORT __declspec(dllimport)
#	endif
#else
#	define __EXPORT
#endif /* _WINDOWS */

typedef mpfr_t synge_t;

struct synge_err {
	enum {
		SUCCESS,
		BASE_CHAR,
		DIVIDE_BY_ZERO,
		MODULO_BY_ZERO,
		UNMATCHED_LEFT_PARENTHESIS,
		UNMATCHED_RIGHT_PARENTHESIS,
		UNKNOWN_TOKEN,
		INVALID_LEFT_OPERAND,
		INVALID_RIGHT_OPERAND,
		INVALID_DELETE,
		FUNCTION_WRONG_ARGC,
		OPERATOR_WRONG_ARGC,
		MISSING_IF,
		MISSING_ELSE,
		EMPTY_IF,
		EMPTY_ELSE,
		EMPTY_BODY,
		TOO_MANY_VALUES,
		EMPTY_STACK,
		NUM_OVERFLOW,
		RESERVED_VARIABLE,
		UNKNOWN_WORD,
		ERROR_FUNC_ASSIGNMENT,
		ERROR_DELETE,
		UNDEFINED,
		TOO_DEEP,
		UNKNOWN_ERROR
	} code;
	int position;
};

enum {
	dynamic = -1
};

struct synge_settings {
	enum {
		degrees,
		radians,
		gradians
	} mode;

	enum {
		simple,
		position,
		traceback,
	} error;

	enum {
		flexible,
		strict
	} strict;

	int precision;
};

struct synge_func {
	/* hard-coded name and description strings */
	char *name;
	char *prototype;
	char *description;

	/* a function pointer with the same format as the mpfr_* functions */
	int (*get)();
};

struct synge_word {
	char *name;
	char *description;
};

__EXPORT int synge_get_precision(synge_t); /* returns minimum decimal precision needed to print number */

__EXPORT struct synge_settings synge_get_settings(void); /* returns active settings */
__EXPORT void synge_set_settings(struct synge_settings); /* set active settings to given settings */

__EXPORT struct synge_func *synge_get_function_list(void); /* returns list of available builtin functions */
__EXPORT struct ohm_t *synge_get_variable_list(void); /* returns list of variables */
__EXPORT struct ohm_t *synge_get_expression_list(void); /* returns list of user functions */
__EXPORT struct synge_word *synge_get_constant_list(void); /* returns list of builtin constants (must be freed) */

__EXPORT char *synge_error_msg(struct synge_err); /* returns a string which describes the error code (DO NOT FREE) */
__EXPORT char *synge_error_msg_pos(int, int); /* same as above, except takes the internal position and code values as args (DO NOT FREE) */

__EXPORT struct synge_err synge_compute_string(char *, synge_t *); /* takes an infix-style string and runs it through the synge core */

/* returns true if the return code should be treated as a success, otherwise false */
#define synge_is_success_code(code) \
	(code == SUCCESS)

/* returns true if the return code and result should be ignored, otherwise false */
#define synge_is_ignore_code(code) \
	(code == EMPTY_STACK || code == ERROR_FUNC_ASSIGNMENT || code == ERROR_DELETE)

__EXPORT void synge_seed(unsigned int seed); /* seed synge's pseudorandom number generator */
__EXPORT void synge_start(void); /* run at program initiation -- assertion will fail if not run before using synge functions */
__EXPORT void synge_end(void); /* run at program termination -- memory WILL leak if not run at end */

__EXPORT void synge_reset_traceback(void); /* reset the traceback list */

struct synge_ver {
	char *version; /* version in the form "maj.min.rev" */
	char *revision; /* git revision (empty if production version) */
	char *compiled; /* time and date of compilation */
};

__EXPORT struct synge_ver synge_get_version(void); /* get a structure containing the relevant version information of the synge core */

/* for windows, we need to define strcasecmp and strncasecmp */
#if defined(_WINDOWS)
typedef long off_t;
typedef long off64_t;

__EXPORT int strcasecmp(char *, char *);
__EXPORT int strncasecmp(char *, char *, size_t);
#endif

#endif /* SYNGE_H */
