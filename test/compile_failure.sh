#!/bin/bash

assert_fail() {
  echo "$1" > ./Xtmp/tmp.c
  ../mimic ./Xtmp/tmp.c
  if [ $? -ne 1 ]; then
    echo "Other than 1 returned."
    exit 1
  fi
}

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
assert_fail 'int main() {return sizeof("foo);}'
assert_fail "int main() {return ';}"
assert_fail "int main() {return '';}"
assert_fail "int main() {return '\l';}"
assert_fail 'int main() {void n;}'
assert_fail 'void f(int, void);'
assert_fail 'void f(void, int);'
assert_fail 'void f(); int main(void) {int n; n = f();}'

