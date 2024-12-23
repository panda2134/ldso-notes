all: main.c start.s
	ccomp -finline-asm -nostdlib -c main.c
	cpp start.s | as -o start.o -
	ld -pie --no-dynamic-linker -o ld.my.so start.o main.o

debug: main.c start.s
	gcc -g -nostdlib -c main.c
	cpp start.s | as -o start.o -
	ld -pie --no-dynamic-linker -o ld.my.so start.o main.o

.PHONY: clean

clean:
	rm -f ld.my.so *.o
