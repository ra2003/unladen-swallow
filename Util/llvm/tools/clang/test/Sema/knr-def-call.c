// RUN: clang-cc -fsyntax-only -verify %s

// C DR #316, PR 3626.
void f0(a, b, c, d) int a,b,c,d; {}
void t0(void) { 
  f0(1);  // expected-warning{{too few arguments}}
}

void f1(a, b) int a, b; {}
void t1(void) { 
  f1(1, 2, 3); // expected-warning{{too many arguments}}
}

void f2(float); // expected-note{{previous declaration is here}}
void f2(x) float x; { } // expected-warning{{promoted type 'double' of K&R function parameter is not compatible with the parameter type 'float' declared in a previous prototype}}

typedef void (*f3)(void);
f3 t3(int b) { return b? f0 : f1; } // okay
