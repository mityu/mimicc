main: main.s
	gcc -o main main.s

main.s: main.c mimic_cc
	../mimic -o main.s -S main.c

mimic_cc:
	cd ../ && make

asm: run-asm;
run-asm:
	gcc -o main main.s
	./main

run: main
	./main

.PHONY: run run-asm mimic_cc
