// RUN: clang-cc  -fsyntax-only -verify %s

  - (NSDictionary*) _executeScript:(NSString *)source { // expected-error 2 {{expected a type}} \
							// expected-error {{missing context for method declaration}} 
                          Exit:  [nilArgs release]; // expected-error {{use of undeclared identifier}}
                          }
                                     - (NSDictionary *) _setupKernelStandardMode:(NSString *)source { // expected-error 2 {{expected a type}} \
expected-error {{missing context for method declaration}}
                           Exit:   if(_ciKernel && !success ) { // expected-error {{use of undeclared identifier}} // expected-error 2 {{expected}}
