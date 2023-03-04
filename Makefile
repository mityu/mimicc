CFLAGS=-std=c11 -static
TARGET=./mimic
SRC=main.c tokenizer.c parser.c codegen.c verifier.c
OBJ=$(SRC:%.c=obj/%.o)

TESTCC=$(TARGET)
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

.PHONY: clean
clean:
	rm -f $(TARGET) ./obj/*.o *~ tmp*

.PHONY: all
all: clean $(TARGET);

# Compilation: 2nd gen
# TODO:

# Compilation: 3rd gen
# TODO:

# Tests

# Test by 1st gen compiler
.PHONY: test
test: test_basic test_advanced test_advanced_errors;

.PHONY: test_prepair
test_prepair: ./test/Xtmp;


# Test by gcc (To find bugs in tests)
.PHONY: test_test
test_test: test_prepair test_test_prepair test_advanced;

.PHONY: test_test_prepair
test_test_prepair:
	$(eval TESTCC=gcc)
	# Currently "sizeof(size_t)" is 8 for gcc and 4 for mimic, so test/sizeof.c
	# fails.  Disable it now.
	$(eval TEST_EXECUTABLES:=$(filter-out %/sizeof.exe,$(TEST_EXECUTABLES)))


.PHONY: test_basic
test_basic: $(TARGET) test_prepair
	./test/basic_functionalities.sh
	@echo OK
	@echo

.PHONY: test_advanced
test_advanced: test_prepair test_clean test_basic $(TEST_EXECUTABLES)
	@for exe in $(TEST_EXECUTABLES); do \
		echo $${exe}; \
		$${exe} || exit 1; \
		echo; \
	done
	@echo OK
	@echo

.PHONY: test_advanced_errors
test_advanced_errors: test_prepair ./test/compile_failure.sh
	./test/compile_failure.sh
	@echo OK
	@echo

.PHONY: test_clean
test_clean:
	rm -r ./test/Xtmp/*

./test/Xtmp:
	mkdir ./test/Xtmp

./test/Xtmp/%.exe: ./test/Xtmp/%.o $(TEST_FRAMEWORK_OBJ)
	gcc -o $@ $< $(TEST_FRAMEWORK_OBJ)

$(TEST_FRAMEWORK_OBJ): $(TEST_FRAMEWORK)
	gcc -o $@ -c -x c $<

./test/Xtmp/%.o: ./test/Xtmp/%.s
	gcc -o $@ -c $<

./test/Xtmp/%.s: ./test/Xtmp/%.c
	$(TESTCC) -o $@ -S $<

./test/Xtmp/%.c: ./test/%.c ./test/test.h
	gcc -o $@ -E -P $<

.PRECIOUS: $(TEST_EXECUTABLES:%.exe=%.s) $(TEST_EXECUTABLES:%.exe=%.c)
