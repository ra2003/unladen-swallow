// RUN: clang-cc -std=gnu89 -triple i386-apple-darwin9 -emit-llvm %s -o - | FileCheck -check-prefix GNU89 %s
// RUN: clang-cc -std=c99 -triple i386-apple-darwin9 -emit-llvm %s -o - | FileCheck -check-prefix C99 %s

// CHECK-GNU89: define i32 @f0()
// CHECK-C99: define i32 @f0()
int f0(void);
int f0(void) { return 0; }

// CHECK-GNU89: define i32 @f1()
// CHECK-C99: define i32 @f1()
inline int f1(void);
int f1(void) { return 0; }

// CHECK-GNU89: define i32 @f2()
// CHECK-C99: define i32 @f2()
int f2(void);
inline int f2(void) { return 0; }

// CHECK-GNU89: define i32 @f3()
// CHECK-C99: define i32 @f3()
extern inline int f3(void);
int f3(void) { return 0; }

// CHECK-GNU89: define i32 @f5()
// CHECK-C99: define i32 @f5()
extern inline int f5(void);
inline int f5(void) { return 0; }

// CHECK-GNU89: define i32 @f6()
// CHECK-C99: define i32 @f6()
inline int f6(void);
extern inline int f6(void) { return 0; }

// CHECK-GNU89: define i32 @f7()
// CHECK-C99: define i32 @f7()
extern inline int f7(void);
extern int f7(void) { return 0; }

// CHECK-GNU89: define i32 @fA()
inline int fA(void) { return 0; }

// CHECK-GNU89: define available_externally i32 @f4()
// CHECK-C99: define i32 @f4()
int f4(void);
extern inline int f4(void) { return 0; }

// CHECK-GNU89: define available_externally i32 @f8()
// CHECK-C99: define i32 @f8()
extern int f8(void);
extern inline int f8(void) { return 0; }

// CHECK-GNU89: define available_externally i32 @f9()
// CHECK-C99: define i32 @f9()
extern inline int f9(void);
extern inline int f9(void) { return 0; }

// CHECK-C99: define available_externally i32 @fA()

int test_all() { 
  return f0() + f1() + f2() + f3() + f4() + f5() + f6() + f7() + f8() + f9() 
    + fA();
}
