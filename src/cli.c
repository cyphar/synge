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

#include "stack.h"
#include "synge.h"
#include "definitions.h"

#define length(x) (sizeof(x) / sizeof(x[0]))

#define ANSI_ERROR	"\x1b[1;31m"
#define ANSI_INFO	"\x1b[1;37m"
#define ANSI_CLEAR	"\x1b[0;m"
#define OUTPUT_PADDING	"\t\t"

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
} cli_command;

void cli_version(void) {
	printf(	"\n%s"
		"Synge:       %s\n"
		"Synge-Cli:   %s\n"
		"%s\n", ANSI_INFO, __SYNGE_VERSION__, __SYNGE_CLI_VERSION__, ANSI_CLEAR);
} /* cli_version() */

void cli_license(void) {
	printf("\n%s%s%s\n", ANSI_INFO, SYNGE_LICENSE, ANSI_CLEAR);
} /* cli_license() */

void cli_warranty(void) {
	printf("\n%s%s%s\n", ANSI_INFO, SYNGE_WARRANTY, ANSI_CLEAR);
} /* cli_warranty() */

void cli_banner(void) {
	printf("%s%s%s\n", ANSI_INFO, CLI_BANNER, ANSI_CLEAR);
} /* cli_banner() */

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
	
	if(!ret)
		printf("%sUnknown argument to 'get': %s.%s\n", ANSI_ERROR, args, ANSI_CLEAR);
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
	else err = true;
	
	if(err) printf("%sUnknown argument to 'set': %s.%s\n", ANSI_ERROR, args, ANSI_CLEAR);
	else set_synge_settings(new_settings);
} /* cli_set_settings */

cli_command cli_command_list[] = {
	{"exit",		      NULL},
	{"quit",		      NULL},

	{"version",	       cli_version},
	{"license",	       cli_license},
	{"warranty",	      cli_warranty},

	{"set ",	  cli_set_settings},
	{"get ",	cli_print_settings}
};

bool cli_is_command(char *s) {
	int i;
	for(i = 0; i < length(cli_command_list); i++)
		if(!strncmp(cli_command_list[i].name, s, strlen(cli_command_list[i].name))) return true;
	return false;
} /* cli_is_command() */

cli_command cli_get_command(char *s) {
	int i;
	for(i = 0; i < length(cli_command_list); i++)
		if(!strncmp(cli_command_list[i].name, s, strlen(cli_command_list[i].name))) return cli_command_list[i];

	cli_command empty = {NULL, NULL};
	return empty;
} /* cli_get_command() */

char *cli_get_str(void) {
	char ch, *ret = NULL;
	int size = 0;

	while((ch = getchar()) != '\n') {
		ret = realloc(ret, ++size * sizeof(char));
		ret[size - 1] = ch;
	}
	ret = realloc(ret, ++size * sizeof(char));
	ret[size - 1] = '\0';
	
	return ret;
} /* get_in_str() */

void sfree(char **pp) {
	if(pp && *pp) {
		free(*pp);
		pp = NULL;
	}
} /* sfree() */

int main(void) {
	char *cur_str = NULL;
	double result = 0;
	error_code ecode;
	cli_banner();
	while(true) {
		if(cur_str) sfree(&cur_str);
		cur_str = cli_get_str();

		if(cli_is_command(cur_str)) {
			cli_command tmp = cli_get_command(cur_str);
			if(!tmp.exec) break; /* command is to exit */
			tmp.exec(cur_str);
		}
		else if((ecode = compute_infix_string(cur_str, &result)) != SUCCESS)
			printf("%s%s%s%s\n", OUTPUT_PADDING, ANSI_ERROR, get_error_msg(ecode), ANSI_CLEAR);
		else printf("%s= %.*f\n", OUTPUT_PADDING, get_precision(result), result);
	}
	if(cur_str) sfree(&cur_str);
	return 0;
}
