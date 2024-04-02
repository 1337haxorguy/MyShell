CC = gcc
CFLAGS = -Wall -fsanitize=address -std=c99

all: myShell

myShell: myShell.c
	$(CC) $(CFLAGS) myShell.c -o mysh

clean:
	rm mysh
