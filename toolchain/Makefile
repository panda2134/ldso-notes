CC = ccomp -finline-asm

.PHONY: all clean
all: ld.my.so
	cd tests && make all

ld.my.so: main.c utils.c syscalls.c dynload.c malloc.c walkdyn.c global.c
	${CC} -fstruct-passing -nostdlib -c main.c utils.c syscalls.c dynload.c malloc.c walkdyn.c global.c
	ld -shared -pie --no-dynamic-linker --entry _start -o ld.my.so main.o utils.o syscalls.o dynload.o malloc.o walkdyn.o global.o

clean:
	rm -rf ld.my.so *.o
	cd tests && make clean
