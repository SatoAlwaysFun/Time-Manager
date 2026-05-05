CC = gcc
CFLAGS = -Iinclude `pkg-config --cflags gtk+-3.0`
LIBS = `pkg-config --libs gtk+-3.0`

SRC = main.c src/gui.c
OUT = main.exe

all:
	$(CC) $(SRC) $(CFLAGS) -o $(OUT) $(LIBS) 