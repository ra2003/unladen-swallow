//===--- TokenLexer.cpp - Lex from a token stream -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the TokenLexer interface.
//
//===----------------------------------------------------------------------===//

#include "MacroArgs.h"
#include "clang/Lex/MacroInfo.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/LexDiagnostic.h"
using namespace clang;

/// MacroArgs ctor function - This destroys the vector passed in.
MacroArgs *MacroArgs::create(const MacroInfo *MI,
                             const Token *UnexpArgTokens,
                             unsigned NumToks, bool VarargsElided) {
  assert(MI->isFunctionLike() &&
         "Can't have args for an object-like macro!");

  // Allocate memory for the MacroArgs object with the lexer tokens at the end.
  MacroArgs *Result = (MacroArgs*)malloc(sizeof(MacroArgs) +
                                         NumToks*sizeof(Token));
  // Construct the macroargs object.
  new (Result) MacroArgs(NumToks, VarargsElided);

  // Copy the actual unexpanded tokens to immediately after the result ptr.
  if (NumToks)
    memcpy(const_cast<Token*>(Result->getUnexpArgument(0)),
           UnexpArgTokens, NumToks*sizeof(Token));

  return Result;
}

/// destroy - Destroy and deallocate the memory for this object.
///
void MacroArgs::destroy() {
  // Run the dtor to deallocate the vectors.
  this->~MacroArgs();
  // Release the memory for the object.
  free(this);
}


/// getArgLength - Given a pointer to an expanded or unexpanded argument,
/// return the number of tokens, not counting the EOF, that make up the
/// argument.
unsigned MacroArgs::getArgLength(const Token *ArgPtr) {
  unsigned NumArgTokens = 0;
  for (; ArgPtr->isNot(tok::eof); ++ArgPtr)
    ++NumArgTokens;
  return NumArgTokens;
}


/// getUnexpArgument - Return the unexpanded tokens for the specified formal.
///
const Token *MacroArgs::getUnexpArgument(unsigned Arg) const {
  // The unexpanded argument tokens start immediately after the MacroArgs object
  // in memory.
  const Token *Start = (const Token *)(this+1);
  const Token *Result = Start;
  // Scan to find Arg.
  for (; Arg; ++Result) {
    assert(Result < Start+NumUnexpArgTokens && "Invalid arg #");
    if (Result->is(tok::eof))
      --Arg;
  }
  assert(Result < Start+NumUnexpArgTokens && "Invalid arg #");
  return Result;
}


/// ArgNeedsPreexpansion - If we can prove that the argument won't be affected
/// by pre-expansion, return false.  Otherwise, conservatively return true.
bool MacroArgs::ArgNeedsPreexpansion(const Token *ArgTok,
                                     Preprocessor &PP) const {
  // If there are no identifiers in the argument list, or if the identifiers are
  // known to not be macros, pre-expansion won't modify it.
  for (; ArgTok->isNot(tok::eof); ++ArgTok)
    if (IdentifierInfo *II = ArgTok->getIdentifierInfo()) {
      if (II->hasMacroDefinition() && PP.getMacroInfo(II)->isEnabled())
        // Return true even though the macro could be a function-like macro
        // without a following '(' token.
        return true;
    }
  return false;
}

/// getPreExpArgument - Return the pre-expanded form of the specified
/// argument.
const std::vector<Token> &
MacroArgs::getPreExpArgument(unsigned Arg, Preprocessor &PP) {
  assert(Arg < NumUnexpArgTokens && "Invalid argument number!");

  // If we have already computed this, return it.
  if (PreExpArgTokens.empty())
    PreExpArgTokens.resize(NumUnexpArgTokens);

  std::vector<Token> &Result = PreExpArgTokens[Arg];
  if (!Result.empty()) return Result;

  const Token *AT = getUnexpArgument(Arg);
  unsigned NumToks = getArgLength(AT)+1;  // Include the EOF.

  // Otherwise, we have to pre-expand this argument, populating Result.  To do
  // this, we set up a fake TokenLexer to lex from the unexpanded argument
  // list.  With this installed, we lex expanded tokens until we hit the EOF
  // token at the end of the unexp list.
  PP.EnterTokenStream(AT, NumToks, false /*disable expand*/,
                      false /*owns tokens*/);

  // Lex all of the macro-expanded tokens into Result.
  do {
    Result.push_back(Token());
    Token &Tok = Result.back();
    PP.Lex(Tok);
  } while (Result.back().isNot(tok::eof));

  // Pop the token stream off the top of the stack.  We know that the internal
  // pointer inside of it is to the "end" of the token stream, but the stack
  // will not otherwise be popped until the next token is lexed.  The problem is
  // that the token may be lexed sometime after the vector of tokens itself is
  // destroyed, which would be badness.
  PP.RemoveTopOfLexerStack();
  return Result;
}


/// StringifyArgument - Implement C99 6.10.3.2p2, converting a sequence of
/// tokens into the literal string token that should be produced by the C #
/// preprocessor operator.  If Charify is true, then it should be turned into
/// a character literal for the Microsoft charize (#@) extension.
///
Token MacroArgs::StringifyArgument(const Token *ArgToks,
                                   Preprocessor &PP, bool Charify) {
  Token Tok;
  Tok.startToken();
  Tok.setKind(tok::string_literal);

  const Token *ArgTokStart = ArgToks;

  // Stringify all the tokens.
  llvm::SmallString<128> Result;
  Result += "\"";

  bool isFirst = true;
  for (; ArgToks->isNot(tok::eof); ++ArgToks) {
    const Token &Tok = *ArgToks;
    if (!isFirst && (Tok.hasLeadingSpace() || Tok.isAtStartOfLine()))
      Result += ' ';
    isFirst = false;

    // If this is a string or character constant, escape the token as specified
    // by 6.10.3.2p2.
    if (Tok.is(tok::string_literal) ||       // "foo"
        Tok.is(tok::wide_string_literal) ||  // L"foo"
        Tok.is(tok::char_constant)) {        // 'x' and L'x'.
      std::string Str = Lexer::Stringify(PP.getSpelling(Tok));
      Result.append(Str.begin(), Str.end());
    } else {
      // Otherwise, just append the token.  Do some gymnastics to get the token
      // in place and avoid copies where possible.
      unsigned CurStrLen = Result.size();
      Result.resize(CurStrLen+Tok.getLength());
      const char *BufPtr = &Result[CurStrLen];
      unsigned ActualTokLen = PP.getSpelling(Tok, BufPtr);

      // If getSpelling returned a pointer to an already uniqued version of the
      // string instead of filling in BufPtr, memcpy it onto our string.
      if (BufPtr != &Result[CurStrLen])
        memcpy(&Result[CurStrLen], BufPtr, ActualTokLen);

      // If the token was dirty, the spelling may be shorter than the token.
      if (ActualTokLen != Tok.getLength())
        Result.resize(CurStrLen+ActualTokLen);
    }
  }

  // If the last character of the string is a \, and if it isn't escaped, this
  // is an invalid string literal, diagnose it as specified in C99.
  if (Result.back() == '\\') {
    // Count the number of consequtive \ characters.  If even, then they are
    // just escaped backslashes, otherwise it's an error.
    unsigned FirstNonSlash = Result.size()-2;
    // Guaranteed to find the starting " if nothing else.
    while (Result[FirstNonSlash] == '\\')
      --FirstNonSlash;
    if ((Result.size()-1-FirstNonSlash) & 1) {
      // Diagnose errors for things like: #define F(X) #X   /   F(\)
      PP.Diag(ArgToks[-1], diag::pp_invalid_string_literal);
      Result.pop_back();  // remove one of the \'s.
    }
  }
  Result += '"';

  // If this is the charify operation and the result is not a legal character
  // constant, diagnose it.
  if (Charify) {
    // First step, turn double quotes into single quotes:
    Result[0] = '\'';
    Result[Result.size()-1] = '\'';

    // Check for bogus character.
    bool isBad = false;
    if (Result.size() == 3)
      isBad = Result[1] == '\'';   // ''' is not legal. '\' already fixed above.
    else
      isBad = (Result.size() != 4 || Result[1] != '\\');  // Not '\x'

    if (isBad) {
      PP.Diag(ArgTokStart[0], diag::err_invalid_character_to_charify);
      Result = "' '";  // Use something arbitrary, but legal.
    }
  }

  PP.CreateString(&Result[0], Result.size(), Tok);
  return Tok;
}

/// getStringifiedArgument - Compute, cache, and return the specified argument
/// that has been 'stringified' as required by the # operator.
const Token &MacroArgs::getStringifiedArgument(unsigned ArgNo,
                                               Preprocessor &PP) {
  assert(ArgNo < NumUnexpArgTokens && "Invalid argument number!");
  if (StringifiedArgs.empty()) {
    StringifiedArgs.resize(getNumArguments());
    memset(&StringifiedArgs[0], 0,
           sizeof(StringifiedArgs[0])*getNumArguments());
  }
  if (StringifiedArgs[ArgNo].isNot(tok::string_literal))
    StringifiedArgs[ArgNo] = StringifyArgument(getUnexpArgument(ArgNo), PP);
  return StringifiedArgs[ArgNo];
}
