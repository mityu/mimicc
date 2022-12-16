TESTERS=test.framework test.h
FRAMEWORK=test.framework
TEST_ADVANCED=$(wildcard *.c)

test: test_basic test_advanced;

# Test basic functionalities.
test_basic: prepair ../mimic
	./basic_functionalities.sh
	@echo OK
	@echo

# Test advanced functionalities.
test_advanced: prepair test_basic $(TESTERS) $(TEST_ADVANCED:%.c=./Xtmp/%.out)
	@for exe in $(wildcard ./Xtmp/*.out); do \
		echo $${exe}; \
		$${exe} || exit 1; \
		echo; \
	done
	./compile_failure.sh
	@echo OK

./Xtmp/%.out: ./Xtmp/%.o ./Xtmp/$(FRAMEWORK).o
	gcc -o $@ $< ./Xtmp/$(FRAMEWORK).o

./Xtmp/$(FRAMEWORK).o: $(FRAMEWORK)
	gcc -o $@ -c -x c $<

./Xtmp/%.o: ./Xtmp/%.s
	gcc -o $@ -c $<

./Xtmp/%.s: ./Xtmp/%.c ../mimic
	../mimic $< > $@

./Xtmp/%.c: %.c test.h
	gcc -o $@ -E -P $<

prepair: CC_MIMIC ./Xtmp;

./Xtmp:
	mkdir ./Xtmp

CC_MIMIC:
	cd ../ && make

clean:
	rm -r Xtmp

.PRECIOUS: $(TEST_ADVANCED:%=./Xtmp/%) $(TEST_ADVANCED:%.c=./Xtmp/%.s) $(TEST_ADVANCED:%.c=./Xtmp/%.o)
.PHONY: test test_basic test_advanced prepair clean CC_MIMIC
