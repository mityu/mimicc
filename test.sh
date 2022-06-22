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

assert 0 '0'
assert 42 '42'
assert 4 '5-3+2'
assert 6 '5+3-2'
assert 4 ' 5 - 3 + 2 '
assert 6 ' 5 + 3 - 2 '
assert 3 '6 / 2'
assert 3 '7 / 2'
assert 8 '10-4/2'
assert 18 '4*3+3*2'
assert 18 ' 4 * 3 + 3 * 2'
assert 11 '5+3*2'
assert 10 '15/3*2'
assert 10 '15*2/3'
# assert 16 '(5+3)*2'
# assert 17 '(5+3)*2+1'
# assert 24 '(5+3)*(2+1)'
# assert 4 '(5+3)/2'
# assert 5 '(5+3)/2+1'
# assert 2 '(5+3)/(2+1)'
assert 10 '-10+20'
assert 10 '-5*+10+60'
# assert 75 '-(-5*+15)'

echo OK
