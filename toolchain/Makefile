CC = ccomp -finline-asm

.PHONY: all clean
all: ld.my.so
	cd tests && make all

ld.my.so: main.c utils.c syscalls.c dynload.c malloc.c walkdyn.c
	${CC} -fstruct-passing -nostdlib -c main.c utils.c syscalls.c dynload.c malloc.c walkdyn.c
	ld -shared -pie --no-dynamic-linker --entry _start -o ld.my.so main.o utils.o syscalls.o dynload.o malloc.o walkdyn.o

clean:
	rm -rf ld.my.so
	cd tests && make clean
