CFLAGS=-std=c11 -static
TARGET=./mimicc
TARGET_DEBUG=$(TARGET)_debug
SRC=main.c tokenizer.c preproc.c parser.c codegen.c verifier.c
OBJ=$(SRC:%.c=obj/%.o)
INCLUDE=./include
HEADERS=$(wildcard $(INCLUDE)/*)

HOME_SELF=./test/self
TARGET_SELF=$(TARGET:./%=$(HOME_SELF)/%)
OBJ_SELF=$(OBJ:obj/%=$(HOME_SELF)/%)
ASM_SELF=$(OBJ_SELF:%.o=%.s)
INCLUDE_SELF=$(INCLUDE:./%=$(HOME_SELF)/%)

HOME_SELFSELF=./test/selfself
TARGET_SELFSELF=$(TARGET:./%=$(HOME_SELFSELF)/%)
OBJ_SELFSELF=$(OBJ:obj/%=$(HOME_SELFSELF)/%)
ASM_SELFSELF=$(OBJ_SELFSELF:%.o=%.s)
INCLUDE_SELFSELF=$(INCLUDE:./%=$(HOME_SELFSELF)/%)

TESTCC=$(TARGET)
TEST_SOURCES=$(wildcard ./test/*.c)
TEST_EXECUTABLES=$(TEST_SOURCES:./test/%.c=./test/Xtmp/%.exe)
TEST_FRAMEWORK=./test/test.framework
TEST_FRAMEWORK_OBJ=$(TEST_FRAMEWORK:./test/%=./test/Xtmp/%.o)

ifeq ($(shell uname),Darwin)
d%:
	docker run --rm -v ./:/mimicc -w /mimicc mimicc make $*

.PHONY: docker-shell
docker-shell:
	docker run --rm -it -v ./:/home/user/mimicc -w /home/user/mimicc mimicc

.PHONY: docker-image
docker-image: | Dockerfile
	docker build -t mimicc ./
endif

# Compilation: 1st gen
$(TARGET): $(OBJ)
	gcc $(CFLAGS) -o $@ $^

obj/%.o: %.c mimicc.h
	@[ -d ./obj ] || mkdir ./obj
	gcc $(CFLAGS) -o $@ -c $<

.PHONY: debug
debug: $(TARGET_DEBUG);

$(TARGET_DEBUG): $(OBJ:.o=.debug.o)
	gcc $(CFLAGS) -g -O0 -o $@ $^

obj/%.debug.o: %.c mimicc.h
	@[ -d ./obj ] || mkdir ./obj
	gcc $(CFLAGS) -g -O0 -o $@ -c $<

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
self_prepair: $(HOME_SELF) $(INCLUDE_SELF);

.PHONY: self_clean
self_clean:
	rm $(HOME_SELF)/*

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

$(INCLUDE_SELF): $(INCLUDE)
	[ -e "$@" ] || ln -snv $$(pwd)/$< $$(pwd)/$@

$(HOME_SELF)/%.o: $(HOME_SELF)/%.s
	gcc -o $@ -c $<

$(HOME_SELF)/%.s: ./%.c $(TARGET) mimicc.h $(HEADERS)
	$(TARGET) -o $@ -S $<


# Compilation: 3rd gen
$(TARGET_SELFSELF): $(TARGET_SELF) selfself_prepair $(OBJ_SELFSELF)
	gcc -no-pie -o $@ $(OBJ_SELFSELF)

.PHONY: selfself
selfself: $(TARGET_SELFSELF);

.PHONY: selfself_prepair
selfself_prepair: $(HOME_SELFSELF) $(INCLUDE_SELFSELF);

.PHONY: selfself_clean
selfself_clean:
	rm $(HOME_SELFSELF)/*

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

$(INCLUDE_SELFSELF): $(INCLUDE)
	[ -e "$@" ] || ln -snv $$(pwd)/$< $$(pwd)/$@

$(HOME_SELFSELF)/%.s: ./%.c $(TARGET_SELF) mimicc.h
	$(TARGET_SELF) -o $@ -S $<


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

./test/Xtmp/define.s: ./test/define.c
	$(TESTCC) -o $@ -S $<

./test/Xtmp/ifdef_directive.s: ./test/ifdef_directive.c
	$(TESTCC) -o $@ -S $<

./test/Xtmp/if_directive.s: ./test/if_directive.c
	$(TESTCC) -o $@ -S $<

./test/Xtmp/include.s: ./test/include.c ./test/include.header
	$(TESTCC) -o $@ -S $<

./test/Xtmp/preproc.s: ./test/preproc.c
	$(TESTCC) -o $@ -S $<

./test/Xtmp/%.s: ./test/Xtmp/%.c
	$(TESTCC) -o $@ -S $<

./test/Xtmp/%.c: ./test/%.c ./test/test.h
	gcc -o $@ -E -P $<

.PRECIOUS: $(TEST_EXECUTABLES:%.exe=%.s) $(TEST_EXECUTABLES:%.exe=%.c)

.PHONY: format
format:
	clang-format -i *.[ch] include/*.h
