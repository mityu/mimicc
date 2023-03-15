## mimicc: MItyu's MIni C Compiler

A toy, self-hosted (subset of) C Compiler.

#### Build menus

```
# Build mimicc compiler with gcc. The binary will available at ./mimicc.
$ make

# Build 2nd gen mimicc compiler.  The binary will available at ./test/self/mimicc.
$ make self

# Build 3rd gen mimicc compiler.  The binary will available at ./test/selfself/mimicc.
$ make selfself
```


#### Test menus

```
# Run tests for 1st gen mimicc compiler.
$ make test

# Run tests for 2nd gen mimicc compiler.
$ make test_self  or  $ make self_test

# Run tests for 3rd gen mimicc compiler.  Also check there's no differencies
# between assembly of 2nd gen mimicc and of 3rd gen mimicc.
$ make test_selfself or  $ make selfself_test

# Run all tests at once
$ make test_all

```

#### Compile your C program

```
$ cd path/to/this/repo
$ ./mimicc -o <out-asm-path> <in-c-program-path>
$ gcc -o <out-binary-path> <out-asm-path>
```

#### Acknowledgements

This project is heavily, heavily inspired by this web book.  Great thanks:
- 「低レイヤを知りたい人のためのCコンパイラ作成入門」(https://www.sigbus.info/compilerbook)
