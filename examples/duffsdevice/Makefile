CC=../../mimicc

main: main.s  ## Build executable (default)
	gcc -o $@ $^

main.s: main.c $(CC)
	$(CC) -o $@ $<

clean:  ## Clean up build files
	$(RM) main main.s

run: main  ## Build and run executable
	./main

all: clean main;  ## Re-build executable

help:  ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "%s\t%s\n", $$1, $$2}'

.PHONY: clean run all help
