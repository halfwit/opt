PREFIX = /usr
MANPREFIX = $(PREFIX)/share/man

CC = cc
LD = $(CC)

CFLAGS += -std=c99 -pedantic -Wall -Os
LDFLAGS += -lxcb -lxcb-util -lxcb-xkb -lxkbcommon-x11 -lxkbcommon
