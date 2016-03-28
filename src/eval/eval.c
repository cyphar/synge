/* Synge-Eval: A scripting interface for Synge
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

/*
 * SYNPOSIS:
 *        ./synge-eval expression[s] [-m mode] [-RVh]
 *
 * DESCRIPION:
 *        Run the expression through Synge, using the given settings, and defaults otherwise.
 *
 * OPTIONS:
 *        -m <mode>, --mode <mode> 	Sets the mode to <mode> (radians || degrees || gradians)
 *        -R, --no-random		Make functions that depend on randomness predictable (FOR TESTING PURPOSES ONLY)
 *        -S, --no-skip			Do not skip "ignorable" error messages
 *
 *        -L, --license         Print license and warranty information
 *        -V, --version			Print version information
 *        -h, --help			Print help information
 */

#include <synge.h>
#include <definitions.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <time.h>
#include <unistd.h>

#define SYNGE_EVAL_HELP "./synge-eval expression[s] [-m mode] [-RVh]\n" \
"\n" \
"Run the expression through Synge, using the given settings, and defaults otherwise.\n" \
"\n" \
"  -m <mode>, --mode <mode>     Sets the mode to <mode> (radians || degrees || gradians)\n" \
"  -R, --no-random              Make functions that depend on randomness predictable (FOR TESTING PURPOSES ONLY)\n" \
"  -S, --no-skip                Do not skip 'ignorable' error messages\n" \
"\n" \
"  -L, --license                Print license and warranty information\n" \
"  -V, --version                Print version information\n" \
"  -h, --help                   Print help information"

#define SYNGE_EVAL_LICENSE "Synge-Eval: A scripting interface for Synge\n" SYNGE_LICENSE

struct synge_settings test_settings;

int skip_ignorable = 1;

void bake_args(int argc, char ***argv) {
	test_settings = synge_get_settings();

	/* TODO: consider using getopt (but need to fix "-(1+2)" problem...) */
	int i;
	for(i = 1; i < argc; i++) {
		if(argc > i + 1 && (!strcmp((*argv)[i], "-m") || !strcmp((*argv)[i], "-mode") || !strcmp((*argv)[i], "--mode"))) {
				i++;

				if(!strcasecmp((*argv)[i], "radians"))
					test_settings.mode = radians;
				else if(!strcasecmp((*argv)[i], "degrees"))
					test_settings.mode = degrees;
				else if(!strcasecmp((*argv)[i], "gradians"))
					test_settings.mode = gradians;

				(*argv)[i-1] = NULL;
				(*argv)[i] = NULL;
		}
		else if(!strcmp((*argv)[i], "-R") || !strcmp((*argv)[i], "-no-random") || !strcmp((*argv)[i], "--no-random")) {
			synge_seed(0); /* seed random number generator, to make it predicatable for testing */
			(*argv)[i] = NULL;
		}
		else if(!strcmp((*argv)[i], "-S") || !strcmp((*argv)[i], "-no-skip") || !strcmp((*argv)[i], "--no-skip")) {
			skip_ignorable = 0;
			(*argv)[i] = NULL;
		}
		else if(!strcmp((*argv)[i], "-L") || !strcmp((*argv)[i], "-license") || !strcmp((*argv)[i], "--license")) {
			puts(SYNGE_EVAL_LICENSE "\n");
			puts(SYNGE_WARRANTY);
			exit(0);
		}
		else if(!strcmp((*argv)[i], "-V") || !strcmp((*argv)[i], "-version") || !strcmp((*argv)[i], "--version")) {
			struct synge_ver core = synge_get_version();

			printf(	"Synge:       %s\n"
					"Synge-Eval:  %s\n"
#if defined(SYNGE_REVISION)
					"Revision:    " SYNGE_REVISION "\n"
#endif /* SYNGE_REVISION */
					"Compiled:    %s, %s\n", core.version, SYNGE_EVAL_VERSION, __TIME__, __DATE__);
			exit(0);
		}
		else if(!strcmp((*argv)[i], "-h") || !strcmp((*argv)[i], "-help") || !strcmp((*argv)[i], "--help")) {
			printf(SYNGE_EVAL_HELP "\n");
			exit(0);
		}
	}

	synge_set_settings(test_settings);
} /* bake_args() */

int main(int argc, char **argv) {
	if(argc < 2)
		return 1;

	synge_start();
	synge_seed(time(NULL) ^ getpid());

	bake_args(argc, &argv);

	synge_t result;
	mpfr_init2(result, SYNGE_PRECISION);

	struct synge_err ecode;

	int i;
	for(i = 1; i < argc; i++) {
		if(!argv[i])
			continue;

		ecode = synge_compute_string(argv[i], &result);

		if(skip_ignorable && synge_is_ignore_code(ecode.code))
			continue;

		if(ecode.code != SUCCESS)
			printf("%s\n", synge_error_msg(ecode));
		else
			synge_printf("%.*" SYNGE_FORMAT "\n", synge_get_precision(result), result);
	}

	mpfr_clears(result, NULL);
	synge_end();
	return 0;
} /* main() */
