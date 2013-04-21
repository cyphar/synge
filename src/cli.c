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
#include <math.h>

#include "stack.h"
#include "calculator.h"
#include "definitions.h"

#define ANSI_ERROR	"\x1b[1;31m"
#define ANSI_INFO	"\x1b[1;37m"
#define ANSI_CLEAR	"\x1b[0;m"

#ifndef __SYNGE_CLI_VERSION__
#define __SYNGE_CLI_VERSION__ ""
#endif

#define BANNER	"Synge-Cli " __SYNGE_CLI_VERSION__ "\n" \
		"Copyright (C) 2013 Cyphar\n" \
		"This free software is licensed under the terms of the MIT License with ABSOLUTELY NO WARRANTY\n" \
		"For more information, type 'version', 'license' and 'warranty'\n" \

void cli_license(void) {
	printf("\n%s%s%s\n", ANSI_INFO, CLI_LICENSE, ANSI_CLEAR);
} /* cli_license() */

void cli_warranty(void) {
	printf("\n%s%s%s\n", ANSI_INFO, WARRANTY, ANSI_CLEAR);
} /* cli_warranty() */

void cli_version(void) {
	printf(	"\n%s"
		"Synge:       %s\n"
		"Synge-Cli:   %s\n"
		"%s\n", ANSI_INFO, __SYNGE_VERSION__, __SYNGE_CLI_VERSION__, ANSI_CLEAR);
} /* cli_version() */

void cli_banner(void) {
	printf("%s%s%s\n", ANSI_INFO, BANNER, ANSI_CLEAR);
} /* cli_banner() */

char *get_cli_str(void) {
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
		cur_str = get_cli_str();

		if(!strcmp("exit", cur_str)) break;
		else if(!strcmp("license", cur_str)) cli_license();
		else if(!strcmp("warranty", cur_str)) cli_warranty();
		else if(!strcmp("version", cur_str)) cli_version();
		else if((ecode = compute_infix_string(cur_str, &result)) != SUCCESS) {
			char *error_msg = get_error_msg(ecode);
			printf("\t\t%s%s%s\n", ANSI_ERROR, error_msg, ANSI_CLEAR);
		}
		else {
			double tmp = 0;
			if(modf(result, &tmp) == 0.0) printf("\t\t= %.0f\n", result);
			else printf("\t\t= %f\n", result);
		}
	}
	if(cur_str) sfree(&cur_str);
	return 0;
}
