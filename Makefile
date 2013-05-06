# Synge: A shunting-yard calculation "engine"
# Synge-CLI: A command-line wrapper for Synge
# Synge-GTK: A GTK+ gui wrapper for Synge

# Copyright (c) 2013 Cyphar

# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:

# 1. The above copyright notice and this permission notice shall be included in
#    all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

CC		?= gcc
EXEC_BASE	= synge

SHR_CFLAGS	= -Wall -pedantic -std=c99 -fsigned-char
CLI_CFLAGS	= `pkg-config --cflags libedit` -Isrc/
GTK_CFLAGS	= `pkg-config --cflags gtk+-3.0` -export-dynamic -Isrc/
TEST_CFLAGS	= -Isrc/

SHR_LFLAGS	= -lm
CLI_LFLAGS	= `pkg-config --libs libedit`
GTK_LFLAGS	= `pkg-config --libs gtk+-3.0 gmodule-2.0 gmodule-export-2.0`
GTK_LFLAGS	= `pkg-config --libs gtk+-3.0`
TEST_LFLAGS	=

SHR_SRC		= src/stack.c src/synge.c
CLI_SRC		= src/cli/cli.c
GTK_SRC		= src/gtk/gtk.c
TEST_SRC	= tests/test.c

DEPS		= src/stack.h src/synge.h

VERSION		= 1.0.9
CLI_VERSION	= 1.0.4
GTK_VERSION	= 1.0.0

# Compile "production" engine and wrappers
all: $(SHR_SRC) $(CLI_SRC) $(GTK_SRC) $(DEPS)
	make cli
	make gtk

# Compile "production" engine and command-line wrapper
cli: $(SHR_SRC) $(CLI_SRC) $(DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_BASE)-cli \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"'
	strip $(EXEC_BASE)-cli

# Compile "production" engine and gui wrapper
gtk: $(SHR_SRC) $(GTK_SRC) $(DEPS)
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_BASE)-gtk \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"'
	strip $(EXEC_BASE)-gtk

# Compile "production" engine and test suite wrapper + execute test suite
test: $(SHR_SRC) $(TEST_SRC) $(DEPS)
	$(CC) $(SHR_SRC) $(TEST_SRC) $(SHR_LFLAGS) $(TEST_LFLAGS) \
		$(SHR_CFLAGS) $(TEST_CFLAGS) -o $(EXEC_BASE)-test \
		-D__SYNGE_VERSION__='"$(VERSION)"'
	strip $(EXEC_BASE)-test
	@if [ -z "`python --version 2>&1`" ]; then \
		echo "python not found - required for test suite"; \
	else \
		python tests/test.py ./$(EXEC_BASE)-test; \
	fi

# Compile "debug" engine and wrappers
debug: $(SHR_SRC) $(CLI_SRC) $(GTK_SRC) $(DEPS)
	make debug-cli
	make debug-gtk

# Compile "debug" engine and command-line wrapper
debug-cli: $(SHR_SRC) $(CLI_SRC) $(DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_BASE)-cli \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"'

# Compile "debug" engine and gui wrapper
debug-gtk: $(SHR_SRC) $(GTK) $(DEPS)
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_BASE)-gtk \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"'

# Clean working directory
clean:
	rm -f $(EXEC_BASE)-*
