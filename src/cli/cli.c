/* Synge-Cli: A command-line wrapper for Synge
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#ifndef __WIN32
#include <histedit.h> /* readline drop-in replacement */
#endif

#include <time.h>
#include <unistd.h>

#include <stack.h>
#include <synge.h>
#include <definitions.h>

#define length(x) (sizeof(x) / sizeof(x[0]))

#ifndef __WIN32

#define ANSI_ERROR	"\x1b[1;31m"
#define ANSI_GOOD	"\x1b[1;32m"
#define ANSI_INFO	"\x1b[1;37m"
#define ANSI_OUTPUT	"\x1b[1;37m"
#define ANSI_CLEAR	"\x1b[0;m"
#define OUTPUT_PADDING	""

#else
/* ANSI Escape Sequences don't work on Windows */
#define ANSI_ERROR	""
#define ANSI_GOOD	""
#define ANSI_INFO	""
#define ANSI_OUTPUT	""
#define ANSI_CLEAR	""
#define OUTPUT_PADDING	""

#endif

#ifndef __SYNGE_CLI_VERSION__
#define __SYNGE_CLI_VERSION__ ""
#endif

#define CLI_BANNER	"Synge-Cli " __SYNGE_CLI_VERSION__ "\n" \
			"Copyright (C) 2013 Cyphar\n" \
			"This free software is licensed under the terms of the MIT License with ABSOLUTELY NO WARRANTY\n" \
			"For more information, type 'version', 'license' and 'warranty'\n"

typedef struct __cli_command__ {
	char *name;
	void (*exec)();
	bool whole;
} cli_command;

void cli_version(void) {
	printf(	"\n%s"
		"Synge:       %s\n"
		"Synge-Cli:   %s\n"
		"\n"
		"Compiled:    %s, %s\n"
		"%s\n", ANSI_INFO, __SYNGE_VERSION__, __SYNGE_CLI_VERSION__, __TIME__, __DATE__, ANSI_CLEAR);
} /* cli_version() */

void cli_license(void) {
	printf("\n%s%s%s\n", ANSI_INFO, SYNGE_CLI_LICENSE, ANSI_CLEAR);
} /* cli_license() */

void cli_warranty(void) {
	printf("\n%s%s%s\n", ANSI_INFO, SYNGE_WARRANTY, ANSI_CLEAR);
} /* cli_warranty() */

void cli_banner(void) {
	printf("\n%s%s%s\n", ANSI_INFO, CLI_BANNER, ANSI_CLEAR);
} /* cli_banner() */

void cli_print_list(char *s) {
	char *args = strchr(s, ' ') + 1;
	int i;
	if(!strcmp(args, "functions")) {
		function *function_list = get_synge_function_list();
		printf("\n");
		for(i = 0; function_list[i].name != NULL; i++)
			printf("%s%*s -- %s%s\n", ANSI_INFO, 10, function_list[i].prototype, function_list[i].description, ANSI_CLEAR);
		printf("\n");
	}
	else printf("%s%s%s%s\n", OUTPUT_PADDING, ANSI_ERROR, get_error_msg_pos(UNKNOWN_TOKEN, -1), ANSI_CLEAR);
} /* cli_print_list() */

void cli_print_settings(char *s) {
	synge_settings current_settings = get_synge_settings();
	char *args = strchr(s, ' ') + 1, *ret = NULL;

	/* should be replaced with a struct lookup */
	if(!strcmp(args, "mode")) {
		switch(current_settings.mode) {
			case degrees:
				ret = "Degrees";
				break;
			case radians:
				ret = "Radians";
				break;
		}
	}
	else if(!strcmp(args, "error")) {
		switch(current_settings.error) {
			case simple:
				ret = "Simple";
				break;
			case position:
				ret = "Position";
				break;
		}
	}

	if(!ret)
		printf("%s%s%s%s\n", OUTPUT_PADDING, ANSI_ERROR, get_error_msg_pos(UNKNOWN_TOKEN, -1), ANSI_CLEAR);
	else printf("\n%s%s%s\n\n", ANSI_INFO, ret, ANSI_CLEAR);
} /* cli_print_settings() */

void cli_set_settings(char *s) {
	synge_settings new_settings = get_synge_settings();
	char *args = strchr(s, ' ') + 1;
	bool err = false;

	/* should be replaced with a struct lookup */
	if(!strncmp(args, "mode ", strlen("mode "))) {
		char *val = strchr(args, ' ') + 1;
		if(!strcasecmp(val, "degrees"))
			new_settings.mode = degrees;
		else if(!strcasecmp(val, "radians"))
			new_settings.mode = radians;
		else err = true;
	}
	else if(!strncmp(args, "error ", strlen("error "))) {
		char *val = strchr(args, ' ') + 1;
		if(!strcasecmp(val, "simple"))
			new_settings.error = simple;
		else if(!strcasecmp(val, "position"))
			new_settings.error = position;
		else err = true;
	}
	else err = true;

	if(err)
		printf("%s%s%s%s\n", OUTPUT_PADDING, ANSI_ERROR, get_error_msg_pos(UNKNOWN_TOKEN, -1), ANSI_CLEAR);
	else set_synge_settings(new_settings);
} /* cli_set_settings() */

cli_command cli_command_list[] = {
	{"exit",	NULL,			true},
	{"exit()",	NULL,			true},
	{"quit",	NULL,			true},
	{"quit()",	NULL,			true},

	{"version",	cli_version,		true},
	{"license",	cli_license,		true},
	{"warranty",	cli_warranty,		true},
	{"banner",	cli_banner,		true},

	{"list ",	cli_print_list,		false},
	{"set ",	cli_set_settings,	false},
	{"get ",	cli_print_settings,	false},
};

bool cli_is_command(char *s) {
	int i;
	for(i = 0; i < length(cli_command_list); i++) {
		if(cli_command_list[i].whole && !strcmp(cli_command_list[i].name, s)) return true;
		else if(!cli_command_list[i].whole && !strncmp(cli_command_list[i].name, s, strlen(cli_command_list[i].name))) return true;
	}
	return false;
} /* cli_is_command() */

cli_command cli_get_command(char *s) {
	int i;
	for(i = 0; i < length(cli_command_list); i++) {
		if(cli_command_list[i].whole && !strcmp(cli_command_list[i].name, s)) return cli_command_list[i];
		else if(!cli_command_list[i].whole && !strncmp(cli_command_list[i].name, s, strlen(cli_command_list[i].name))) return cli_command_list[i];
	}
	cli_command empty = {NULL, NULL};
	return empty;
} /* cli_get_command() */

void sfree(char **pp) {
	if(pp && *pp) {
		free(*pp);
		pp = NULL;
	}
} /* sfree() */

char *cli_get_prompt(void *e) {
	return ">>> ";
} /* cli_get_prompt() */

char *fallback_prompt(int *count) {
	printf("%s", cli_get_prompt(NULL));
	fflush(stdout);

	char *input = NULL, ch;
	int len = 0;

	while((ch = getchar()) != '\n') {
		input = realloc(input, ++len);
		input[len - 1] = ch;
	}

	input = realloc(input, ++len);
	input[len - 1] = '\0';

	*count = len;
	return input;
} /* fallback_prompt() */

int main(int argc, char **argv) {
	synge_start();
	srand(time(NULL) ^ getpid());

	char *cur_str = NULL;
	int count;
	double result = 0;
	error_code ecode;
#ifndef __WIN32
	/* Local stuff for libedit */
	EditLine *cli_el;
	History *cli_history;
	HistEvent cli_ev;

	cli_el = el_init(argv[0], stdin, stdout, stderr);
	el_set(cli_el, EL_PROMPT, &cli_get_prompt);
	el_set(cli_el, EL_EDITOR, "emacs");

	/* Initialize the history */
	cli_history = history_init();
	history(cli_history, &cli_ev, H_SETSIZE, 800);
	el_set(cli_el, EL_HIST, history, cli_history);
#endif
	/* print banner (cli_banner has a leading newline)*/
	printf("%s%s%s\n", ANSI_INFO, CLI_BANNER, ANSI_CLEAR);

	while(true) {
#ifndef __WIN32
		cur_str = (char *) el_gets(cli_el, &count); /* get input */
		if(strchr(cur_str, '\n')) *strchr(cur_str, '\n') = '\0';
#else
		cur_str = fallback_prompt(&count);
#endif

		if(cur_str && strlen(cur_str) && count) {
			if(cli_is_command(cur_str)) {
				cli_command tmp = cli_get_command(cur_str);
				if(!tmp.exec) break; /* command is to exit */
				tmp.exec(cur_str);
			}
			else if((ecode = compute_infix_string(cur_str, &result)).code != SUCCESS) {
				if(ecode.code == EMPTY_STACK) continue;
				else printf("%s%s%s%s\n", OUTPUT_PADDING, is_success_code(ecode.code) ? ANSI_GOOD : ANSI_ERROR, get_error_msg(ecode), ANSI_CLEAR);
			}
			else printf("%s%.*f%s\n", ANSI_OUTPUT, get_precision(result), result, ANSI_CLEAR);
#ifndef __WIN32
			history(cli_history, &cli_ev, H_ENTER, cur_str); /* add input to history */
#endif
		}
#ifdef __WIN32
		free(cur_str);
#endif
	}

#ifndef __WIN32
	/* free up memory */
	history_end(cli_history);
	el_end(cli_el);
#else
	free(cur_str);
#endif
	synge_end();
	return 0;
}
