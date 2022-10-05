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

assert 0 '0;'
assert 42 '42;'
assert 4 '5-3+2;'
assert 6 '5+3-2;'
assert 4 ' 5 - 3 + 2 ;'
assert 6 ' 5 + 3 - 2 ;'
assert 3 '6 / 2;'
assert 3 '7 / 2;'
assert 8 '10-4/2;'
assert 18 '4*3+3*2;'
assert 18 ' 4 * 3 + 3 * 2;'
assert 11 '5+3*2;'
assert 10 '15/3*2;'
assert 10 '15*2/3;'
assert 16 '(5+3)*2;'
assert 17 '(5+3)*2+1;'
assert 24 '(5+3)*(2+1);'
assert 4 '(5+3)/2;'
assert 5 '(5+3)/2+1;'
assert 2 '(5+3)/(2+1);'
assert 10 '-10+20;'
assert 10 '-5*+10+60;'
assert 75 '-(-5*+15);'
assert 1 '1 == 1;'
assert 0 '0 == 1;'
assert 1 '0 != 1;'
assert 0 '1 != 1;'
assert 1 '1 < 2;'
assert 0 '3 < 2;'
assert 1 '1 <= 2;'
assert 1 '2 <= 2;'
assert 0 '3 <= 2;'
assert 0 '1 > 2;'
assert 1 '3 > 2;'
assert 0 '1 >= 2;'
assert 1 '2 >= 2;'
assert 1 '3 >= 2;'
assert 10 'a = 10; a;'
assert 75 'a = -(-5*+15); a;'
assert 10 'a = 10; b = a; b;'
assert 10 'a = b = 10; a;'
assert 10 'a = b = 10; b;'
assert 13 'a = 3; t = 10; a + t;'
assert 30 'a = 3; t = 10; a * t;'
assert 1 'a = 3; t = 10; a < t;'
assert 0 'a = 3; t = 10; a > t;'
assert 10 'foo = 10; foo;'
assert 10 'foo = 10; bar = 20; foo;'
assert 20 'foo = 10; bar = 20; bar;'
assert 10 'foo = 2; bar = 5; foo * bar;'
assert 5 'foo = 2; foo = 5; foo;'
assert 5 'foo = 5; bar = foo; bar;'
assert 1 'foo = 5; bar = (foo * foo == 25); bar;'
assert 1 'bar = ((foo = 5) == 5); bar;'

echo OK
