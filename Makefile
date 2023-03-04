CC=gcc
CFLAGS=-std=c11 -static
TARGET=./mimic
SRC=main.c tokenizer.c parser.c codegen.c verifier.c
OBJ=$(SRC:%.c=obj/%.o)

TEST_SOURCES=$(wildcard ./test/*.c)
TEST_EXECUTABLES=$(TEST_SOURCES:./test/%.c=./test/Xtmp/%.exe)
TEST_FRAMEWORK=./test/test.framework
TEST_FRAMEWORK_OBJ=$(TEST_FRAMEWORK:./test/%=./test/Xtmp/%.o)

# Compilation: 1st gen
$(TARGET): $(OBJ)
	gcc $(CFLAGS) -o $@ $^

.PHONY: debug
debug: CFLAGS+=-g -O0
debug: clean $(TARGET)

obj/%.o: %.c mimic.h
	@[ -d ./obj ] || mkdir ./obj
	gcc $(CFLAGS) -o $@ -c $<

# Compilation: 2nd gen
# TODO:

# Compilation: 3rd gen
# TODO:

# Tests
.PHONY: test
test: test_basic test_advanced;

.PHONY: test_prepair
test_prepair: ./test/Xtmp;

.PHONY: test_basic
test_basic: $(TARGET) test_prepair
	./test/basic_functionalities.sh
	@echo OK
	@echo

.PHONY: test_advanced
test_advanced: test_prepair test_basic $(TEST_EXECUTABLES) ./test/compile_failure.sh
	@for exe in $(TEST_EXECUTABLES); do \
		echo $${exe}; \
		$${exe} || exit 1; \
		echo; \
	done
	./test/compile_failure.sh
	@echo OK

.PHONY: test_clean
test_clean:
	rm -r ./test/Xtmp

./test/Xtmp:
	mkdir ./test/Xtmp

./test/Xtmp/%.exe: ./test/Xtmp/%.o $(TEST_FRAMEWORK_OBJ)
	$(CC) -o $@ $< $(TEST_FRAMEWORK_OBJ)

$(TEST_FRAMEWORK_OBJ): $(TEST_FRAMEWORK)
	gcc -o $@ -c -x c $<

./test/Xtmp/%.o: ./test/Xtmp/%.s
	gcc -o $@ -c $<

./test/Xtmp/%.s: ./test/Xtmp/%.c $(TARGET)
	$(TARGET) $< > $@

./test/Xtmp/%.c: ./test/%.c ./test/test.h
	gcc -o $@ -E -P $<

.PRECIOUS: $(TEST_EXECUTABLES:%.exe=%.s) $(TEST_EXECUTABLES:%.exe=%.c)

# Utils
.PHONY: clean
clean:
	rm -f $(TARGET) ./obj/*.o *~ tmp*

.PHONY: all
all: clean $(TARGET);
