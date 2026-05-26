# ── Time Manager – MinGW / MSYS2 Makefile ──────────────────────────
CC      = gcc
TARGET  = TimeManager.exe

GTK_CFLAGS  := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS    := $(shell pkg-config --libs   gtk+-3.0)

CFLAGS  = -Wall -Wextra -std=c11 -Iinclude $(GTK_CFLAGS)
LDFLAGS = $(GTK_LIBS) -mwindows

SRCS    = main.c \
          src/gui.c \
          src/pomodoro.c \
          src/sorttask.c \
          src/task_manager.c \
          src/file_manager.c

OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q $(subst /,\,$(OBJS)) $(TARGET) 2>nul || true

.PHONY: all clean