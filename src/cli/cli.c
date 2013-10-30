/* Synge-CLI: A command-line interface for Synge
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
#define _XOPEN_SOURCE

#include <synge.h>
#include <definitions.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

/* My self-contained readline alternative. *nix-only (for now)
 * You can get the code at https://github.com/cyphar/rawline. */

#if defined(_UNIX)
#	include "rawline.h"
#endif

#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>

#define length(x) (sizeof(x) / sizeof(x[0]))

#if defined(SYNGE_COLOUR) || defined(_UNIX)
#	define ANSI_ERROR	"\x1b[1;31m"
#	define ANSI_GOOD	"\x1b[1;32m"
#	define ANSI_INFO	"\x1b[1;37m"
#	define ANSI_OUTPUT	"\x1b[1;37m"
#	define ANSI_CLEAR	"\x1b[0;m"
#else
#	define ANSI_ERROR	""
#	define ANSI_GOOD	""
#	define ANSI_INFO	""
#	define ANSI_OUTPUT	""
#	define ANSI_CLEAR	""
#endif /* SYNGE_COLOUR || _UNIX */

#define OUTPUT_PADDING		""
#define ERROR_PADDING		""

#define CLI_PROMPT			">>> "
#define CLI_COMMAND_PREFIX	':'

#define SYNGE_CLI_LICENSE	"Synge-CLI: A command-line interface for Synge\n" SYNGE_LICENSE

#define CLI_BANNER	"Synge-CLI " SYNGE_CLI_VERSION "\n" \
			"Copyright (C) 2013 Cyphar\n" \
			"This free software is licensed under the terms of the MIT License and is provided with ABSOLUTELY NO WARRANTY\n" \
			"For more information, type ':version', ':license' and ':warranty'\n"

#define FLUSH_INPUT() do { while(getchar() != '\n'); } while(0) /* flush input buffer */

typedef struct cli_command {
	char *name;
	void (*exec)();
	bool whole;
} cli_command;

void cli_version(void) {
	synge_v core = synge_get_version();

	printf(	"%s"
			"Synge:       %s\n"
			"Synge-CLI:   %s\n"
#if defined(SYNGE_REVISION)
			"Revision:    " SYNGE_REVISION "\n"
#endif
			"Compiled:    %s, %s\n"
			"%s", ANSI_INFO, core.version, SYNGE_CLI_VERSION, __TIME__, __DATE__, ANSI_CLEAR);

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
	/* get argument */
	while(isspace(*s) && *s)
		s++;
	char *args = s;

	/* TODO: Should be replaced with a struct lookup */
	if(!strcmp(args, "functions")) {
		function *function_list = synge_get_function_list();

		unsigned int i, longest = 0;

		/* get longest name */
		for(i = 0; function_list[i].name != NULL; i++)
			if(strlen(function_list[i].prototype) > longest)
				longest = strlen(function_list[i].prototype);

		for(i = 0; function_list[i].name != NULL; i++)
			printf("%s%*s - %s%s\n", ANSI_INFO, longest, function_list[i].prototype, function_list[i].description, ANSI_CLEAR);
	}
	else if(!strcmp(args, "constants")) {
		word *constant_list = synge_get_constant_list();

		unsigned int i, longest = 0;

		/* get longest name */
		for(i = 0; constant_list[i].name != NULL; i++)
			if(strlen(constant_list[i].name) > longest)
				longest = strlen(constant_list[i].name);

		for(i = 0; constant_list[i].name != NULL; i++)
			printf("%s%*s - %s%s\n", ANSI_INFO, longest, constant_list[i].name, constant_list[i].description, ANSI_CLEAR);

		free(constant_list);
	}
	else if(!strcmp(args, "expressions")) {
		struct ohm_t *exps = synge_get_expression_list();

		unsigned int longest = 0;
		struct ohm_iter i = ohm_iter_init(exps);

		/* get longest word */
		for(; i.key; ohm_iter_inc(&i))
			if(strlen(i.key) > longest)
				longest = strlen(i.key);

		i = ohm_iter_init(exps);
		for(; i.key; ohm_iter_inc(&i))
			printf("%s%*s - %s%s\n", ANSI_INFO, longest, (char *) i.key, (char *) i.value, ANSI_CLEAR);
	}
	else if(!strcmp(args, "variables")) {
		struct ohm_t *vars = synge_get_variable_list();

		unsigned int longest = 0;
		struct ohm_iter i = ohm_iter_init(vars);

		/* get longest word */
		for(; i.key; ohm_iter_inc(&i)) {
			if(strlen(i.key) > longest)
				longest = strlen(i.key);
		}

		i = ohm_iter_init(vars);
		for(; i.key; ohm_iter_inc(&i)) {
			synge_t *num = i.value;
			synge_printf("%s%*s - %.*" SYNGE_FORMAT "%s\n", ANSI_INFO, longest, (char *) i.key, synge_get_precision(*num), *num, ANSI_CLEAR);
		}
	}
	else {
		printf("%s%s%s%s\n", ERROR_PADDING, ANSI_ERROR, synge_error_msg_pos(UNKNOWN_TOKEN, -1), ANSI_CLEAR);
	}
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
	synge_settings current_settings = synge_get_settings();

	/* get argument */
	while(isspace(*s) && *s)
		s++;
	char *args = s;

	/* TODO: Should be replaced with a struct lookup */
	char *ret = NULL, *tmpfree = NULL;
	if(!strcmp(args, "mode")) {
		switch(current_settings.mode) {
			case degrees:
				ret = "Degrees";
				break;
			case gradians:
				ret = "Gradians";
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
		printf("%s%s%s%s\n", ERROR_PADDING, ANSI_ERROR, synge_error_msg_pos(UNKNOWN_TOKEN, -1), ANSI_CLEAR);
	else printf("%s%s%s%s\n", OUTPUT_PADDING, ANSI_INFO, ret, ANSI_CLEAR);

	free(tmpfree);
} /* cli_print_settings() */

void cli_set_settings(char *s) {
	synge_settings new_settings = synge_get_settings();

	/* get argument */
	while(isspace(*s) && *s)
		s++;
	char *args = s;

	/* get new value */
	while(!isspace(*s) && *s)
		s++;
	while(isspace(*s) && *s)
		s++;
	char *val = s;

	/* TODO: Should be replaced with a struct lookup */
	bool err = false;
	if(!strncmp(args, "mode ", strlen("mode "))) {
		if(!strcasecmp(val, "degrees"))
			new_settings.mode = degrees;
		else if(!strcasecmp(val, "gradians"))
			new_settings.mode = gradians;
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
		printf("%s%s%s%s\n", ERROR_PADDING, ANSI_ERROR, synge_error_msg_pos(UNKNOWN_TOKEN, -1), ANSI_CLEAR);
	else {
		synge_set_settings(new_settings);

		char *p = args;
		while(!isspace(*p) && *p)
			p++;

		char *tmp = malloc(p - args + 1);
		strncpy(tmp, args, p - args);
		tmp[p - args] = '\0';

		cli_print_settings(tmp);
		free(tmp);
	}
} /* cli_set_settings() */

void cli_exec(char *str) {
	/* get rid of leading spaces */
	while(isspace(*str)) {
		str++;
	}

#if defined(SYNGE_SAFE)
	int ch;
	do {
		/* print warning */
		printf("%sAre you sure you wish to run the command %s'%s'%s as %syou%s (y/n)?%s ", ANSI_INFO, ANSI_ERROR, str, ANSI_INFO, ANSI_ERROR, ANSI_INFO, ANSI_CLEAR);
		fflush(stdout);

		/* get response */
		ch = getchar();

		/* just pressed enter -- no need to do anything else */
		if(ch == '\n')
			continue;

		FLUSH_INPUT();
	} while(tolower(ch) != 'y' && tolower(ch) != 'n'); /* wait for valid input */

	/* user said no - gtfo */
	if(tolower(ch) == 'n')
		return;
#endif /* SYNGE_SAFE */

	/* run the command */
	int ret = system(str);

#if defined(_UNIX)
	ret /= 256; /* system in *nix seems to multiply the real return value by 256 */
#endif /* _UNIX */

	/* an error occured */
	if(ret)
		printf("%s%sError code: %d%s\n", ERROR_PADDING, ANSI_ERROR, ret, ANSI_CLEAR);
} /* cli_exec() */

cli_command cli_command_list[] = {
	{"exit",		NULL,				true},
	{"exit()",		NULL,				true},
	{"quit",		NULL,				true},
	{"quit()",		NULL,				true},

	{"version",		cli_version,		true},
	{"license",		cli_license,		true},
	{"warranty",	cli_warranty,		true},
	{"banner",		cli_banner,			true},

	{"$",			cli_exec,			false},

	{"list ",		cli_print_list,		false},
	{"set ",		cli_set_settings,	false},
	{"get ",		cli_print_settings,	false},
};

cli_command cli_get_command(char *s) {
	int i, len = length(cli_command_list);

	for(i = 0; i < len; i++) {
		if((cli_command_list[i].whole && !strcasecmp(cli_command_list[i].name, s)) ||
		   (!cli_command_list[i].whole && !strncasecmp(cli_command_list[i].name, s, strlen(cli_command_list[i].name))))
			return cli_command_list[i];
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

/* for some reason, this code (when run on Windows) has
 * history and line editing automatically added */
char *cli_fallback_prompt(void) {
	printf("%s", CLI_PROMPT);
	fflush(stdout);

	char *input = NULL, ch;
	int len = 0;

	while((ch = getchar()) != '\n') {
		input = realloc(input, ++len);
		input[len - 1] = ch;
	}

	input = realloc(input, ++len);
	input[len - 1] = '\0';

	return input;
} /* fallback_prompt() */

int cli_str_empty(char *str) {
	int i, len = strlen(str);

	/* go through entire string */
	for(i = 0; i < len; i++)
		/* if a character isn't whitepace, gtfo */
		if(!isspace(str[i]))
			return false;

	/* string was empty */
	return true;
} /* cli_str_empty() */

void cli_default_settings(void) {
	synge_settings new = synge_get_settings();

	new.error = traceback;

	synge_set_settings(new);
} /* cli_default_settings() */

int main(int argc, char **argv) {
	synge_start();
	synge_seed(time(NULL) ^ getpid());
	cli_default_settings();

	char *cur_str = NULL;
	synge_t result;
	mpfr_init2(result, SYNGE_PRECISION);

	error_code ecode;
#if defined(_UNIX)
	/* rawline stuff */
	struct raw_t *cli_raw = raw_new("exit");
	raw_hist(cli_raw, true, 1000);
#endif /* _UNIX */

	/* print banner (cli_banner has a leading newline)*/
	printf("%s%s%s\n", ANSI_INFO, CLI_BANNER, ANSI_CLEAR);

	while(true) {
#if defined(_UNIX)
		cur_str = raw_input(cli_raw, CLI_PROMPT); /* get input */
		if(strchr(cur_str, '\n')) *strchr(cur_str, '\n') = '\0';
#else
		cur_str = cli_fallback_prompt();
#endif /* _UNIX */

		if(cur_str && !cli_str_empty(cur_str)) {

			int cont = true;
			char *cmd = cur_str;

			/* jump past spaces for command */
			while(isspace(*cmd))
				cmd++;

			/* input is cli command */
			if((cli_get_command(cmd).name && !cli_get_command(cmd).exec) || *cmd == '$' || *cmd++ == CLI_COMMAND_PREFIX) {
				synge_reset_traceback();

				while(isspace(*cmd))
					cmd++;

				if(cli_get_command(cmd).name) {
					cli_command tmp = cli_get_command(cmd);

					if(!tmp.exec)
						break; /* command is to exit */

					tmp.exec(cmd + strlen(tmp.name));
					cont = false;
				}
			}

			if(cont) {
				/* computation yielded error */
				if((ecode = synge_compute_string(cur_str, &result)).code != SUCCESS) {
					/* do not add history for empty stack errors */
					if(ecode.code == EMPTY_STACK)
						continue;

					/* if there is some form of non-ignorable return code, print it */
					else if(!synge_is_ignore_code(ecode.code))
						printf("%s%s%s%s\n", ERROR_PADDING, synge_is_success_code(ecode.code) ? ANSI_GOOD : ANSI_ERROR, synge_error_msg(ecode), ANSI_CLEAR);
				} else {
					/* otherwise, print the result of the computation*/
					synge_printf("%s%s%.*" SYNGE_FORMAT "%s\n", OUTPUT_PADDING, ANSI_OUTPUT, synge_get_precision(result), result, ANSI_CLEAR);
				}
			}

#if defined(_UNIX)
			/* add input to history */
			raw_hist_add(cli_raw);
#endif /* _UNIX */
		}
#if defined(_WINDOWS)
		free(cur_str);
#endif /* _WINDOWS */
	}

	/* free up memory */
#if defined(_UNIX)
	raw_free(cli_raw);
#else
	free(cur_str);
#endif /* _UNIX */
	mpfr_clears(result, NULL);
	synge_end();
	return 0;
}
