CC=gcc
CFLAGS=-std=gnu99 -g -W -Wall -pedantic
LDFLAGS=-lm

all:
	$(CC) $(CFLAGS) hgk.c -o hgk $(LDFLAGS)

clean:
	rm hgk
