CFLAGS=-std=c11 -g -static
TARGET=mimic
SRC=main.c parser.c codegen.c
OBJ=$(SRC:.c=.o)
$(TARGET): $(OBJ)
	gcc -o $@ $^

%.o: %.c mimic.h
	gcc -o $@ -c $<

test: $(TARGET)
	./test.sh

clean:
	rm -f $(TARGET) *.o *~ tmp*


.PHONY: test clean
