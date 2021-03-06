// RUN: clang-cc -Eonly -verify %s 
#  define HEADER <float.h>

#  include HEADER

#include <limits.h> NON_EMPTY // expected-warning {{extra tokens at end of #include directive}}

// PR3916: these are ok.
#define EMPTY
#include <limits.h> EMPTY
#include HEADER  EMPTY

// PR3916
#define FN limits.h>
#include <FN

#include <>    // expected-error {{empty filename}}
