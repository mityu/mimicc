CFLAGS=-std=c11 -g -static
TARGET=mimic
SRC=main.c parser.c codegen.c verifier.c
OBJ=$(SRC:%.c=obj/%.o)
$(TARGET): $(OBJ)
	gcc -o $@ $^

obj/%.o: %.c mimic.h
	@[ -d ./obj ] || mkdir ./obj
	gcc -o $@ -c $<

test: $(TARGET) test/Makefile
	cd test && make

clean:
	rm -f $(TARGET) ./obj/*.o *~ tmp*


.PHONY: test clean
