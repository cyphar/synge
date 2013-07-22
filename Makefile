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
	COLOUR		= 0
	GTK_LFLAGS	= -mwindows

	EXEC_PREFIX	=
	EXEC_SUFFIX	=.exe

	GIT_VERSION	=
	SY_OS		= windows
else
	COLOUR		= 1
	SHR_CFLAGS	= -Werror

	CLI_CFLAGS	= `pkg-config --cflags libedit`
	CLI_LFLAGS	= `pkg-config --libs libedit`

	EXEC_PREFIX	=./
	EXEC_SUFFIX	=

	GIT_VERSION	= $(shell git rev-parse --verify HEAD)
	SY_OS		= unix
endif

OS_PRE		= $(SY_OS)-pre
OS_POST		= $(SY_OS)-post

PYTHON		= python

##################
# OPTIONAL FLAGS #
##################

SAFE		= 0
CHEEKY		= 0

#####################
# CONSTANTS SECTION #
#####################

CC		?= cc
EXEC_BASE	= synge

NAME_CLI	= $(EXEC_BASE)-cli
NAME_GTK	= $(EXEC_BASE)-gtk
NAME_EVAL	= $(EXEC_BASE)-eval

EXEC_CLI	= $(NAME_CLI)$(EXEC_SUFFIX)
EXEC_GTK	= $(NAME_GTK)$(EXEC_SUFFIX)
EXEC_EVAL	= $(NAME_EVAL)$(EXEC_SUFFIX)

RES_DIR		= res

RC_CLI		= $(RES_DIR)/$(NAME_CLI).rc
RC_GTK		= $(RES_DIR)/$(NAME_GTK).rc
RC_EVAL		= $(RES_DIR)/$(NAME_EVAL).rc

ICON_CLI	= $(RES_DIR)/$(NAME_CLI).o
ICON_GTK	= $(RES_DIR)/$(NAME_GTK).o
ICON_EVAL	= $(RES_DIR)/$(NAME_EVAL).o

ifeq ($(OS), Windows_NT)
	LINK_CLI	+= $(ICON_CLI)
	LINK_GTK	+= $(ICON_GTK)
	LINK_EVAL	+= $(ICON_EVAL)
endif

# Source directories
SRC_DIR		= src
CLI_SDIR	= $(SRC_DIR)/cli
GTK_SDIR	= $(SRC_DIR)/gtk
EVAL_SDIR	= $(SRC_DIR)/eval

# Include directories
INCLUDE_DIR	= include
CLI_IDIR	= $(INCLUDE_DIR)/cli
GTK_IDIR	= $(INCLUDE_DIR)/gtk
EVAL_IDIR	= $(INCLUDE_DIR)/eval

# Test directories
TEST_DIR	= tests

WARNINGS	= -Wall -Wextra -pedantic -Wno-overlength-strings -Wno-unused-parameter
SHR_CFLAGS	+= -std=c99 -I$(INCLUDE_DIR)/
CLI_CFLAGS	+=
GTK_CFLAGS	+= `pkg-config --cflags gtk+-2.0`
EVAL_CFLAGS	+=

SHR_LFLAGS	+= -lm -lgmp -lmpfr
CLI_LFLAGS	+=
GTK_LFLAGS	+= `pkg-config --libs gtk+-2.0 gmodule-2.0`
EVAL_LFLAGS	+=

SHR_SRC		+= $(SRC_DIR)/stack.c $(SRC_DIR)/synge.c $(SRC_DIR)/ohmic.c $(SRC_DIR)/linked.c
CLI_SRC		+= $(CLI_SDIR)/cli.c
GTK_SRC		+= $(GTK_SDIR)/gtk.c
EVAL_SRC	+= $(EVAL_SDIR)/eval.c

SHR_DEPS	+= $(INCLUDE_DIR)/stack.h $(INCLUDE_DIR)/synge.h $(INCLUDE_DIR)/ohmic.h $(INCLUDE_DIR)/definitions.h $(INCLUDE_DIR)/version.h
CLI_DEPS	+=
GTK_DEPS	+= $(GTK_SDIR)/xmltemplate.h $(GTK_SDIR)/ui.glade $(GTK_SDIR)/bakeui.py
EVAL_DEPS	+=

TO_CLEAN	= $(EXEC_CLI) $(EXEC_GTK) $(EXEC_EVAL) $(GTK_SDIR)/xmlui.h $(ICON_CLI) $(ICON_GTK) $(ICON_EVAL)

PREFIX		?= /usr
INSTALL_DIR	= $(PREFIX)/bin

######################
# PRODUCTION SECTION #
######################

# Compile "production" engine and wrappers (w/ git-version)
all:
	make $(NAME_CLI)
	make $(NAME_GTK)
	make $(NAME_EVAL)

# Compile "final" engine and wrappers (w/o git-version)
final:
	make $(NAME_CLI) GIT_VERSION=
	make $(NAME_GTK) GIT_VERSION=
	make $(NAME_EVAL) GIT_VERSION=

# Compile "production" engine and command-line wrapper
$(NAME_CLI): $(SHR_SRC) $(CLI_SRC) $(SHR_DEPS) $(CLI_DEPS)
	make -B $(OS_PRE)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(LINK_CLI) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_CLI) \
		-D__SYNGE_GIT_VERSION__='"$(GIT_VERSION)"' \
		-D__SYNGE_SAFE__="$(SAFE)" \
		-D__SYNGE_COLOUR__="$(COLOUR)" \
		-D__SYNGE_CHEEKY__="$(CHEEKY)" \
		$(WARNINGS)
	strip $(EXEC_CLI)
	make -B $(OS_POST)

# Compile "production" engine and gui wrapper
$(NAME_GTK): $(SHR_SRC) $(GTK_SRC) $(SHR_DEPS) $(GTK_DEPS)
	make -B $(OS_PRE)
	make -B xmlui
	$(CC) $(SHR_SRC) $(GTK_SRC) $(LINK_GTK) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_GTK) \
		-D__SYNGE_GIT_VERSION__='"$(GIT_VERSION)"' \
		-D__SYNGE_SAFE__="$(SAFE)" \
		-D__SYNGE_COLOUR__="$(COLOUR)" \
		-D__SYNGE_CHEEKY__="$(CHEEKY)" \
		$(WARNINGS)
	strip $(EXEC_GTK)
	make -B $(OS_POST)

# Compile "production" engine and simple eval wrapper
$(NAME_EVAL): $(SHR_SRC) $(EVAL_SRC) $(SHR_DEPS) $(EVAL_DEPS)
	make -B $(OS_PRE)
	$(CC) $(SHR_SRC) $(EVAL_SRC) $(LINK_EVAL) $(SHR_LFLAGS) $(EVAL_LFLAGS) \
		$(SHR_CFLAGS) $(EVAL_CFLAGS) -o $(EXEC_EVAL) \
		-D__SYNGE_GIT_VERSION__='"$(GIT_VERSION)"' \
		-D__SYNGE_SAFE__="$(SAFE)" \
		-D__SYNGE_COLOUR__="$(COLOUR)" \
		-D__SYNGE_CHEEKY__="$(CHEEKY)" \
		$(WARNINGS)
	strip $(EXEC_EVAL)
	make -B $(OS_POST)

################
# TEST SECTION #
################

# Compile "production" engine and test suite wrapper + execute test suite
test: $(NAME_EVAL) $(SHR_SRC) $(TEST_SRC) $(SHR_DEPS) $(TEST_DEPS)
	@if [ -z "`$(PYTHON) --version 2>&1`" ]; then \
		echo "$(PYTHON) not found - required for test suite"; \
		false; \
	else \
		$(PYTHON) $(TEST_DIR)/test.py "$(EXEC_PREFIX)$(EXEC_EVAL) -R -S"; \
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
	make -B $(OS_PRE)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(LINK_CLI) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_CLI) \
		-g -O0 -D__SYNGE_DEBUG__ \
		-D__SYNGE_GIT_VERSION__='"$(GIT_VERSION)"' \
		-D__SYNGE_SAFE__="$(SAFE)" \
		-D__SYNGE_COLOUR__="$(COLOUR)" \
		$(WARNINGS)
	make -B $(OS_POST)

# Compile "debug" engine and gui wrapper
debug-gtk: $(SHR_SRC) $(GTK) $(SHR_DEPS) $(GTK_DEPS)
	make -B $(OS_PRE)
	make -B xmlui
	$(CC) $(SHR_SRC) $(GTK_SRC) $(LINK_GTK) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_GTK) \
		-g -O0 -D__SYNGE_DEBUG__ \
		-D__SYNGE_GIT_VERSION__='"$(GIT_VERSION)"' \
		-D__SYNGE_SAFE__="$(SAFE)" \
		-D__SYNGE_COLOUR__="$(COLOUR)" \
		$(WARNINGS)
	make -B $(OS_POST)

# Compile "debug" engine and simple eval wrapper
debug-eval: $(SHR_SRC) $(EVAL_SRC) $(SHR_DEPS) $(EVAL_DEPS)
	make -B $(OS_PRE)
	$(CC) $(SHR_SRC) $(EVAL_SRC) $(LINK_EVAL) $(SHR_LFLAGS) $(EVAL_LFLAGS) \
		$(SHR_CFLAGS) $(EVAL_CFLAGS) -o $(EXEC_EVAL) \
		-g -O0 -D__SYNGE_DEBUG__ \
		-D__SYNGE_GIT_VERSION__='"$(GIT_VERSION)"' \
		-D__SYNGE_SAFE__="$(SAFE)" \
		-D__SYNGE_COLOUR__="$(COLOUR)" \
		$(WARNINGS)
	make -B $(OS_POST)

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
		$(PYTHON) $(GTK_SDIR)/bakeui.py $(GTK_SDIR)/xmltemplate.h $(GTK_SDIR)/ui.glade $(GTK_SDIR)/xmlui.h; \
	fi

###########################
# OS PRE/POST COMPILATION #
###########################

# Windows pre-compilation (make resource files)
windows-pre: $(RC_CLI) $(RC_GTK) $(RC_EVAL)
	windres $(RC_CLI)  -I$(INCLUDE_DIR)/ -o $(ICON_CLI)
	windres $(RC_GTK)  -I$(INCLUDE_DIR)/ -o $(ICON_GTK)
	windres $(RC_EVAL) -I$(INCLUDE_DIR)/ -o $(ICON_EVAL)

# Windows post-compilation
windows-post:
	@true

# *Unix pre-compilation
unix-pre:
	@true

# *Unix post-compilation
unix-post:
	@true

