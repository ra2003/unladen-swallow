// RUN: clang-cc -fsyntax-only -verify %s
// RUN: clang-cc -fsyntax-only -verify -x objective-c++ %s
void * proc();

@interface NSConstantString
@end

@interface Frob
@end

@interface Frob1
@end

void * foo()
{
  @try {
    return proc();
  }
  @catch (Frob* ex) {
    @throw;
  }
  @catch (Frob1* ex) {
    @throw proc();
  }
  @finally {
    @try {
      return proc();
    }
    @catch (Frob* ex) {
      @throw 1,2; // expected-error {{@throw requires an Objective-C object type ('int' invalid)}}
    }
    @catch (float x) {  // expected-error {{@catch parameter is not a pointer to an interface type}}
      
    }
    @catch(...) {
      @throw (4,3,proc());
    }
  }

  @try {  // expected-error {{@try statement without a @catch and @finally clause}}
    return proc();
  }
}


void bar()
{
  @try {}// expected-error {{@try statement without a @catch and @finally clause}}
  @"s"; //  expected-warning {{result unused}}
}

void baz()
{
  @try {}// expected-error {{@try statement without a @catch and @finally clause}}
  @try {}
  @finally {}
}

void noTwoTokenLookAheadRequiresABitOfFancyFootworkInTheParser() {
    @try {
        // Do something
    } @catch (...) {}
    @try {
        // Do something
    } @catch (...) {}
    return;
}

