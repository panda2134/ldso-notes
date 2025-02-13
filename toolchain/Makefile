CC = ccomp -finline-asm
SOURCES := main.c $(filter-out main.c,$(wildcard *.c))


.PHONY: all clean
all: ld.my.so
	cd tests && make all

ld.my.so: ${SOURCES}
	${CC} -fstruct-passing -nostdlib -c $?
	ld -shared -pie --no-dynamic-linker --entry _start -o ld.my.so $(?:.c=.o)

clean:
	rm -rf ld.my.so *.o
	cd tests && make clean
