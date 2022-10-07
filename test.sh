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

assert 42 'main(){ return 42;}'
assert 4 'main(){ return 5-3+2;}'
assert 6 'main(){ return 5+3-2;}'
assert 6 'main(){ return  5 + 3 - 2 ;}'
assert 3 'main(){ return 6 / 2;}'
assert 3 'main(){ return 7 / 2;}'
assert 8 'main(){ return 10-4/2;}'
assert 18 'main(){ return  4 * 3 + 3 * 2;}'
assert 11 'main(){ return 5+3*2;}'
assert 10 'main(){ return 15/3*2;}'
assert 10 'main(){ return 15*2/3;}'
assert 16 'main(){ return (5+3)*2;}'
assert 17 'main(){ return (5+3)*2+1;}'
assert 24 'main(){ return (5+3)*(2+1);}'
assert 4 'main(){ return (5+3)/2;}'
assert 5 'main(){ return (5+3)/2+1;}'
assert 2 'main(){ return (5+3)/(2+1);}'
assert 10 'main(){ return -10+20;}'
assert 10 'main(){ return -5*+10+60;}'
assert 75 'main(){ return -(-5*+15);}'
assert 1 'main(){ return 1 == 1;}'
assert 0 'main(){ return 0 == 1;}'
assert 1 'main(){ return 0 != 1;}'
assert 0 'main(){ return 1 != 1;}'
assert 1 'main(){ return 1 < 2;}'
assert 0 'main(){ return 3 < 2;}'
assert 1 'main(){ return 1 <= 2;}'
assert 1 'main(){ return 2 <= 2;}'
assert 0 'main(){ return 3 <= 2;}'
assert 0 'main(){ return 1 > 2;}'
assert 1 'main(){ return 3 > 2;}'
assert 0 'main(){ return 1 >= 2;}'
assert 1 'main(){ return 2 >= 2;}'
assert 1 'main(){ return 3 >= 2;}'
assert 10 'main(){ a = 10; return a;}'
assert 75 'main(){ a = -(-5*+15); return a;}'
assert 10 'main(){ a = 10; b = a; return b;}'
assert 10 'main(){ a = b = 10; return a;}'
assert 10 'main(){ a = b = 10; return b;}'
assert 13 'main(){ a = 3; t = 10; return a + t;}'
assert 30 'main(){ a = 3; t = 10; return a * t;}'
assert 1 'main(){ a = 3; t = 10; return a < t;}'
assert 0 'main(){ a = 3; t = 10; return a > t;}'
assert 10 'main(){ foo = 10; return foo;}'
assert 10 'main(){ foo = 10; bar = 20; return foo;}'
assert 20 'main(){ foo = 10; bar = 20; return bar;}'
assert 10 'main(){ foo = 2; bar = 5; return foo * bar;}'
assert 5 'main(){ foo = 2; foo = 5; return foo;}'
assert 5 'main(){ foo = 5; bar = foo; return bar;}'
assert 1 'main(){ foo = 5; bar = (foo * foo == 25); return bar;}'
assert 1 'main(){ bar = ((foo = 5) == 5); return bar;}'
assert 42 'main(){ return 42; return 10;}'
assert 42 'main(){ if (1) return 42; return 10;}'
assert 10 'main(){ if (0) return 42; return 10;}'
assert 10 'main(){ if (1) if (0) return 42; return 10;}'
assert 42 'main(){ if (1) if (1) return 42; return 10;}'
assert 20 'main(){ if (0) return 10; else return 20; return 30;}'
assert 6 'main(){ a = 0; for (i = 0; i < 3; i = i + 1) a = a + i + 1; return a;}'
assert 10 'main(){ for (;;) return 10;}'
assert 10 'main(){ for (1;;) return 10;}'
assert 10 'main(){ for (;1;) return 10;}'
assert 10 'main(){ for (;;1) return 10;}'
assert 10 'main(){ while (1) return 10;}'
assert 10 'main(){ while (0) return 3; return 10;}'
assert 50 'main(){ a = 0; while (a < 1) while (a < 2) a = a+1; return 50;}'
assert 50 'main(){ {a = 1; b = 2; return 50;}}'
assert 50 'main(){ {{a = 1; b = 2;} return 50;}}'
assert 89 'main(){ a=1; b=1; for (i=0; i<9; i=i+1) {tmp=a;a=a+b;b=tmp;} return a;}'
assert 10 'main(){ if (0) {return 1;} else if (1) {return 10;} else {return 2;}}'
assert 10 'main(){ if (0) {return 1;} else if (0) {return 2;} else if (1) {return 10;} else {return 3;}}'
assert 10 'main(){ if (0) {return 1;} else if (0) {return 2;} else {return 10;}}'
assert_fcall 42 'main() {return f();}' 'int f() {return 42;}'
assert_fcall 1 'main() {return f(1, 2);}' 'int f(int a, int b) {return a == 1 && b == 2;}'
assert_fcall 8 'main() {return f(1, 2, 3, 4, 5, 6, 7, 8);}' 'int f(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8) { return n8;}'
assert_fcall 42 'main() {return f(1, 2, 3, 4, 5, 6, f(1, 2, 3, 4, 5, 6, 7, 8), 42);}' 'int f(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8) { return n8;}'
assert 13 'fib(n) { if (n<=1) return 1; else return fib(n-1)+fib(n-2);} main() {return fib(6);}'
assert 23 'f(a, b) { n1 = a; n2 = b; return n1*3 + n2*2; } main() { return f(5, 4); }'
assert 38 'f(a, b, c, d, e, f, g, h) { a = 2; b = 3; return a * g + b * h; } main() { return f(1, 2, 3, 4, 5, 6, 7, 8); }'
assert 10 'main() {n=5; {n=10;} return n;}'
assert 10 'main() {n=10; {a=5;} return n;}'
assert 10 'main() {n=20; p=&n; *p=10; return n;}'

echo OK
