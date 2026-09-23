CC      = gcc
PKGS    = xcb xcb-keysyms xcb-cursor xcb-render
# -lm нужен только на этапе линковки (nanosvgrast.h использует math.h);
# в CFLAGS он раньше игнорировался, т.к. флаги -l там не действуют на
# этапе компиляции (-c). libx11-dev/x11proto-dev больше не нужны:
# XK_*-константы (snap.c/altTab.c/config.c) взяты в src/keysyms_min.h
# своим списком, без зависимости от заголовков X11.
CFLAGS  = -std=c11 -Wall -Wextra -O2 $(foreach p,$(PKGS),$(shell pkg-config --cflags $(p) 2>/dev/null))
LDFLAGS = $(foreach p,$(PKGS),$(shell pkg-config --libs $(p) 2>/dev/null)) -lm

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = QSWM

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)
