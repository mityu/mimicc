main: main.s
	gcc -o main main.s

main.s: main.c mimic_cc
	../mimic main.c > main.s

mimic_cc:
	cd ../ && make

asm: run-asm;
run-asm:
	gcc -o main main.s
	./main

run: main
	./main

.PHONY: run run-asm mimic_cc
