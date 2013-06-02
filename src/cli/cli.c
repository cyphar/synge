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

#define _BSD_SOURCE

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
#include <errno.h>

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
#define ERR_TO_OUT	" 2>&1"

#else
/* ANSI Escape Sequences don't work on Windows */
#define ANSI_ERROR	""
#define ANSI_GOOD	""
#define ANSI_INFO	""
#define ANSI_OUTPUT	""
#define ANSI_CLEAR	""
#define ERR_TO_OUT	" 2>&1"

#endif

#define OUTPUT_PADDING	""

#ifndef BLOCKSIZE
#define BLOCKSIZE 1024
#endif

#ifndef __SYNGE_CLI_VERSION__
#define __SYNGE_CLI_VERSION__ ""
#endif

#define CLI_BANNER	"Synge-Cli " __SYNGE_CLI_VERSION__ "\n" \
			"Copyright (C) 2013 Cyphar\n" \
			"This free software is licensed under the terms of the MIT License and is provided with ABSOLUTELY NO WARRANTY\n" \
			"For more information, type 'version', 'license' and 'warranty'\n"

typedef struct __cli_command__ {
	char *name;
	void (*exec)();
	bool whole;
} cli_command;

void cli_version(void) {
	printf(	"%s"
		"Synge:       %s\n"
		"Synge-Cli:   %s\n"
		"Compiled:    %s, %s\n"
		"%s", ANSI_INFO, __SYNGE_VERSION__, __SYNGE_CLI_VERSION__, __TIME__, __DATE__, ANSI_CLEAR);
} /* cli_version() */

void cli_license(void) {
	printf("%s%s%s", ANSI_INFO, SYNGE_CLI_LICENSE, ANSI_CLEAR);
} /* cli_license() */

void cli_warranty(void) {
	printf("%s%s%s", ANSI_INFO, SYNGE_WARRANTY, ANSI_CLEAR);
} /* cli_warranty() */

void cli_banner(void) {
	printf("%s%s%s", ANSI_INFO, CLI_BANNER, ANSI_CLEAR);
} /* cli_banner() */

void cli_print_list(char *s) {
	char *args = strchr(s, ' ') + 1;
	int i;
	if(!strcmp(args, "functions")) {
		function *function_list = get_synge_function_list();
		for(i = 0; function_list[i].name != NULL; i++)
			printf("%s%*s - %s%s\n", ANSI_INFO, 10, function_list[i].prototype, function_list[i].description, ANSI_CLEAR);
	}
	else printf("%s%s%s%s\n", OUTPUT_PADDING, ANSI_ERROR, get_error_msg_pos(UNKNOWN_TOKEN, -1), ANSI_CLEAR);
} /* cli_print_list() */

#define intlen(number) (int) (number ? floor(log10(abs(number))) + 1 : 1) /* magical math hack to get number of digits in an integer */

char *itoa(int i) {
	char *buf = malloc(intlen(i) + (i < 0 ? 2 : 1)); /* this makes buf exactly the size it needs to be */
	char *p = buf + intlen(i) + (i < 0 ? 2 : 1); /* point *p to the end of buf */
	*--p = '\0'; /* null terminate string */

	/* generate the string backwards */
	long j = 1;
	do {
		*--p = '0' + (abs(i / j) % 10); /* '0' + a one-digit number = the ascii val of the number */
		j *= 10;
	} while (i / j);

	if(i < 0) *--p = '-'; /* if number is negative, add a negative sign */
	return buf; /* must be freed! */
} /* itoa() */

void cli_print_settings(char *s) {
	synge_settings current_settings = get_synge_settings();
	char *args = strchr(s, ' ') + 1, *ret = NULL, *tmpfree = NULL;

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
			case traceback:
				ret = "Traceback";
				break;
		}
	}
	else if(!strcmp(args, "strict")) {
		switch(current_settings.strict) {
			case flexible:
				ret = "Flexible";
				break;
			case strict:
				ret = "Strict";
				break;
		}
	}
	else if(!strcmp(args, "precision")) {
		if(current_settings.precision >= 0)
			tmpfree = ret = itoa(current_settings.precision);
		else
			ret = "Dynamic";
	}

	if(!ret)
		printf("%s%s%s%s\n", OUTPUT_PADDING, ANSI_ERROR, get_error_msg_pos(UNKNOWN_TOKEN, -1), ANSI_CLEAR);
	else printf("%s%s%s\n", ANSI_INFO, ret, ANSI_CLEAR);

	free(tmpfree);
} /* cli_print_settings() */

void cli_set_settings(char *s) {
	synge_settings new_settings = get_synge_settings();
	char *args = strchr(s, ' ') + 1;
	char *val = strchr(args, ' ') + 1;
	bool err = false;

	/* should be replaced with a struct lookup */
	if(!strncmp(args, "mode ", strlen("mode "))) {
		if(!strcasecmp(val, "degrees"))
			new_settings.mode = degrees;
		else if(!strcasecmp(val, "radians"))
			new_settings.mode = radians;
		else err = true;
	}
	else if(!strncmp(args, "error ", strlen("error "))) {
		if(!strcasecmp(val, "simple"))
			new_settings.error = simple;
		else if(!strcasecmp(val, "position"))
			new_settings.error = position;
		else if(!strcasecmp(val, "traceback"))
			new_settings.error = traceback;
		else err = true;
	}
	else if(!strncmp(args, "strict ", strlen("strict "))) {
		if(!strcasecmp(val, "flexible"))
			new_settings.strict = flexible;
		else if(!strcasecmp(val, "strict"))
			new_settings.strict = strict;
		else err = true;
	}
	else if(!strncmp(args, "precision ", strlen("precision "))) {
		errno = 0;

		if(!strcasecmp(val, "dynamic"))
			new_settings.precision = -1;
		else
			new_settings.precision = strtol(val, NULL, 10);

		if(errno)
			err = true;
	}
	else err = true;

	if(err)
		printf("%s%s%s%s\n", OUTPUT_PADDING, ANSI_ERROR, get_error_msg_pos(UNKNOWN_TOKEN, -1), ANSI_CLEAR);
	else {
		set_synge_settings(new_settings);

		char *tmp = malloc(strlen(s));
		strcpy(tmp, s);

		tmp[0] = 'g';
		*strrchr(tmp, ' ') = '\0';

		cli_print_settings(tmp);
		free(tmp);
	}
} /* cli_set_settings() */

void cli_exec(char *str) {
	char *command = malloc(strlen(str + 1) + strlen(ERR_TO_OUT) + 1);
	strcpy(command, str + 1);
	strcat(command, ERR_TO_OUT);

	FILE *outf = popen(command, "r");
	if(!outf) {
		printf("%s%s%s%s\n", OUTPUT_PADDING, ANSI_ERROR, strerror(errno), ANSI_CLEAR);
		return;
	}

	char *output = malloc(1), *buf = malloc(BLOCKSIZE + 1);
	*output = '\0';

	while(fgets(buf, BLOCKSIZE, outf) != NULL) {
		output = realloc(output, strlen(output) + strlen(buf) + 1);
		strcat(output, buf);
	}

	if(output && output[strlen(output) - 1] == '\n')
		output[strlen(output) - 1] = '\0';

	int ret = fclose(outf);
	printf("%s%s%s\n", !ret ? ANSI_INFO : ANSI_ERROR, output, ANSI_CLEAR);

	fflush(stdout);
	fflush(stderr);

	free(buf);
	free(output);
	free(command);
} /* cli_exec() */

cli_command cli_command_list[] = {
	{"exit",	NULL,			true},
	{"exit()",	NULL,			true},
	{"quit",	NULL,			true},
	{"quit()",	NULL,			true},

	{"version",	cli_version,		true},
	{"license",	cli_license,		true},
	{"warranty",	cli_warranty,		true},
	{"banner",	cli_banner,		true},

	{"!",		cli_exec,		false},

	{"list ",	cli_print_list,		false},
	{"set ",	cli_set_settings,	false},
	{"get ",	cli_print_settings,	false},
};

bool cli_is_command(char *s) {
	int i;
	for(i = 0; i < (int) length(cli_command_list); i++) {
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
	cli_command empty = {NULL, NULL, false};
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

/* for some reason, this code (when run on Windows) has
 * history and line editing automatically added */
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
