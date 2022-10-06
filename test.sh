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

assert 0 'return 0;'
assert 42 'return 42;'
assert 4 'return 5-3+2;'
assert 6 'return 5+3-2;'
assert 4 'return  5 - 3 + 2 ;'
assert 6 'return  5 + 3 - 2 ;'
assert 3 'return 6 / 2;'
assert 3 'return 7 / 2;'
assert 8 'return 10-4/2;'
assert 18 'return 4*3+3*2;'
assert 18 'return  4 * 3 + 3 * 2;'
assert 11 'return 5+3*2;'
assert 10 'return 15/3*2;'
assert 10 'return 15*2/3;'
assert 16 'return (5+3)*2;'
assert 17 'return (5+3)*2+1;'
assert 24 'return (5+3)*(2+1);'
assert 4 'return (5+3)/2;'
assert 5 'return (5+3)/2+1;'
assert 2 'return (5+3)/(2+1);'
assert 10 'return -10+20;'
assert 10 'return -5*+10+60;'
assert 75 'return -(-5*+15);'
assert 1 'return 1 == 1;'
assert 0 'return 0 == 1;'
assert 1 'return 0 != 1;'
assert 0 'return 1 != 1;'
assert 1 'return 1 < 2;'
assert 0 'return 3 < 2;'
assert 1 'return 1 <= 2;'
assert 1 'return 2 <= 2;'
assert 0 'return 3 <= 2;'
assert 0 'return 1 > 2;'
assert 1 'return 3 > 2;'
assert 0 'return 1 >= 2;'
assert 1 'return 2 >= 2;'
assert 1 'return 3 >= 2;'
assert 10 'a = 10; return a;'
assert 75 'a = -(-5*+15); return a;'
assert 10 'a = 10; b = a; return b;'
assert 10 'a = b = 10; return a;'
assert 10 'a = b = 10; return b;'
assert 13 'a = 3; t = 10; return a + t;'
assert 30 'a = 3; t = 10; return a * t;'
assert 1 'a = 3; t = 10; return a < t;'
assert 0 'a = 3; t = 10; return a > t;'
assert 10 'foo = 10; return foo;'
assert 10 'foo = 10; bar = 20; return foo;'
assert 20 'foo = 10; bar = 20; return bar;'
assert 10 'foo = 2; bar = 5; return foo * bar;'
assert 5 'foo = 2; foo = 5; return foo;'
assert 5 'foo = 5; bar = foo; return bar;'
assert 1 'foo = 5; bar = (foo * foo == 25); return bar;'
assert 1 'bar = ((foo = 5) == 5); return bar;'
assert 42 'return 42; return 10;'

echo OK
