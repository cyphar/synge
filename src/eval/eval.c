/* Synge-Eval: A simple evaluation wrapper for Synge
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
 * cc <source files> src/eval/eval.c -lm -Isrc/ -std=c99
 *
 * Command-line Arguments:
 *
 * SYNPOSIS:
 *        ./synge-eval expression[s] [-m mode] [=R]
 *
 * DESCRIPION:
 *        Run the expression through Synge, using the given settings, and defaults otherwise.
 *
 * OPTIONS:
 *        -m <mode>, --mode <mode> 	Sets the mode to <mode> (radians || degrees)
 *        -R, --no-random		Make functions that depend on randomness predictable (FOR TESTING PURPOSES ONLY)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <time.h>
#include <unistd.h>

#include <synge.h>

synge_settings test_settings;

void bake_args(int argc, char ***argv) {
	test_settings = get_synge_settings();

	int i;
	for(i = 1; i < argc; i++) {
		if(!strcmp((*argv)[i], "-m") || !strcmp((*argv)[i], "-mode") || !strcmp((*argv)[i], "--mode")) {
				i++;
				if(!strcasecmp((*argv)[i], "radians")) test_settings.mode = radians;
				else if(!strcasecmp((*argv)[i], "degrees")) test_settings.mode = degrees;
				(*argv)[i-1] = NULL;
				(*argv)[i] = NULL;
		}
		else if(!strcmp((*argv)[i], "-R") || !strcmp((*argv)[i], "-no-random") || !strcmp((*argv)[i], "--no-random")) {
			srand(0); /* seed random number generator, to make it predicatable for testing */
			(*argv)[i] = NULL;
		}
	}

	set_synge_settings(test_settings);
} /* bake_args() */

int main(int argc, char **argv) {
	if(argc < 2) return 1;
	synge_start();
	srand(time(NULL) ^ getpid());
	bake_args(argc, &argv);

	double result;
	error_code ecode;

	int i;
	for(i = 1; i < argc; i++) {
		if(!argv[i]) continue;

		if((ecode = compute_infix_string(argv[i], &result)).code != SUCCESS)
			printf("%s\n", get_error_msg(ecode));
		else
			printf("%.*f\n", get_precision(result), result);
	}

	synge_end();
	return 0;
} /* main() */
