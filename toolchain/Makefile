CC = ccomp -finline-asm

all: main.c
	${CC} -nostdlib -c main.c
	ld -shared -pie --no-dynamic-linker --entry _start -o ld.my.so main.o

.PHONY: clean

clean:
	rm -f ld.my.so *.o
