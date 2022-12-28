CC=gcc
CFLAGS=-O2 -lm -Wall
all: cvt12
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
