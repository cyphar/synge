/* Synge-Test: A testing wrapper for Synge
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

/*
 * To Compile:
 * cc <source files> test/test.c -lm -Isrc/ -std=c99 -fsigned-char
 *
 * Command-line Arguments:
 *
 * SYNPOSIS:
 *        ./test expression [-m mode]
 *
 * DESCRIPION:
 *        Run the expression through Synge, using the given settings, and defaults otherwise.
 *
 * OPTIONS:
 *        -m <mode>, --mode <mode> 	Sets the mode to <mode> (radians, degrees)
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include <synge.h>

synge_settings test_settings;

char *bake_args(int argc, char **argv) {
	int i;
	for(i = 1; i < argc; i++) {
		if(!strcmp(argv[i], "-m") || !strcmp(argv[i], "-mode") || !strcmp(argv[i], "--mode")) {
				i++;
				if(!strcasecmp(argv[i], "radians")) test_settings.mode = radians;
				else if(!strcasecmp(argv[i], "degrees")) test_settings.mode = degrees;
				argv[i-1] = NULL;
				argv[i] = NULL;
		}
	}

	for(i = 1; i < argc; i++)
		if(argv[i]) return argv[i];
	return NULL;
} /* bake_args() */

int main(int argc, char **argv) {
	if(argc < 2) return 1;
	synge_start();
	srand(0); /* seed random number generator, to make it predicatable for testing */

	double result;
	error_code ecode;

	test_settings = get_synge_settings();
	char *expression = bake_args(argc, argv);
	set_synge_settings(test_settings);

	if(!expression) return 1;
	if((ecode = compute_infix_string(expression, &result)).code != SUCCESS)
		printf("%s\n", get_error_msg(ecode));
	else
		printf("%.*f\n", get_precision(result), result);

	synge_end();
	return 0;
} /* main() */
