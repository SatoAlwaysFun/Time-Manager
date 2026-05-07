CC     = gcc
CFLAGS = -Iinclude $(shell pkg-config --cflags gtk+-3.0) -Wall -Wextra -std=c11
LIBS   = $(shell pkg-config --libs gtk+-3.0)

SRC = main.c src/gui.c src/pomodoro.c src/task.c src/task_manager.c src/file_manager.c
OUT = TimeManager.exe

.PHONY: all clean

all:
	$(CC) $(SRC) $(CFLAGS) -o $(OUT) $(LIBS)

clean:
	rm -f $(OUT)