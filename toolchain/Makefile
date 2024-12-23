all: main.c start.s
	ccomp -finline-asm -nostdlib -static -c main.c
	cpp start.s | as -o start.o -
	ld start.o main.o

debug: main.c start.s
	gcc -g -nostdlib -static -c main.c
	cpp start.s | as -o start.o -
	ld start.o main.o

.PHONY: clean

clean:
	rm -f a.out *.o
