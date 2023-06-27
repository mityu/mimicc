## mimicc: MItyu's MIni C Compiler

A toy, self-hosted (subset of) C Compiler for Linux on x86\_64.

### Supported functionalities

#### Type

- `char`
- `int`
- Array
- Pointer (including function pointer)
- Struct
- Enum
- Typedef

#### Function

- Global function
- `static` attribute
- `extern` attribute
- Function call with over 6 arguments
- Variadic argument
    - `va_start` and `va_end` is available. (`va_arg` is not supported yet.)

Note that passing/returning struct itself is not supported.

#### Variable

- Global variable
- Local variable
- `static` attribute
- `extern` attribute
- Variable initializer (primitive types, structs, arrays)
- Type checking (incomplete)
    - So sometimes `mimicc` accepts incorrect C program and rejects correct C program.

#### Literals

- Decimal, hexadecimal, octal number
- Characters
- String literals
- Escape sequence
    - `\'`, `\"`, `\\`, `\?`, `\a`, `\b`, `\f`, `\n`, `\r`, `\t`, `\v`, and `\0`

#### Control syntax

- `if`, `else if` and `else`
- `for` (with variable declaration at initializer statement support: `for (int i = 0; ...)`)
- `while`
- `switch`, `case` and `default`
- `break`
- `continue`
- `return`

#### Statements

- Compound literal for struct (it for array is not yet)
- Type cast
- One-line comment (`// ...`)
- Multi-line comment (`/* ... */`)

#### operators

- `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, and `||`
- `+=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, and `>>=`
- `++` and `--` of both prefix and postfix
- `+`, `*`, `/`, and `%`
- `&`, `|`, `^`, `<<`, and `>>` (Bit operation)
- `!` ("not" operator)
- Addressing `&`
- Dereferencing `*`
- Unary `+` and `-` (e.g. `+3` and `-5`)
- `sizeof`
- `.` and `->`
- `()` (function call)
- `[]` (array accessing)
- Ternary operator (`condition ? expr-true : expr-false`)
- `,` (`expr, expr, expr, ...`)

#### Preprocess

- `#include <header>` and `#include "header"`
- `#if`, `#elif`, `#end`, and `#ifdef`
   - Nesting them is also OK.
- Macros
   - Object-like macro (e.g. `#define BUFSIZE (64)`)
   - Function-like macro (e.g. `#define SIZEOF(ar) (sizeof(ar) / sizeof(ar[0]))`)
   - `#undef`
   - Note that `mimicc` doesn't have infinite macro expansion guard, so `mimicc` won't return with recursive macro like this:

```
#define REC REC
```

```
#define MUTUAL_REC1(arg) MUTUAL_REC2(arg)
#define MUTUAL_REC2(arg) MUTUAL_REC1(arg)
```


### Build menus

```
# Build mimicc compiler with gcc. The binary will available at ./mimicc.
$ make

# Build 2nd gen mimicc compiler.  The binary will available at ./test/self/mimicc.
$ make self

# Build 3rd gen mimicc compiler.  The binary will available at ./test/selfself/mimicc.
$ make selfself
```


### Test menus

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

### Compile your C program

```
$ cd path/to/this/repo
$ ./mimicc -o <out-asm-path> <in-c-program-path>
$ gcc -o <out-binary-path> <out-asm-path>
```

### Acknowledgements

This project is heavily, heavily inspired by this web book.  Great thanks:
- 「低レイヤを知りたい人のためのCコンパイラ作成入門」(https://www.sigbus.info/compilerbook)
