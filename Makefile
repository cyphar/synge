CC ?= gcc
EXEC_BASE = synge

CFLAGS = -Wall -pedantic -std=c99 -fsigned-char

SHR_LFLAGS = -lm
CLI_LFLAGS =
GTK_LFLAGS =

SHR_SRC = src/stack.c src/calculator.c
CLI_SRC = src/cli.c
GTK_SRC = src/gtk.c

DEPS = src/stack.h src/calculator.h

VERSION = 1.0.1
CLI_VERSION =
GTK_VERSION =

all: $(SHR_SRC) $(CLI_SRC) $(GTK_SRC) $(DEPS)
	make cli
	make gtk

cli: $(SHR_SRC) $(CLI_SRC) $(DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) $(CFLAGS) -o $(EXEC_BASE)-cli \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"'
	strip $(EXEC_BASE)-cli

gtk: $(SHR_SRC) $(GTK_SRC) $(DEPS)
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) $(CFLAGS) -o $(EXEC_BASE)-gtk \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"'
	strip $(EXEC_BASE)-gtk

debug: $(SHR_SRC) $(CLI_SRC) $(GTK_SRC) $(DEPS)
	make debug-cli
	make debug-gtk

debug-cli: $(SHR_SRC) $(CLI_SRC) $(DEPS)
	$(CC) $(SHR_SRC) $(CLI_SRC) $(SHR_LFLAGS) $(CLI_LFLAGS) $(CFLAGS) -o $(EXEC_BASE)-cli \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_CLI_VERSION__='"$(CLI_VERSION)"'

debug-gtk: $(SHR_SRC) $(GTK) $(DEPS)
	$(CC) $(SHR_SRC) $(GTK_SRC) $(SHR_LFLAGS) $(GTK_LFLAGS) $(CFLAGS) -o $(EXEC_BASE)-gtk \
		-g -O0 -D__DEBUG__ \
		-D__SYNGE_VERSION__='"$(VERSION)"' \
		-D__SYNGE_GTK_VERSION__='"$(GTK_VERSION)"'

clean:
	rm -f $(EXEC_BASE)-cli $(EXEC_BASE)-gtk
