CFLAGS=-std=c11 -g -static
TARGET=mimic
$(TARGET): main.c
	gcc -o $@ $^

test: $(TARGET)
	./test.sh

clean:
	rm -f $(TARGET) *.o *~ tmp*

.PHONY: test clean
