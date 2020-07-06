CFLAGS	+=-I/usr/local/include/luajit-2.1 -I/usr/local/include

.PHONY: all test shared.so clean

all: test shared.so

test: test.c ltable.c
	gcc -g ltable.c test.c -o test

shared.so: shared.c ltable.c
	$(CC) -shared -o shared.so ${CFLAGS} shared.c ltable.c -luv -L/usr/local/lib -lluajit-5.1

clean:
	rm -rf test shared.so
