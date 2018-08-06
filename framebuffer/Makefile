all: main

main: main.o framebuffer.o

CFLAGS +=-I./framebuffer.h

%.o: %.c
	$(CC) -c $^ -o $@
