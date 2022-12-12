#!/bin/bash

assert() {
  expected="$1"
  input="$2"

  ./mimic "$input" > tmp.s
  cc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert_fcall() {
  expected="$1"
  input="$2"
  restcode="$3"

  ./mimic "$input" > tmp1.s
  gcc -c -o tmp1.o tmp1.s
  gcc -c -o tmp2.o -x c <(echo "$restcode")
  gcc -o tmp tmp1.o tmp2.o
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert_fail() {
  input="$1"
  ./mimic "$1"
  if [ $? -ne 1 ]; then
    echo "Other than 1 returned."
    exit 1
  fi
}

assert 42 'int main(){ return 42;}'
assert 4 'int main(){ return 5-3+2;}'
assert 6 'int main(){ return 5+3-2;}'
assert 6 'int main(){ return  5 + 3 - 2 ;}'
assert 3 'int main(){ return 6 / 2;}'
assert 3 'int main(){ return 7 / 2;}'
assert 3 'int main(){ return 8 % 5;}'
assert 8 'int main(){ return 10-4/2;}'
assert 18 'int main(){ return  4 * 3 + 3 * 2;}'
assert 11 'int main(){ return 5+3*2;}'
assert 10 'int main(){ return 15/3*2;}'
assert 10 'int main(){ return 15*2/3;}'
assert 16 'int main(){ return (5+3)*2;}'
assert 17 'int main(){ return (5+3)*2+1;}'
assert 24 'int main(){ return (5+3)*(2+1);}'
assert 4 'int main(){ return (5+3)/2;}'
assert 5 'int main(){ return (5+3)/2+1;}'
assert 2 'int main(){ return (5+3)/(2+1);}'
assert 10 'int main(){ return -10+20;}'
assert 10 'int main(){ return -5*+10+60;}'
assert 75 'int main(){ return -(-5*+15);}'
assert 1 'int main(){ return 1 == 1;}'
assert 0 'int main(){ return 0 == 1;}'
assert 1 'int main(){ return 0 != 1;}'
assert 0 'int main(){ return 1 != 1;}'
assert 1 'int main(){ return 1 < 2;}'
assert 0 'int main(){ return 3 < 2;}'
assert 1 'int main(){ return 1 <= 2;}'
assert 1 'int main(){ return 2 <= 2;}'
assert 0 'int main(){ return 3 <= 2;}'
assert 0 'int main(){ return 1 > 2;}'
assert 1 'int main(){ return 3 > 2;}'
assert 0 'int main(){ return 1 >= 2;}'
assert 1 'int main(){ return 2 >= 2;}'
assert 1 'int main(){ return 3 >= 2;}'
assert 10 'int main(){ int a; a = 10; return a;}'
assert 75 'int main(){ int a; a = -(-5*+15); return a;}'
assert 10 'int main(){ int a; int b; a = 10; b = a; return b;}'
assert 10 'int main(){ int a; int b; a = b = 10; return a;}'
assert 10 'int main(){ int a; int b; a = b = 10; return b;}'
assert 13 'int main(){ int a; int t; a = 3; t = 10; return a + t;}'
assert 30 'int main(){ int a; int t; a = 3; t = 10; return a * t;}'
assert 1 'int main(){ int a; int t; a = 3; t = 10; return a < t;}'
assert 0 'int main(){ int a; int t; a = 3; t = 10; return a > t;}'
assert 10 'int main(){ int foo; foo = 10; return foo;}'
assert 10 'int main(){ int foo; int bar; foo = 10; bar = 20; return foo;}'
assert 20 'int main(){ int foo; int bar; foo = 10; bar = 20; return bar;}'
assert 10 'int main(){ int foo; int bar; foo = 2; bar = 5; return foo * bar;}'
assert 5 'int main(){ int foo; foo = 2; foo = 5; return foo;}'
assert 5 'int main(){ int foo; int bar; foo = 5; bar = foo; return bar;}'
assert 1 'int main(){ int foo; int bar; foo = 5; bar = (foo * foo == 25); return bar;}'
assert 1 'int main(){ int foo; int bar; bar = ((foo = 5) == 5); return bar;}'
assert 11 'int main() {int a; int b; int c; int d; a=4; b=3; c=2; d=1; a+=b+=c+=d+=1; return a;}'
assert 2 'int main() {int a; int b; int c; int d; a=4; b=3; c=2; d=1; a-=b-=c-=d; return a;}'
assert 15 'int main() {int a, b; a = 5; b = 10; return a + b;}'
assert 11 'int main() {int n; n = 10; return ++n;}'
assert 9 'int main() {int n; n = 10; return --n;}'
assert 10 'int main() {int n; n = 10; return n++;}'
assert 10 'int main() {int n; n = 10; return n--;}'
assert 11 'int main() {char a, b; a = 3; b = 8; return (a + b);}'
assert 1 'int main() {char a, b; a = 3; b = 8; return (a + b) == 11;}'
assert 42 'int main(){ return 42; return 10;}'
assert 42 'int main(){ if (1) return 42; return 10;}'
assert 10 'int main(){ if (0) return 42; return 10;}'
assert 10 'int main(){ if (1) if (0) return 42; return 10;}'
assert 42 'int main(){ if (1) if (1) return 42; return 10;}'
assert 20 'int main(){ if (0) return 10; else return 20; return 30;}'
assert 6 'int main(){ int a; int i; a = 0; for (i=0; i<3; i=i+1) a = a + i + 1; return a;}'
assert 10 'int main(){ for (;;) return 10;}'
assert 10 'int main(){ for (1;;) return 10;}'
assert 10 'int main(){ for (;1;) return 10;}'
assert 10 'int main(){ for (;;1) return 10;}'
assert 10 'int main(){ while (1) return 10;}'
assert 10 'int main(){ while (0) return 3; return 10;}'
assert 50 'int main(){ int a; a = 0; while (a < 1) while (a < 2) a = a+1; return 50;}'
assert 50 'int main(){ {int a, b; a = 1; b = 2; return 50;}}'
assert 50 'int main(){ {{int a, b; a = 1; b = 2;} return 50;}}'
assert 89 'int main(){ int a, b, i; a=1; b=1; for (i=0; i<9; ++i) {int tmp;tmp=a;a=a+b;b=tmp;} return a;}'
assert 10 'int main(){ if (0) {return 1;} else if (1) {return 10;} else {return 2;}}'
assert 10 'int main(){ if (0) {return 1;} else if (0) {return 2;} else if (1) {return 10;} else {return 3;}}'
assert 10 'int main(){ if (0) {return 1;} else if (0) {return 2;} else {return 10;}}'
assert 10 'int f(); int f() {return 10;} int main() {return f();}'
assert_fcall 42 'int f(); int main() {return f();}' 'int f(void) {return 42;}'
assert_fcall 1 'int f(int a, int b); int main() {return f(1, 2);}' 'int f(int a, int b) {return a == 1 && b == 2;}'
assert_fcall 8 'int f(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8); int main() {return f(1, 2, 3, 4, 5, 6, 7, 8);}' 'int f(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8) { return n8;}'
assert_fcall 42 'int f(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8); int main() {return f(1, 2, 3, 4, 5, 6, f(1, 2, 3, 4, 5, 6, 7, 8), 42);}' 'int f(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8) { return n8;}'
assert 32 'int f(int n1, int n2, int n3, int n4, int n5, int n6, int n7) {int a; a = 25; return n7 + a;} int main() {f(1, 2, 3, 4, 5, 6, 7); return f(1, 2, 3, 4, 5, 6, 7);}'
assert 13 'int fib(int n) { if (n<=1) return 1; else return fib(n-1)+fib(n-2);} int main() {return fib(6);}'
assert 23 'int f(int a, int b) { int n1, n2; n1 = a; n2 = b; return n1*3 + n2*2; } int main() { return f(5, 4); }'
assert 38 'int f(int a, int b, int c, int d, int e, int f, int g, int h) { int n, m; n = 2; m = 3; return n * g + m * h; } int main() { return f(1, 2, 3, 4, 5, 6, 7, 8); }'
assert 10 'int main() {int n; n=5; {n=10;} return n;}'
assert 10 'int main() {int n; n=10; {int a; a=5;} return n;}'
assert 10 'int main() {int n; int* p; n=20; p=&n; *p=10; return n;}'
assert 10 'int main() {int n, *p; n=20; p=&n; *p=10; return n;}'
assert 10 'int main() {int n; int* p; int **pp; n=20; p=&n; pp=&p; **pp=10; return n;}'
assert 10 'int f(int *n) {*n = 10;} int main() {int n; n = 15; f(&n); return n;}'
assert 4 'int main() {return sizeof(10);}'
assert 4 'int main() {int n; return sizeof(n);}'
assert 4 'int main() {int n; return sizeof(sizeof(n));}'
assert 8 'int main() {int *n; return sizeof(n);}'
assert 8 'int main() {int *n; return sizeof n;}'
assert 4 'int main() {return sizeof(int);}'
assert 8 'int main() {return sizeof(int*);}'
assert 8 'int main() {return sizeof(int**);}'
assert 8 'int main(){ int a[2]; return sizeof(a);}'
assert 10 'int add(int, int); int add(int a, int b) {return a + b;} int main() {return add(3, 7);}'
assert_fcall 4 'int alloc4(int **p); int main() {int *p; int *q; alloc4(&p); q = 3 + p; return *q;}' \
  "#include <stdlib.h>
  void alloc4(int **p) {*p = (int *)malloc(sizeof(int)*4); (*p)[0] = 1; (*p)[1] = 2; (*p)[2] = 3; (*p)[3] = 4;}"
assert_fcall 4 'int alloc4(int **p); int main() {int *p; int *q; alloc4(&p); q = p + 3; return *q;}' \
  "#include <stdlib.h>
  void alloc4(int **p) {*p = (int *)malloc(sizeof(int)*4); (*p)[0] = 1; (*p)[1] = 2; (*p)[2] = 3; (*p)[3] = 4;}"
assert_fcall 2 'int alloc4(int **p); int main() {int *p; alloc4(&p); p++; return *p;}' \
  "#include <stdlib.h>
  void alloc4(int **p) {*p = (int *)malloc(sizeof(int)*4); (*p)[0] = 1; (*p)[1] = 2; (*p)[2] = 3; (*p)[3] = 4;}"
assert_fcall 3 'int alloc4(int **p); int main() {int *p; alloc4(&p); p = p + 3; p--; return *p;}' \
  "#include <stdlib.h>
  void alloc4(int **p) {*p = (int *)malloc(sizeof(int)*4); (*p)[0] = 1; (*p)[1] = 2; (*p)[2] = 3; (*p)[3] = 4;}"
assert_fcall 10 'int **ptr(); int main() {int **pp; pp = ptr(); return **pp;}' 'int n = 10; int *p = &n; int **ptr(void) {return &p;}'
assert_fcall 1 'int ptr(int **pp, int ***ppp); int main() {int *p; int **pp; ptr(&p, &pp); return p == *pp;}' 'int n = 10; int *p = &n; void ptr(int **pp, int ***ppp) {*ppp = &p; *pp = p;}'
assert 5 'int main() {int a[2]; return 5;}'
assert 5 'int main() {int a[2]; *a = 2; return 5;}'
assert 2 'int main() {int a[2]; *a = 2; return *a;}'
assert 5 'int main() {int a[2]; *a = 2; *(a+1) = 3; return *a + *(a+1);}'
assert 10 'int main() {int b[2]; int a; a = 10; *b = 3; *(b+1) = 5; return a;}'
assert 10 'int main() {int a; int b[2]; a = 10; *b = 3; *(b+1) = 5; return a;}'
assert 10 'int main() {int a; int b[2]; a = 10; b[0] = 3; b[1] = 5; return a;}'
assert 5 'int main() {int a; int b[2]; a = 10; b[0] = 3; b[1] = 5; return b[1];}'
assert 10 'int a; int main() {a = 10; return a;}'
assert 105 'int a[3]; int b; int main() {a[0] = 3; a[1] = 5; a[2] = 7; return a[0] * a[1] * a[2];}'
assert 3 'int main() {char x[3]; int y; x[0] = 2; x[1] = -1; y = 4; return x[1] + y;}'
assert 2 'int main() {char x[3]; char *p; x[0] = 1; x[1] = 2; x[2] = 3; p = x; p++; return *p;}'
assert 6 'int main() {char c; c = 5; c++; return c;}'
assert 4 'int main() {char c; c = 5; c--; return c;}'
assert 97 "int main() {return 'a';}"
assert 97 'int main() {char *s; s = "abc"; return *s;}'
assert 100 'int main() {char *s; s = "abc"; s = "def"; return *s;}'
assert 102 'int main() {char *s; s = "abc"; s = "def"; return s[2];}'
assert 4 'int main() {return sizeof("foo");}'
assert 2 'int main() {return sizeof("\n");}'
assert 2 'int main() {return sizeof("\t");}'
assert 2 'int main() {return sizeof("\\");}'
assert 2 'int main() {return sizeof("\"");}'
assert 10 'int main() {return 10;} // comment'
assert 10 'int main() {/* return 5; */ return 10;}'

assert_fail 'int main(){int n; int *m; n = m; return 0;}'
assert_fail 'int main(){return f();}'
assert_fail 'int f(); int g(){f(3);}'
assert_fail 'int f(int *p); int g(){f(3);}'
assert_fail 'int f(); int main(){int *n; n = f();}'
assert_fail 'int *f() {return 5;}'
assert_fail 'int f() {15 = 3;}'
assert_fail 'int f() {int n; int m; n = &m;}'
assert_fail 'int f() {int n; int *p; n = **p;}'
assert_fail 'int f() {int n; int m; m = &*n;}'
assert_fail 'int add(int, int); int add(int a, int b) {int a;}'
assert_fail 'int main() {/*}'

echo OK
