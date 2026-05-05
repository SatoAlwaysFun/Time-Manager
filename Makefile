CC = gcc
CFLAGS = -Iinclude $(shell pkg-config --cflags gtk+-3.0)
LIBS = $(shell pkg-config --libs gtk+-3.0)

# Gom tất cả các file nguồn hiện có trong hình của bạn
SRC = main.c src/gui.c src/file_manager.c src/pomodoro.c src/task_manager.c src/task.c
OUT = TimeManager.exe

all:
	$(CC) $(SRC) $(CFLAGS) -o $(OUT) $(LIBS)

clean:
	rm -f $(OUT)