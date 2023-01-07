CFLAGS=-std=c11 -static
TARGET=mimic
SRC=main.c tokenizer.c parser.c codegen.c verifier.c
OBJ=$(SRC:%.c=obj/%.o)
$(TARGET): $(OBJ)
	gcc $(CFLAGS) -o $@ $^

obj/%.o: %.c mimic.h
	@[ -d ./obj ] || mkdir ./obj
	gcc $(CFLAGS) -o $@ -c $<

debug:
	make all CFLAGS="$(CFLAGS) -g -O0"

test: $(TARGET) test/Makefile
	cd test && make

clean:
	rm -f $(TARGET) ./obj/*.o *~ tmp*

all: clean $(TARGET);

.PHONY: test clean debug all
