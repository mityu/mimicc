#!/bin/bash
# Basic functionalities test to guarantee functions in test.framework (mainly
# assert* functions) works correctly.

cd $(dirname $0)

TESTCC=${TESTCC:-../mimicc}

assert() {
  expected="$1"
  input="$2"

  echo "$input" > ./Xtmp/tmp.c
  $TESTCC -o ./Xtmp/tmp.s -S ./Xtmp/tmp.c || exit 1
  gcc -o ./Xtmp/tmp ./Xtmp/tmp.s || exit 1
  ./Xtmp/tmp
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

  echo "$input" > ./Xtmp/tmp1.c
  $TESTCC -o ./Xtmp/tmp1.s -S ./Xtmp/tmp1.c || exit 1
  gcc -c -o ./Xtmp/tmp1.o ./Xtmp/tmp1.s || exit 1
  gcc -c -o ./Xtmp/tmp2.o -x c <(echo "$restcode") || exit 1
  gcc -o ./Xtmp/tmp ./Xtmp/tmp1.o ./Xtmp/tmp2.o || exit 1
  ./Xtmp/tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 'int main(void) {}'
assert 42 'int main(void){ return 42;}'
assert 42 'int main(void){ return 42; return 10;}'
assert 42 'int main(void) { if (35 == 35) { return 42;} else { return 10;} return 35;}'
assert 42 'int main(void) { if (0 == 35) { return 10;} else { return 42;} return 35;}'
assert 17 'int main(void) {int n = 13; do {n = 17;} while(0); return n;}'
assert_fcall 42 \
  'int f(void); int main(void) {return f();}' \
  'int f(void) {return 42;}'
assert_fcall 42 \
  'void f(int *); int main(void) {int n; n = 10; f(&n); return n;}' \
  'void f(int *p) {*p = 42;}'
assert_fcall 1 \
  'int f(int a, int b); int main(void) {return f(1, 2);}' \
  'int f(int a, int b) {return a == 1 && b == 2;}'
assert_fcall 8 \
  'int f(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8); int main(void) {return f(1, 2, 3, 4, 5, 6, 7, 8);}' \
  'int f(int n1, int n2, int n3, int n4, int n5, int n6, int n7, int n8) { return n8;}'
assert 10 'int main(void) {return 10;} // comment'
assert 10 'int main(void) {/* return 5; */ return 10;}'
