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

SRC_DIR		= src
CLI_DIR		= $(SRC_DIR)/cli
GTK_DIR		= $(SRC_DIR)/gtk
TEST_DIR	= tests

SHR_CFLAGS	= -Wall -pedantic -std=c99 -fsigned-char
CLI_CFLAGS	= `pkg-config --cflags libedit` -I$(SRC_DIR)/
GTK_CFLAGS	= `pkg-config --cflags gtk+-3.0` -export-dynamic -I$(SRC_DIR)/
TEST_CFLAGS	= -I$(SRC_DIR)/

SHR_LFLAGS	= -lm
CLI_LFLAGS	= `pkg-config --libs libedit`
GTK_LFLAGS	= `pkg-config --libs gtk+-3.0 gmodule-2.0 gmodule-export-2.0`
GTK_LFLAGS	= `pkg-config --libs gtk+-3.0`
TEST_LFLAGS	=

SHR_SRC		= $(SRC_DIR)/stack.c $(SRC_DIR)/synge.c
CLI_SRC		= $(CLI_DIR)/cli.c
GTK_SRC		= $(GTK_DIR)/gtk.c
TEST_SRC	= $(TEST_DIR)/test.c

SHR_DEPS	= $(SRC_DIR)/stack.h $(SRC_DIR)/synge.h
CLI_DEPS	=
GTK_DEPS	= $(GTK_DIR)/xmltemplate.h $(GTK_DIR)/xmlui.h
TEST_DEPS	=

VERSION		= 1.1.0
CLI_VERSION	= 1.0.4
GTK_VERSION	= 1.0.0

TO_CLEAN	= $(EXEC_BASE)-cli $(EXEC_BASE)-gtk $(EXEC_BASE)-test $(GTK_DIR)/xmlui.h

# Compile "production" engine and wrappers
all: $(SHR_SRC) $(CLI_SRC) $(GTK_SRC) $(SHR_DEPS)
	make cli
	make gtk

# Compile "production" engine and command-line wrapper
cli: $(SHR_SRC) $(CLI_SRC) $(SHR_DEPS) $(CLI_DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_BASE)-cli \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"'
	strip $(EXEC_BASE)-cli

# Compile "production" engine and gui wrapper
gtk: $(SHR_SRC) $(GTK_SRC) $(SHR_DEPS) $(GTK_DEPS)
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_BASE)-gtk \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"'
	strip $(EXEC_BASE)-gtk

# Compile "production" engine and test suite wrapper + execute test suite
test: $(SHR_SRC) $(TEST_SRC) $(SHR_DEPS) $(TEST_DEPS)
	$(CC) $(SHR_SRC) $(TEST_SRC) $(SHR_LFLAGS) $(TEST_LFLAGS) \
		$(SHR_CFLAGS) $(TEST_CFLAGS) -o $(EXEC_BASE)-test \
		-D__SYNGE_VERSION__='"$(VERSION)"'
	strip $(EXEC_BASE)-test
	@if [ -z "`python2 --version 2>&1`" ]; then \
		echo "python2 not found - required for test suite"; \
		false; \
	else \
		python2 $(TEST_DIR)/test.py ./$(EXEC_BASE)-test; \
	fi

# Compile "debug" engine and wrappers
debug: $(SHR_SRC) $(CLI_SRC) $(GTK_SRC) $(SHR_DEPS) $(CLI_DEPS) $(GTK_DEPS)
	make debug-cli
	make debug-gtk

# Compile "debug" engine and command-line wrapper
debug-cli: $(SHR_SRC) $(CLI_SRC) $(SHR_DEPS) $(CLI_DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_BASE)-cli \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"'

# Compile "debug" engine and gui wrapper
debug-gtk: $(SHR_SRC) $(GTK) $(SHR_DEPS) $(GTK_DEPS)
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_BASE)-gtk \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"'

# Clean working directory
clean:
	rm -f $(TO_CLEAN)

$(GTK_DIR)/xmlui.h:
	@if [ -z "`python2 --version 2>&1`" ]; then \
		echo "python2 not found - required to bake gtk xml ui"; \
		false; \
	else \
		python2 $(GTK_DIR)/bakeui.py $(GTK_DIR)/xmltemplate.h $(GTK_DIR)/ui.glade $(GTK_DIR)/xmlui.h; \
	fi
