CFLAGS=-std=c11 -static
TARGET=./mimicc
SRC=main.c tokenizer.c preproc.c parser.c codegen.c verifier.c
OBJ=$(SRC:%.c=obj/%.o)

HOME_SELF=./test/self
TARGET_SELF=$(TARGET:./%=$(HOME_SELF)/%)
OBJ_SELF=$(OBJ:obj/%=$(HOME_SELF)/%)
ASM_SELF=$(OBJ_SELF:%.o=%.s)

HOME_SELFSELF=./test/selfself
TARGET_SELFSELF=$(TARGET:./%=$(HOME_SELFSELF)/%)
OBJ_SELFSELF=$(OBJ:obj/%=$(HOME_SELFSELF)/%)
ASM_SELFSELF=$(OBJ_SELFSELF:%.o=%.s)

INCLUDE=./include
HEADERS=$(wildcard $(INCLUDE)/*)

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

obj/%.o: %.c mimicc.h
	@[ -d ./obj ] || mkdir ./obj
	gcc $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -f $(TARGET) ./obj/*.o *~ tmp*

.PHONY: all
all: clean $(TARGET);

.PHONY: test
test: $(TARGET) test_basic test_advanced test_advanced_errors;


# Compilation: 2nd gen
$(TARGET_SELF): $(TARGET) self_prepair $(OBJ_SELF)
	gcc -no-pie -o $@ $(OBJ_SELF)

.PHONY: self
self: $(TARGET_SELF);

.PHONY: self_prepair
self_prepair: $(HOME_SELF);

.PHONY: self_clean
self_clean:
	rm $(HOME_SELF)/* $(HOME_SELF)/Xtmp/*

.PHONY: self_test
self_test: test_self;

.PHONY: test_self
test_self: $(TARGET_SELF) test_prepair test_self_prepair \
	test_advanced test_advanced_errors;

.PHONY: test_self_prepair
test_self_prepair:
	$(eval TESTCC=$(TARGET_SELF))

$(HOME_SELF):
	mkdir $(HOME_SELF)

$(HOME_SELF)/%.o: $(HOME_SELF)/%.s
	gcc -o $@ -c $<

$(HOME_SELF)/%.s: $(HOME_SELF)/%.c $(TARGET)
	$(TARGET) -o $@ -S $<

$(HOME_SELF)/%.c: ./%.c mimicc.h $(HEADERS)
	gcc -o $@ -I$(INCLUDE) -E -P $<


# Compilation: 3rd gen
$(TARGET_SELFSELF): $(TARGET_SELF) selfself_prepair $(OBJ_SELFSELF)
	gcc -no-pie -o $@ $(OBJ_SELFSELF)

.PHONY: selfself
selfself: $(TARGET_SELFSELF);

.PHONY: selfself_prepair
selfself_prepair: $(HOME_SELFSELF);

.PHONY: selfself_clean
selfself_clean:
	rm $(HOME_SELFSELF)/* $(HOME_SELFSELF)/Xtmp/*

.PHONY: selfself_test
selfself_test: test_selfself;

.PHONY: test_selfself
test_selfself: $(TARGET_SELFSELF) test_prepair test_selfself_prepair \
	test_advanced test_advanced_errors test_selfself_diff;

.PHONY: test_selfself_prepair
test_selfself_prepair:
	$(eval TESTCC=$(TARGET_SELFSELF))

.PHONY: test_selfself_diff
test_selfself_diff: $(ASM_SELF) $(ASM_SELFSELF)
test_selfself_diff: $(addprefix test_selfself_diff-,$(notdir $(ASM_SELFSELF:%.s=%)))
	@echo OK
	@echo

.PHONY: test_selfself_diff-%
test_selfself_diff-%: $(HOME_SELF)/%.s $(HOME_SELFSELF)/%.s
	diff -u $^

$(HOME_SELFSELF):
	mkdir $(HOME_SELFSELF)

$(HOME_SELFSELF)/%.o: $(HOME_SELFSELF)/%.s
	gcc -o $@ -c $<

$(HOME_SELFSELF)/%.s: $(HOME_SELFSELF)/%.c $(TARGET_SELF)
	$(TARGET_SELF) -o $@ -S $<

$(HOME_SELFSELF)/%.c: ./%.c mimicc.h $(HEADERS)
	gcc -o $@ -I$(INCLUDE) -E -P $<


# Test by gcc (To find bugs in tests)
.PHONY: test_test
test_test: test_test_prepair test_prepair test_advanced;

.PHONY: test_test_prepair
test_test_prepair:
	$(eval TESTCC=gcc)
	# Currently "sizeof(size_t)" is 8 for gcc and 4 for mimicc, so test/sizeof.c
	# fails.  Disable it now.
	$(eval TEST_EXECUTABLES:=$(filter-out %/sizeof.exe,$(TEST_EXECUTABLES)))


# Test system
.PHONY: test_prepair
test_prepair: ./test/Xtmp;

.PHONY: test_basic
test_basic: ABSPATH=$(abspath $(TESTCC))
test_basic: CCPATH=$(shell [ -f '$(ABSPATH)' ] && echo '$(ABSPATH)' || echo '$(TESTCC)')
test_basic: test_prepair
	TESTCC=$(CCPATH) ./test/basic_functionalities.sh
	@echo OK
	@echo

.PHONY: test_advanced
test_advanced: test_prepair test_clean test_basic $(TEST_EXECUTABLES) \
	$(addprefix test_advanced_run-,$(notdir $(TEST_EXECUTABLES:%.exe=%)))
	@echo OK
	@echo

.PHONY: test_advanced_run-%
test_advanced_run-%: ./test/Xtmp/%.exe
	$<

.PHONY: test_advanced_errors
test_advanced_errors: ABSPATH=$(abspath $(TESTCC))
test_advanced_errors: CCPATH=$(shell [ -f '$(ABSPATH)' ] && echo '$(ABSPATH)' || echo '$(TESTCC)')
test_advanced_errors: test_prepair ./test/compile_failure.sh
	TESTCC=$(CCPATH) ./test/compile_failure.sh
	@echo OK
	@echo

.PHONY: test_clean
test_clean:
	rm -r ./test/Xtmp/*

.PHONY: test_all
test_all:
	$(MAKE) test
	$(MAKE) test_self
	$(MAKE) test_selfself

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
