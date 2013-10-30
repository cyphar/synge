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
#include "version.h"
#include "global.h"
#include "common.h"
#include "stack.h"
#include "ohmic.h"
#include "linked.h"

/* use theta symbol for angles if appropriate */
#if defined(SYNGE_UTF8_STRINGS)
#	define SYNGE_THETA "θ"
#else
#	define SYNGE_THETA "x"
#endif

/* global state variables */
int synge_started = false;
gmp_randstate_t synge_state;

/* variables and functions */
ohm_t *variable_list = NULL;
ohm_t *expression_list = NULL;
synge_t prev_answer;

/* traceback */
char *error_msg_container = NULL;
struct link_t *traceback_list = NULL;

/* default settings */
synge_settings active_settings = {
	.mode = degrees,
	.error = position,
	.strict = strict,
	.precision = dynamic
};

static int synge_rand(synge_t to, synge_t number, mpfr_rnd_t round) {
	/* A = rand() -- 0 <= rand() < 1 */
	synge_t random;
	mpfr_init2(random, SYNGE_PRECISION);
	mpfr_urandomb(random, synge_state);

	/* rand(B) = rand() * B -- where 0 <= rand() < 1 */
	mpfr_mul(to, random, number, round);

	/* free memory */
	mpfr_clears(random, NULL);
	return 0;
} /* synge_rand() */

static int synge_int_rand(synge_t to, synge_t number, mpfr_rnd_t round) {
	/* round input */
	mpfr_floor(number, number);

	/* get random number */
	synge_rand(to, number, round);

	/* round output */
	mpfr_round(to, to);
	return 0;
} /* synge_int_rand() */

static int synge_factorial(synge_t to, synge_t num, mpfr_rnd_t round) {
	/* round input */
	synge_t number;
	mpfr_init2(number, SYNGE_PRECISION);
	mpfr_abs(number, num, round);
	mpfr_floor(number, number);

	/* multilply original number by reverse iterator */
	mpfr_set_si(to, 1, round);
	while(!iszero(number)) {
		mpfr_mul(to, to, number, round);
		mpfr_sub_si(number, number, 1, round);
	}

	mpfr_copysign(to, to, num, round);
	mpfr_clears(number, NULL);
	return 0;
} /* synge_factorial() */

static int synge_sum(synge_t to, synge_t number, mpfr_rnd_t round) {
	/* round input */
	mpfr_floor(number, number);

	/* (x * (x + 1)) / 2 */
	mpfr_set(to, number, round);
	mpfr_add_si(number, number, 1, round);
	mpfr_mul(to, to, number, round);
	mpfr_div_si(to, to, 2, round);
	return 0;
} /* synge_sum() */

static int synge_bool(synge_t to, synge_t check, mpfr_rnd_t round) {
	return mpfr_set_si(to, iszero(check) ? 0 : 1, round);
} /* synge_bool() */

/* builtin function names, prototypes, descriptions and function pointers */
function func_list[] = {
	{"abs",		"abs(n)",						"Absolute value of n",								mpfr_abs},
	{"sqrt",	"sqrt(n)",						"Square root of n",									mpfr_sqrt},
	{"cbrt",	"cbrt(n)",						"Cubic root of n",									mpfr_cbrt},

	{"round",	"round(n)",						"Round n away from 0",								mpfr_round},
	{"ceil",	"ceil(n)",						"Round n toward positive infinity",					mpfr_ceil},
	{"floor",	"floor(n)",						"Round n toward negative infinity",					mpfr_floor},

	{"log",		"log(n)",						"Base 2 logarithm of n",							mpfr_log2},
	{"ln",		"ln(n)",						"Natural logarithm of n",							mpfr_log},
	{"log10",	"log10(n)",						"Base 10 logarithm of n",							mpfr_log10},

	{"rand",	"rand(n)",						"Generate a random number between 0 and n",			synge_rand},
	{"randi",	"randi(n)",						"Generate a random integer between 0 and n",		synge_int_rand},

	{"fact",	"fact(n)",						"Factorial of the integer n",						synge_factorial},
	{"sum",		"sum(n)",						"Gives sum of all integers up to n",				synge_sum},
	{"bool",	"bool(n)",						"Returns 0 if x is falseish, 1 otherwise",			synge_bool},

	{"deg2rad",	"deg2rad(" SYNGE_THETA ")",		"Convert " SYNGE_THETA " degrees to radians",		deg_to_rad},
	{"deg2grad","deg2grad(" SYNGE_THETA ")",	"Convert " SYNGE_THETA " degrees to gradians",		deg_to_grad},

	{"rad2deg",	"rad2deg(" SYNGE_THETA ")",		"Convert " SYNGE_THETA " radians to degrees",		rad_to_deg},
	{"rad2grad","rad2grad(" SYNGE_THETA ")",	"Convert " SYNGE_THETA " radians to gradians",		rad_to_grad},

	{"grad2deg","grad2deg(" SYNGE_THETA ")",	"Convert " SYNGE_THETA " gradians to degrees",		grad_to_deg},
	{"grad2rad","grad2rad(" SYNGE_THETA ")",	"Convert " SYNGE_THETA " gradians to radians",		grad_to_rad},

	{"sinh",	"sinh(" SYNGE_THETA ")",		"Hyperbolic sine of " SYNGE_THETA "",				mpfr_sinh},
	{"cosh",	"cosh(" SYNGE_THETA ")",		"Hyperbolic cosine of " SYNGE_THETA "",				mpfr_cosh},
	{"tanh",	"tanh(" SYNGE_THETA ")",		"Hyperbolic tangent of " SYNGE_THETA "",			mpfr_tanh},
	{"asinh",	"asinh(" SYNGE_THETA ")",		"Inverse hyperbolic sine of " SYNGE_THETA "",		mpfr_asinh},
	{"acosh",	"acosh(" SYNGE_THETA ")",		"Inverse hyperbolic cosine of " SYNGE_THETA "",		mpfr_acosh},
	{"atanh",	"atanh(" SYNGE_THETA ")",		"Inverse hyperbolic tangent of " SYNGE_THETA "",	mpfr_atanh},

	{"sin",		"sin(" SYNGE_THETA ")",			"Sine of " SYNGE_THETA "",							mpfr_sin},
	{"cos",		"cos(" SYNGE_THETA ")",			"Cosine of " SYNGE_THETA "",						mpfr_cos},
	{"tan",		"tan(" SYNGE_THETA ")",			"Tangent of " SYNGE_THETA "",						mpfr_tan},
	{"asin",	"asin(" SYNGE_THETA ")",		"Inverse sine of " SYNGE_THETA "",					mpfr_asin},
	{"acos",	"acos(" SYNGE_THETA ")",		"Inverse cosine of " SYNGE_THETA "",				mpfr_acos},
	{"atan",	"atan(" SYNGE_THETA ")",		"Inverse tangent of " SYNGE_THETA "",				mpfr_atan},
	{NULL,		NULL,			NULL,												NULL}
};

/* used for when a (char *) is needed, but needn't be freed and *
 * converts the string into switch-friendly enumeration values. */
operator op_list[] = {
	{"+",	op_add},
	{"-",	op_subtract},
	{"*",	op_multiply},
	{"/",	op_divide},
	{"//",	op_int_divide}, /* integer division */
	{"%",	op_modulo},
	{"^",	op_index},

	{"(",	op_lparen},
	{")",	op_rparen},

	/* comparison operators */
	{">",	op_gt},
	{">=",	op_gteq},

	{"<",	op_lt},
	{"<=",	op_lteq},

	{"!=",	op_neq},
	{"==",	op_eq},

	/* bitwise operators */
	{"&",	op_band},
	{"|",	op_bor},
	{"#",	op_bxor},

	{"~",	op_binv},
	{"!",	op_bnot},

	{"<<",	op_bshiftl},
	{">>",	op_bshiftr},

	/* tertiary operators */
	{"?",	op_if},
	{":",	op_else},

	/* assignment operators */
	{"=",	op_var_set},
	{":=",	op_func_set},
	{"::",	op_del},

	/* compound assignment operators */
	{"+=",	op_ca_add},
	{"-=",	op_ca_subtract},
	{"*=",	op_ca_multiply},
	{"/=",	op_ca_divide},
	{"//=",	op_ca_int_divide}, /* integer division */
	{"%=",	op_ca_modulo},
	{"^=",	op_ca_index},
	{"&=",	op_ca_band},
	{"|=",	op_ca_bor},
	{"#=",	op_ca_bxor},
	{"<<=",	op_ca_bshiftl},
	{">>=",	op_ca_bshiftr},

	/* prefix/postfix compound assignment operators */
	{"++",	op_ca_increment},
	{"--",	op_ca_decrement},

	/* null terminator */
	{NULL,	op_none}
};

static int synge_pi(synge_t num, mpfr_rnd_t round) {
	mpfr_const_pi(num, round);
	return 0;
} /* synge_pi() */

static int synge_phi(synge_t num, mpfr_rnd_t round) {
	/* get sqrt(5) */
	synge_t root_five;
	mpfr_init2(root_five, SYNGE_PRECISION);
	mpfr_sqrt_ui(root_five, 5, round);

	/* (1 + sqrt(5)) / 2 */
	mpfr_add_si(num, root_five, 1, round);
	mpfr_div_si(num, num, 2, round);

	mpfr_clears(root_five, NULL);
	return 0;
} /* synge_phi() */

static int synge_euler(synge_t num, mpfr_rnd_t round) {
	/* get one */
	synge_t one;
	mpfr_init2(one, SYNGE_PRECISION);
	mpfr_set_si(one, 1, round);

	/* e^1 */
	mpfr_exp(num, one, round);

	mpfr_clears(one, NULL);
	return 0;
} /* synge_euler() */

static int synge_life(synge_t num, mpfr_rnd_t round) {
	cheeky("How many paths must a man walk down?\n");
	mpfr_set_si(num, 42, round);
	return 0;
} /* synge_life() */

static int synge_true(synge_t num, mpfr_rnd_t round) {
	mpfr_set_si(num, 1, round);
	return 0;
} /* synge_true() */

static int synge_false(synge_t num, mpfr_rnd_t round) {
	mpfr_set_si(num, 0, round);
	return 0;
} /* synge_false() */

static int synge_ans(synge_t num, mpfr_rnd_t round) {
	mpfr_set(num, prev_answer, round);
	return 0;
} /* synge_ans() */

/* inbuilt constants (given using function pointers) */
special_number constant_list[] = {
	{"pi",				"The ratio of a circle's circumfrence to its diameter",				synge_pi},
	{"phi",				"The golden ratio",													synge_phi},
	{"e",				"The base of a natural logarithm",									synge_euler},
	{"life",			"The answer to the question of life, the universe and everything",	synge_life}, /* Sorry, I couldn't resist */

	{"true",			"Standard true value",												synge_true},
	{"false",			"Standard false value",												synge_false},

	{SYNGE_PREV_ANSWER,	"Gives the last successful answer",									synge_ans},
	{NULL,				NULL,																NULL},
};
