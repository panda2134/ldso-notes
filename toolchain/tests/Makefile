CC = /usr/local/musl/bin/musl-gcc -g

.PHONY: all clean

all: test.out libtester.so
clean:
	rm -f libtester.so test.out

libtester.so: libtester.c libtester.h
	${CC} -shared -fPIC -fno-stack-protector -o libtester.so libtester.c

test.out: test.c libtester.so
	${CC} -no-pie -fno-stack-protector -Wl,-rpath,$(shell pwd) -L. -ltester -o test.out test.c
	patchelf --set-interpreter $(shell cd ..; pwd)/ld.my.so test.out
