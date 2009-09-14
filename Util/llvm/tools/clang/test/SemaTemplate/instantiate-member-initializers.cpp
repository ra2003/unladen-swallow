// RUN: clang-cc -fsyntax-only -Wall -verify %s

template<typename T> struct A {
  A() : a(1) { } // expected-error{{incompatible type passing 'int', expected 'void *'}}

  T a;
};

A<int> a0;
A<void*> a1; // expected-note{{in instantiation of member function 'A<void *>::A' requested here}}

template<typename T> struct B {
  // FIXME: This should warn about initialization order
  B() : b(1), a(2) { }
  
  int a;
  int b;
};

B<int> b0;