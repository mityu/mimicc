#!/bin/bash

cd $(dirname $0)

TESTCC=${TESTCC:-../mimicc}

assert_fail() {
  echo "$1" > ./Xtmp/tmp.c
  $TESTCC -o ./Xtmp/tmp.s -S ./Xtmp/tmp.c
  if [ $? -eq 0 ]; then
    echo "Unexpected 0 is returned."
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
assert_fail 'int main(void) {int *p; int n = p;}'
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
assert_fail 'int main(void) {int *p; int x[1] = {p};}'
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
assert_fail 'struct S {int n; char n;};'
assert_fail 'enum E {ItemA, ItemA};'
assert_fail 'typedef struct S A1; typedef struct S A2; struct S s;'
assert_fail 'typedef enum E A1; typedef enum E A2; enum E s;'
assert_fail 'typedef int X; typedef char X;'
assert_fail 'typedef int Int[]; Int array;'
assert_fail 'typedef int Int[]; int main(void) {Int array[3] = {};}'
assert_fail 'typedef int Int[]; int main(void) {sizeof(Int);}'
assert_fail 'int main(void) {int a[]; sizeof(a);}'
assert_fail 'typedef struct {int n;} S; int main(void) {S s = {3, 5};}'
assert_fail 'typedef struct {} S1; typedef struct {} S2; int main(void) {S1 s1 = (S2){};}'
assert_fail 'typedef int Ar[2]; int main(void) {Ar array = {3, 5, 7};}'
assert_fail 'int f(void); char str[f()];'
assert_fail 'int main(void) {int n = 5; int array[n];}'
assert_fail 'int main(void) {int n = 5; int array[3 + n];}'
assert_fail 'void f(undefined_type varname);'
assert_fail 'int main(void) {int n, *p; n & p;}'
assert_fail 'int main(void) {int n, *p; n | p;}'
assert_fail 'int main(void) {int n, *p; n ^ p;}'
assert_fail 'int main(void) {struct {} obj; !obj;}'
assert_fail 'int main(void) {struct {} obj; 3 && obj;}'
assert_fail 'int main(void) {struct {} obj; 3 || obj;}'
assert_fail 'int main(void) {int *p; p < 3;}'
assert_fail 'int main(void) {int *p; p <= 3;}'
assert_fail 'int main(void) {int *p; p < 0;}'
assert_fail 'int main(void) {int *p; p <= 0;}'
assert_fail 'int main(void) {int *p; p == 3;}'
assert_fail 'int main(void) {int *p; p != 3;}'
assert_fail 'int main(void) {void (**fpp)(void); fpp();}'
assert_fail 'int main(void) {void (*fp1)(void), (*fp2)(void); fp1 = &fp2;}'
assert_fail 'void f(void); int main(void) {void (**fpp)(void); fpp = &f;}'

echo 'static int main(void) { return 0;}' > ./Xtmp/tmp.c
$TESTCC -o ./Xtmp/tmp.s -S ./Xtmp/tmp.c || exit 1
gcc ./Xtmp/tmp.s
if [ $? -eq 0 ]; then
  echo "Unexpected 0 is returned."
  exit 1
fi
