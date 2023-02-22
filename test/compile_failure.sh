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
assert_fail 'int main() {int x[3], y[3]; x = y;}'
assert_fail 'int main() {int x[3]; int *y; x = y;}'
assert_fail 'int main() {void **p; int n; p = n;}'
assert_fail 'int main() {void **p; int n; p == n;}'
assert_fail 'int main(void) {int x[1] = {2, 3, 4};}'
assert_fail 'int main(void) {int x[1][2] = {{1, 2}, {3, 4}};}'
assert_fail 'int main(void) {int x[1][2] = {{1, 2, 3}};}'
assert_fail 'int main(void) {int x[2][2] = {{1, 2}, {2, 3, 4}};}'
assert_fail 'int main(void) {int x[];}'
assert_fail 'int main(void) {int x[][] = {{1}, {2}};}'
assert_fail 'void gvar;'
assert_fail 'int gvar[3][];'
assert_fail 'int f(void); void f(void){}'
assert_fail 'int f(int); int f(void){}'
assert_fail 'int f(int); int f(char c){}'
assert_fail 'int main(void) {break;}'
assert_fail 'int main(void) {if (0) break;}'
assert_fail 'int main(void) {struct Foo foo;}'
assert_fail 'struct S{ struct S obj; };'
assert_fail 'int main(void) { int n; n->member;}'
assert_fail 'struct S {int n;}; int main(void) { struct S obj; obj->n;}'
assert_fail 'struct S {int n;}; int main(void) { struct S **obj; obj->n;}'
