CC = ccomp -finline-asm

all: ld.my.so tests/test.out

ld.my.so: main.c utils.c
	${CC} -nostdlib -c main.c utils.c
	ld -shared -pie --no-dynamic-linker --entry _start -o ld.my.so main.o utils.o

tests/test.out: ld.my.so tests/test.c
	gcc -nostdlib -o tests/test.out tests/test.c
	patchelf --set-interpreter ${PWD}/ld.my.so tests/test.out

.PHONY: clean

clean:
	rm -rf ld.my.so {.,tests}/*.o {.,tests}/*.out
