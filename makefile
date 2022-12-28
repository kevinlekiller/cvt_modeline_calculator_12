CC=gcc
CFLAGS=-O2 -Wall
cvt12: cvt12.c
	$(CC) $(CFLAGS) cvt12.c -o cvt12 -lm
