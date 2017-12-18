# Synge: A shunting-yard calculation "engine"
# Copyright (C) 2013, 2016 Aleksa Sarai

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

#######################
# OS SPECIFIC SECTION #
#######################

ifeq ($(OS), Windows_NT)
	UTF8		= 0
	COLOUR		= 0

	GTK_LFLAGS	= -mwindows

	EXEC_PREFIX	=
	EXEC_SUFFIX	=.exe

	CORE_PREFIX	=
	CORE_SUFFIX	=.dll

	REVISION	=
	SY_OS		= windows
else
	UTF8		= 1
	COLOUR		= 1

	WARNINGS	= -Werror

	CLI_SRC		= $(CLI_SDIR)/rawline.c
	CORE_CFLAGS	= -fPIC

	EXEC_PREFIX	=./
	EXEC_SUFFIX	=

	CORE_PREFIX	=lib
	CORE_SUFFIX	=.so

	REVISION	= $(shell git rev-parse --verify HEAD)
	SY_OS		= unix
endif

XCC		 = $(CC)

ifeq ($(strip $(XCC)),)
	XCC		?= cc
endif

OS_PRE		= $(SY_OS)-pre
OS_POST		= $(SY_OS)-post

PYTHON		= python3

##################
# OPTIONAL FLAGS #
##################

REV			?= 1
BAKE		?= 1
DEBUG		?= 0
SAFE		?= 0
CHEEKY		?= 0
OPTIM		?= 0

ifeq ($(OPTIM), 1)
	XCC += -O2
endif

ifeq ($(REV), 1)
	SYNGE_FLAGS	+= -DSYNGE_REVISION='"$(REVISION)"'
endif

ifeq ($(BAKE), 1)
	SYNGE_FLAGS += -DSYNGE_BAKE
endif

ifeq ($(DEBUG), 1)
	SYNGE_FLAGS += -DSYNGE_DEBUG
	SHR_CFLAGS += -g -O0
endif

ifeq ($(SAFE), 1)
	SYNGE_FLAGS += -DSYNGE_SAFE
endif

ifeq ($(COLOUR), 1)
	SYNGE_FLAGS	+= -DSYNGE_COLOUR
endif

ifeq ($(CHEEKY), 1)
	SYNGE_FLAGS += -DSYNGE_CHEEKY
endif

ifeq ($(UTF8), 1)
	SYNGE_FLAGS += -DSYNGE_UTF8_STRINGS
endif

#####################
# CONSTANTS SECTION #
#####################

XCC			+= -pipe
EXEC_BASE	= synge
LIB_CORE	= $(EXEC_BASE)

NAME_CLI	= $(EXEC_BASE)-cli
NAME_GTK	= $(EXEC_BASE)-gtk
NAME_EVAL	= $(EXEC_BASE)-eval
NAME_CORE	= $(CORE_PREFIX)$(EXEC_BASE)$(CORE_SUFFIX)

EXEC_CLI	= $(NAME_CLI)$(EXEC_SUFFIX)
EXEC_GTK	= $(NAME_GTK)$(EXEC_SUFFIX)
EXEC_EVAL	= $(NAME_EVAL)$(EXEC_SUFFIX)

RES_DIR		= res

RC_CORE		= $(RES_DIR)/$(LIB_CORE).rc
RC_CLI		= $(RES_DIR)/$(NAME_CLI).rc
RC_GTK		= $(RES_DIR)/$(NAME_GTK).rc
RC_EVAL		= $(RES_DIR)/$(NAME_EVAL).rc

ICON_CORE	= $(RES_DIR)/$(LIB_CORE)dll.o
ICON_CLI	= $(RES_DIR)/$(NAME_CLI).o
ICON_GTK	= $(RES_DIR)/$(NAME_GTK).o
ICON_EVAL	= $(RES_DIR)/$(NAME_EVAL).o

ifeq ($(OS), Windows_NT)
	LINK_CORE	+= $(ICON_CORE)
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

# Documentation
DOC_DIR		= doc
DOC_FLAGS	= --date="`date '+%Y-%m-%d'`" --organization="cyphar" --manual="User Commands"
DOC_SRC		= $(wildcard $(DOC_DIR)/*.ronn)

DOCS		= $(DOC_SRC:.ronn=)
DOCS_COMP	= $(addsuffix .gz,$(DOCS))

DOCS1		= $(filter %1.gz,$(DOCS_COMP))
DOCS3		= $(filter %3.gz,$(DOCS_COMP))

# Man magic
MAN_DIR		?= /usr/share/man
MAN1		= $(MAN_DIR)/man1
MAN3		= $(MAN_DIR)/man3

WARNINGS	+= -Wall -Wextra -Wno-overlength-strings -Wno-unused-parameter -Wno-variadic-macros -Wno-implicit-fallthrough

SHR_CFLAGS	+= -ansi -I$(INCLUDE_DIR)/

CORE_LFLAGS	+= -lm -lgmp -lmpfr
CORE_CFLAGS	+= -DBUILD_LIB
CORE_SFLAGS += -shared

CLI_CFLAGS	+=
GTK_CFLAGS	+= `pkg-config --cflags gtk+-2.0`
EVAL_CFLAGS	+=

SHR_LFLAGS	+= -lm -lgmp -lmpfr -L. -l$(LIB_CORE)
CORE_LFLAGS	+=
CLI_LFLAGS	+=
GTK_LFLAGS	+= `pkg-config --libs gtk+-2.0 gmodule-2.0`
EVAL_LFLAGS	+=

SHR_SRC		+= $(wildcard $(SRC_DIR)/*.c)
CORE_SRC	+= $(wildcard $(SRC_DIR)/*.c)
CLI_SRC		+= $(CLI_SDIR)/cli.c
GTK_SRC		+= $(wildcard $(GTK_SDIR)/*.c)
EVAL_SRC	+= $(wildcard $(EVAL_SDIR)/*.c)

SHR_DEPS	+= $(wildcard $(INCLUDE_DIR)/*.h)
CLI_DEPS	+=
GTK_DEPS	+= $(wildcard $(GTK_SDIR)/*.h) $(GTK_SDIR)/ui.glade $(GTK_SDIR)/bakeui.py
EVAL_DEPS	+=

TO_CLEAN	= $(NAME_CORE) $(EXEC_CLI) $(EXEC_GTK) $(EXEC_EVAL) $(GTK_SDIR)/xmlui.h $(ICON_CORE) $(ICON_CLI) $(ICON_GTK) $(ICON_EVAL) $(DOCS_COMP) $(DOCS)

VALGRIND	= valgrind --leak-check=full --show-reachable=yes
PREFIX		?= /usr
INSTALL_BIN	= $(PREFIX)/bin
INSTALL_LIB = $(PREFIX)/lib

.PHONY: all doc final xmlui debug clean install uninstall test mtest unix-pre unix-post windows-pre windows-post

######################
# PRODUCTION SECTION #
######################

# Compile "production" core and interfaces (w/ git-version)
all:
	make $(NAME_CORE)
	make $(NAME_CLI)
	make $(NAME_GTK)
	make $(NAME_EVAL)

# Compile "final" core and interfaces (w/o git-version)
final:
	make $(NAME_CORE) REV=0
	make $(NAME_CLI)  REV=0
	make $(NAME_GTK)  REV=0
	make $(NAME_EVAL) REV=0

debug:
	make $(NAME_CORE) DEBUG=1
	make $(NAME_CLI)  DEBUG=1
	make $(NAME_GTK)  DEBUG=1
	make $(NAME_EVAL) DEBUG=1

# Compile core library
$(NAME_CORE): $(CORE_SRC) $(CORE_DEPS)
	make -B $(OS_PRE)
	$(XCC) $(SHR_CFLAGS) $(CORE_CFLAGS) -c $(CORE_SRC) \
		$(SYNGE_FLAGS) \
		$(WARNINGS)
	$(XCC) $(LINK_CORE) *.o -o $(NAME_CORE) $(CORE_SFLAGS) $(CORE_LFLAGS)
	if [ $(DEBUG) -ne 1 ]; then strip $(NAME_CORE); fi
	make -B $(OS_POST)
	rm *.o

# Compile command-line interface
$(NAME_CLI): $(NAME_CORE) $(SHR_SRC) $(CLI_SRC) $(SHR_DEPS) $(CLI_DEPS)
	make -B $(OS_PRE)
	$(XCC) $(CLI_SRC) $(LINK_CLI) $(SHR_LFLAGS) $(CLI_LFLAGS) \
		$(SHR_CFLAGS) $(CLI_CFLAGS) -o $(EXEC_CLI) \
		$(SYNGE_FLAGS) \
		$(WARNINGS)
	if [ $(DEBUG) -ne 1 ]; then strip $(EXEC_CLI); fi
	make -B $(OS_POST)

# Compile gui interface
$(NAME_GTK): $(NAME_CORE) $(SHR_SRC) $(GTK_SRC) $(SHR_DEPS) $(GTK_DEPS)
	make -B $(OS_PRE)
	make -B xmlui
	$(XCC) $(GTK_SRC) $(LINK_GTK) $(SHR_LFLAGS) $(GTK_LFLAGS) \
		$(SHR_CFLAGS) $(GTK_CFLAGS) -o $(EXEC_GTK) \
		$(SYNGE_FLAGS) \
		$(WARNINGS)
	if [ $(DEBUG) -ne 1 ]; then strip $(EXEC_GTK); fi
	make -B $(OS_POST)

# Compile simple eval interface
$(NAME_EVAL): $(NAME_CORE) $(SHR_SRC) $(EVAL_SRC) $(SHR_DEPS) $(EVAL_DEPS)
	make -B $(OS_PRE)
	$(XCC) $(EVAL_SRC) $(LINK_EVAL) $(SHR_LFLAGS) $(EVAL_LFLAGS) \
		$(SHR_CFLAGS) $(EVAL_CFLAGS) -o $(EXEC_EVAL) \
		$(SYNGE_FLAGS) \
		$(WARNINGS)
	if [ $(DEBUG) -ne 1 ]; then strip $(EXEC_EVAL); fi
	make -B $(OS_POST)

################
# TEST SECTION #
################

# Execute test suite
test: $(NAME_EVAL) $(SHR_SRC) $(TEST_SRC) $(SHR_DEPS) $(TEST_DEPS)
	@if [ -z "`$(PYTHON) --version 2>&1`" ]; then \
		echo "$(PYTHON) not found -- required for test suite"; \
		false; \
	else \
		LD_LIBRARY_PATH=. $(PYTHON) $(TEST_DIR)/test.py "$(EXEC_PREFIX)$(EXEC_EVAL) -R -S"; \
	fi

# Execute test suite (in valgrind)
mtest: $(NAME_EVAL) $(SHR_SRC) $(TEST_SRC) $(SHR_DEPS) $(TEST_DEPS)
	@if [ -z "`$(PYTHON) --version 2>&1`" ]; then \
		echo "$(PYTHON) not found -- required for test suite"; \
		false; \
	else \
		LD_LIBRARY_PATH=. $(PYTHON) $(TEST_DIR)/test.py "$(VALGRIND) $(EXEC_PREFIX)$(EXEC_EVAL) -R -S"; \
	fi

#########################
# DOCUMENTATION SECTION #
#########################

doc: $(DOC_SRC)
	ronn -r $(DOC_FLAGS) $(DOC_SRC)
	gzip -f $(DOCS)

###################
# INSTALL SECTION #
###################

# Install both interfaces
install:
	make install-lib
	make install-cli
	make install-gtk
	make install-eval
	make install-doc

# Install core library
install-lib: $(NAME_CORE)
	cp $(NAME_CORE) $(INSTALL_LIB)/$(NAME_CORE)

# Install cli interface
install-cli: $(EXEC_CLI)
	cp $(EXEC_CLI) $(INSTALL_BIN)/$(EXEC_CLI)

# Install gtk interface
install-gtk: $(EXEC_GTK)
	cp $(EXEC_GTK) $(INSTALL_BIN)/$(EXEC_GTK)

# Install eval interface
install-eval: $(EXEC_EVAL)
	cp $(EXEC_EVAL) $(INSTALL_BIN)/$(EXEC_EVAL)

# Install documentation
install-doc: doc
	@if [ -n "$(DOCS1)" ]; then cp -v $(DOCS1) $(MAN1); fi
	@if [ -n "$(DOCS3)" ]; then cp -v $(DOCS3) $(MAN3); fi

#################
# CLEAN SECTION #
#################

# Clean working directory
clean:
	rm -f $(TO_CLEAN)

# Uninstall both interfaces
uninstall:
	make uninstall-lib
	make uninstall-cli
	make uninstall-gtk
	make uninstall-eval
	make uninstall-doc

# Uninstall core library
uninstall-lib:
	rm -f $(INSTALL_LIB)/$(NAME_CORE)

# Uninstall cli interface
uninstall-cli:
	rm -f $(INSTALL_BIN)/$(EXEC_CLI)

# Uninstall gtk interface
uninstall-gtk:
	rm -f $(INSTALL_BIN)/$(EXEC_GTK)

# Uninstall eval interface
uninstall-eval:
	rm -f $(INSTALL_BIN)/$(EXEC_EVAL)

# Uninstall documentation
uninstall-doc:
	@if [ -n "$(DOCS1)" ]; then rm -vf $(addprefix $(MAN1)/,$(notdir $(DOCS1))); fi
	@if [ -n "$(DOCS3)" ]; then rm -vf $(addprefix $(MAN3)/,$(notdir $(DOCS3))); fi

################
# MISC SECTION #
################

xmlui:
	@if [ -z "`$(PYTHON) --version 2>&1`" ]; then \
		echo "$(PYTHON) not found -- required to bake gtk xml ui"; \
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
	windres $(RC_CORE) -I$(INCLUDE_DIR)/ -o $(ICON_CORE)
	windres $(RC_CLI)  -I$(INCLUDE_DIR)/ -o $(ICON_CLI)
	windres $(RC_GTK)  -I$(INCLUDE_DIR)/ -o $(ICON_GTK)
	windres $(RC_EVAL) -I$(INCLUDE_DIR)/ -o $(ICON_EVAL)

# Windows post-compilation
windows-post:
	@true

# *UNIX pre-compilation
unix-pre:
	@true

# *UNIX post-compilation
unix-post:
	@true
