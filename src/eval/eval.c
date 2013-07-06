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
 *        -S, --no-skip			Do not skip "ignorable" error messages
 *
 *        -V, --version			Print version information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <time.h>
#include <unistd.h>

#include <synge.h>
#include <definitions.h>

#ifndef __SYNGE_GIT_VERSION__
#	define __SYNGE_GIT_VERSION__ ""
#endif

synge_settings test_settings;

int skip_ignorable = 1;

void bake_args(int argc, char ***argv) {
	test_settings = synge_get_settings();

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
		else if(!strcmp((*argv)[i], "-S") || !strcmp((*argv)[i], "-no-skip") || !strcmp((*argv)[i], "--no-skip")) {
			skip_ignorable = 0;
			(*argv)[i] = NULL;
		}
		else if(!strcmp((*argv)[i], "-V") || !strcmp((*argv)[i], "-version") || !strcmp((*argv)[i], "--version")) {
			char *revision = "";
			if(strlen(__SYNGE_GIT_VERSION__) == 40)
				revision = "Revision:    " __SYNGE_GIT_VERSION__ "\n";

			printf(	"Synge:       %s\n"
				"Synge-Eval:  %s\n"
				"%s"
				"Compiled:    %s, %s\n", __SYNGE_VERSION__, __SYNGE_EVAL_VERSION__, revision, __TIME__, __DATE__);
			exit(0);
		}
	}

	synge_set_settings(test_settings);
} /* bake_args() */

int main(int argc, char **argv) {
	if(argc < 2) return 1;
	synge_start();
	srand(time(NULL) ^ getpid());
	bake_args(argc, &argv);

	synge_t result;
	mpfr_init2(result, SYNGE_PRECISION);

	error_code ecode;

	int i;
	for(i = 1; i < argc; i++) {
		if(!argv[i]) continue;
		ecode = synge_compute_string(argv[i], &result);

		if(skip_ignorable && synge_is_ignore_code(ecode.code)) continue;
		else if(ecode.code != SUCCESS)
			printf("%s\n", synge_error_msg(ecode));
		else
			synge_printf("%.*" SYNGE_FORMAT "\n", synge_get_precision(result), result);
	}

	mpfr_clears(result, NULL);
	synge_end();
	return 0;
} /* main() */
