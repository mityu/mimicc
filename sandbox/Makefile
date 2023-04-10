main: main.s
	gcc -o main main.s

main.s: main.c mimicc-cc
	../mimicc -o main.s -S main.c

mimicc-cc:
	@cd ../ && make

gcc: main.c
	gcc -o main main.c

2nd: mimicc-2nd-cc compile-by-2nd main;
2nd-run: 2nd
	./main
run-2nd: 2nd-run;

compile-by-2nd: main.c
	../test/self/mimicc -o main.s -S main.c

mimicc-2nd-cc:
	@cd ../ && make self

asm: run-asm;
run-asm:
	gcc -o main main.s
	./main

run: main
	./main

.PHONY: run asm run-asm mimicc-cc 2nd mimicc-2nd-cc 2nd-run run-2nd compile-by-2nd
