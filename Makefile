# Synge: A shunting-yard calculation "engine"
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

#######################
# OS SPECIFIC SECTION #
#######################

ifeq ($(OS), Windows_NT)
	OS_SHR_CFLAGS	=
	OS_CLI_CFLAGS	=
	OS_GTK_CFLAGS	=
	OS_EVAL_CFLAGS	=

	OS_SHR_LFLAGS	=
	OS_CLI_LFLAGS	=
	OS_GTK_LFLAGS	= -mwindows
	OS_EVAL_LFLAGS	=

	OS_PREFIX	=
	OS_SUFFIX	=.exe

	OS_GIT_VERSION	=
else
	OS_SHR_CFLAGS	= -Werror
	OS_CLI_CFLAGS	= `pkg-config --cflags libedit`
	OS_GTK_CFLAGS	= -export-dynamic
	OS_EVAL_CFLAGS	=

	OS_SHR_LFLAGS	=
	OS_CLI_LFLAGS	= `pkg-config --libs libedit`
	OS_GTK_LFLAGS	=
	OS_EVAL_LFLAGS	=

	OS_PREFIX	=./
	OS_SUFFIX	=

	OS_GIT_VERSION	= $(shell git rev-parse --verify HEAD)
endif

PYTHON		= python

#####################
# CONSTANTS SECTION #
#####################

CC		?= cc
EXEC_BASE	= synge
SAFE		= 1

NAME_CLI	= $(EXEC_BASE)-cli
NAME_GTK	= $(EXEC_BASE)-gtk
NAME_EVAL	= $(EXEC_BASE)-eval

EXEC_CLI	= $(NAME_CLI)$(OS_SUFFIX)
EXEC_GTK	= $(NAME_GTK)$(OS_SUFFIX)
EXEC_EVAL	= $(NAME_EVAL)$(OS_SUFFIX)

SRC_DIR		= src
CLI_DIR		= $(SRC_DIR)/cli
GTK_DIR		= $(SRC_DIR)/gtk
EVAL_DIR	= $(SRC_DIR)/eval
TEST_DIR	= tests

WARNINGS	= -Wall -Wextra -pedantic -Wno-overlength-strings -Wno-sign-compare -Wno-unused-parameter
SHR_CFLAGS	= -std=c99 -fsigned-char -I$(SRC_DIR)/ -D__SYNGE_SAFE__="$(SAFE)" $(OS_SHR_CFLAGS)
CLI_CFLAGS	= $(OS_CLI_CFLAGS)
GTK_CFLAGS	= `pkg-config --cflags gtk+-2.0` $(OS_GTK_CFLAGS)
EVAL_CFLAGS	= $(OS_EVAL_CFLAGS)

SHR_LFLAGS	= -lm $(OS_SHR_LFLAGS)
CLI_LFLAGS	= $(OS_CLI_LFLAGS)
GTK_LFLAGS	= `pkg-config --libs gtk+-2.0 gmodule-2.0` $(OS_GTK_LFLAGS)
EVAL_LFLAGS	= $(OS_EVAL_LFLAGS)

SHR_SRC		= $(SRC_DIR)/stack.c $(SRC_DIR)/synge.c $(SRC_DIR)/ohmic.c $(SRC_DIR)/linked.c
CLI_SRC		= $(CLI_DIR)/cli.c
GTK_SRC		= $(GTK_DIR)/gtk.c
EVAL_SRC	= $(EVAL_DIR)/eval.c

SHR_DEPS	= $(SRC_DIR)/stack.h $(SRC_DIR)/synge.h $(SRC_DIR)/ohmic.h
CLI_DEPS	=
GTK_DEPS	= $(GTK_DIR)/xmltemplate.h $(GTK_DIR)/ui.glade $(GTK_DIR)/bakeui.py
EVAL_DEPS	=

VERSION		= 1.3.1
CLI_VERSION	= 1.1.0
GTK_VERSION	= 1.0.1 [CONCEPT]
EVAL_VERSION	= 1.0.2

TO_CLEAN	= $(EXEC_CLI) $(EXEC_GTK) $(EXEC_EVAL) $(GTK_DIR)/xmlui.h

PREFIX		?= /usr
INSTALL_DIR	= $(PREFIX)/bin

######################
# PRODUCTION SECTION #
######################

# Compile "production" engine and wrappers
all:
	make $(NAME_CLI)
	make $(NAME_GTK)
	make $(NAME_EVAL)

# Compile "production" engine and command-line wrapper
$(NAME_CLI): $(SHR_SRC) $(CLI_SRC) $(SHR_DEPS) $(CLI_DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_CLI) \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GIT_VERSION__='"$(OS_GIT_VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"' \
		$(WARNINGS)
	strip $(EXEC_CLI)

# Compile "production" engine and gui wrapper
$(NAME_GTK): $(SHR_SRC) $(GTK_SRC) $(SHR_DEPS) $(GTK_DEPS)
	make -B xmlui
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_GTK) \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GIT_VERSION__='"$(OS_GIT_VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"' \
		$(WARNINGS)
	strip $(EXEC_GTK)

# Compile "production" engine and simple eval wrapper
$(NAME_EVAL): $(SHR_SRC) $(EVAL_SRC) $(SHR_DEPS) $(EVAL_DEPS)
	$(CC) $(SHR_SRC) $(EVAL_SRC) $(SHR_LFLAGS) $(EVAL_LFLAGS) \
		$(SHR_CFLAGS) $(EVAL_CFLAGS) -o $(EXEC_EVAL) \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GIT_VERSION__='"$(OS_GIT_VERSION)"' \
		-D__SYNGE_EVAL_VERSION__='"$(EVAL_VERSION)"' \
		$(WARNINGS)
	strip $(EXEC_EVAL)

################
# TEST SECTION #
################

# Compile "production" engine and test suite wrapper + execute test suite
test: $(NAME_EVAL) $(SHR_SRC) $(TEST_SRC) $(SHR_DEPS) $(TEST_DEPS)
	@if [ -z "`$(PYTHON) --version 2>&1`" ]; then \
		echo "$(PYTHON) not found - required for test suite"; \
		false; \
	else \
		$(PYTHON) $(TEST_DIR)/test.py "$(OS_PREFIX)$(EXEC_EVAL)$(OS_SUFFIX) -R -S"; \
	fi

#################
# DEBUG SECTION #
#################

# Compile "debug" engine and wrappers
debug: $(SHR_SRC) $(CLI_SRC) $(GTK_SRC) $(SHR_DEPS) $(CLI_DEPS) $(GTK_DEPS)
	make debug-cli
	make debug-gtk
	make debug-eval

# Compile "debug" engine and command-line wrapper
debug-cli: $(SHR_SRC) $(CLI_SRC) $(SHR_DEPS) $(CLI_DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_CLI) \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GIT_VERSION__='"$(OS_GIT_VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"' \
		$(WARNINGS)

# Compile "debug" engine and gui wrapper
debug-gtk: $(SHR_SRC) $(GTK) $(SHR_DEPS) $(GTK_DEPS)
	make -B xmlui
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_GTK) \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GIT_VERSION__='"$(OS_GIT_VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"' \
		$(WARNINGS)

# Compile "debug" engine and simple eval wrapper
debug-eval: $(SHR_SRC) $(EVAL_SRC) $(SHR_DEPS) $(EVAL_DEPS)
	$(CC) $(SHR_SRC) $(EVAL_SRC) $(SHR_LFLAGS) $(EVAL_LFLAGS) \
		$(SHR_CFLAGS) $(EVAL_CFLAGS) -o $(EXEC_EVAL) \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GIT_VERSION__='"$(OS_GIT_VERSION)"' \
		-D__SYNGE_EVAL_VERSION__='"$(EVAL_VERSION)"' \
		$(WARNINGS)

###################
# INSTALL SECTION #
###################

# Install both wrappers
install:
	make install-cli
	make install-gtk
	make install-eval

# Install cli wrapper
install-cli: $(EXEC_CLI)
	cp $(EXEC_CLI) $(INSTALL_DIR)/$(EXEC_CLI)

# Install gtk wrapper
install-gtk: $(EXEC_GTK)
	cp $(EXEC_GTK) $(INSTALL_DIR)/$(EXEC_GTK)

# Install eval wrapper
install-eval: $(EXEC_EVAL)
	cp $(EXEC_EVAL) $(INSTALL_DIR)/$(EXEC_EVAL)

#################
# CLEAN SECTION #
#################

# Clean working directory
clean:
	rm -f $(TO_CLEAN)

# Uninstall both wrappers
uninstall:
	make uninstall-cli
	make uninstall-gtk
	make uninstall-eval

# Uninstall cli wrapper
uninstall-cli:
	rm -f $(INSTALL_DIR)/$(EXEC_CLI)

# Uninstall gtk wrapper
uninstall-gtk:
	rm -f $(INSTALL_DIR)/$(EXEC_GTK)

# Uninstall eval wrapper
uninstall-eval:
	rm -f $(INSTALL_DIR)/$(EXEC_EVAL)

################
# MISC SECTION #
################

xmlui:
	@if [ -z "`$(PYTHON) --version 2>&1`" ]; then \
		echo "$(PYTHON) not found - required to bake gtk xml ui"; \
		false; \
	else \
		echo "--- BAKING UI ---"; \
		$(PYTHON) $(GTK_DIR)/bakeui.py $(GTK_DIR)/xmltemplate.h $(GTK_DIR)/ui.glade $(GTK_DIR)/xmlui.h; \
	fi
