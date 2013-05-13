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

#####################
# CONSTANTS SECTION #
#####################

CC		?= gcc
EXEC_BASE	= synge

EXEC_CLI	= $(EXEC_BASE)-cli
EXEC_GTK	= $(EXEC_BASE)-gtk
EXEC_TEST	= $(EXEC_BASE)-test

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
GTK_DEPS	= $(GTK_DIR)/xmltemplate.h
TEST_DEPS	=

VERSION		= 1.1.0
CLI_VERSION	= 1.0.5
GTK_VERSION	= 1.0.1 [CONCEPT]

TO_CLEAN	= $(EXEC_CLI) $(EXEC_GTK) $(EXEC_TEST) $(GTK_DIR)/xmlui.h

PREFIX		?=
INSTALL_DIR	= $(PREFIX)/bin

######################
# PRODUCTION SECTION #
######################

# Compile "production" engine and wrappers
all: $(SHR_SRC) $(CLI_SRC) $(GTK_SRC) $(SHR_DEPS)
	make cli
	make gtk

# Compile "production" engine and command-line wrapper
cli: $(SHR_SRC) $(CLI_SRC) $(SHR_DEPS) $(CLI_DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_CLI) \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"'
	strip $(EXEC_BASE)-cli

# Compile "production" engine and gui wrapper
gtk: $(SHR_SRC) $(GTK_SRC) $(SHR_DEPS) $(GTK_DEPS)
	make -B xmlui
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_GTK) \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"'
	strip $(EXEC_BASE)-gtk

################
# TEST SECTION #
################

# Compile "production" engine and test suite wrapper + execute test suite
test: $(SHR_SRC) $(TEST_SRC) $(SHR_DEPS) $(TEST_DEPS)
	$(CC) $(SHR_SRC) $(TEST_SRC) $(SHR_LFLAGS) $(TEST_LFLAGS) \
		$(SHR_CFLAGS) $(TEST_CFLAGS) -o $(EXEC_TEST) \
		-D__SYNGE_VERSION__='"$(VERSION)"'
	strip $(EXEC_BASE)-test
	@if [ -z "`python2 --version 2>&1`" ]; then \
		echo "python2 not found - required for test suite"; \
		false; \
	else \
		python2 $(TEST_DIR)/test.py ./$(EXEC_TEST); \
	fi

#################
# DEBUG SECTION #
#################

# Compile "debug" engine and wrappers
debug: $(SHR_SRC) $(CLI_SRC) $(GTK_SRC) $(SHR_DEPS) $(CLI_DEPS) $(GTK_DEPS)
	make debug-cli
	make debug-gtk

# Compile "debug" engine and command-line wrapper
debug-cli: $(SHR_SRC) $(CLI_SRC) $(SHR_DEPS) $(CLI_DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_CLI) \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"'

# Compile "debug" engine and gui wrapper
debug-gtk: $(SHR_SRC) $(GTK) $(SHR_DEPS) $(GTK_DEPS)
	make -B xmlui
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_GTK) \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"'

###################
# INSTALL SECTION #
###################

# Install both wrappers
install:
	make install-cli
	make install-gtk

# Install cli wrapper
install-cli:
	cp $(EXEC_CLI) $(INSTALL_DIR)/$(EXEC_CLI)

# Install gtk wrapper
install-gtk:
	cp $(EXEC_GTK) $(INSTALL_DIR)/$(EXEC_GTK)

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

# Uninstall cli wrapper
uninstall-cli:
	rm -f $(INSTALL_DIR)/$(EXEC_CLI)

# Uninstall gtk wrapper
uninstall-gtk:
	rm -f $(INSTALL_DIR)/$(EXEC_GTK)

################
# MISC SECTION #
################

xmlui:
	@if [ -z "`python2 --version 2>&1`" ]; then \
		echo "python2 not found - required to bake gtk xml ui"; \
		false; \
	else \
		echo "--- BAKING UI ---"; \
		python2 $(GTK_DIR)/bakeui.py $(GTK_DIR)/xmltemplate.h $(GTK_DIR)/ui.glade $(GTK_DIR)/xmlui.h; \
	fi
