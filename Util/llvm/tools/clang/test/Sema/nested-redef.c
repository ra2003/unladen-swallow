// RUN: clang -fsyntax-only -verify %s
struct X { // expected-note{{previous definition is here}}
  struct X { } x; // expected-error{{nested redefinition of 'X'}}
}; 

struct Y { };
void f(void) {
  struct Y { }; // okay: this is a different Y
}

struct T;
struct Z {
  struct T { int x; } t;
  struct U { int x; } u;
};

void f2(void) {
  struct T t;
  // FIXME: this is well-formed, but Clang breaks on it struct U u;
}

