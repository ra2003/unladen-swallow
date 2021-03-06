//===--- ASTUnit.cpp - ASTUnit utility ------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// ASTUnit Implementation.
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/PCHReader.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclVisitor.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/Support/Compiler.h"
#include "llvm/System/Path.h"

using namespace clang;

ASTUnit::ASTUnit(DiagnosticClient *diagClient) : tempFile(false) {      
  Diags.setClient(diagClient ? diagClient : new TextDiagnosticBuffer());
}
ASTUnit::~ASTUnit() { 
  if (tempFile)
    llvm::sys::Path(getPCHFileName()).eraseFromDisk();
  
  //  The ASTUnit object owns the DiagnosticClient.
  delete Diags.getClient();
}

namespace {

/// \brief Gathers information from PCHReader that will be used to initialize
/// a Preprocessor.
class VISIBILITY_HIDDEN PCHInfoCollector : public PCHReaderListener {
  LangOptions &LangOpt;
  HeaderSearch &HSI;
  std::string &TargetTriple;
  std::string &Predefines;
  unsigned &Counter;

  unsigned NumHeaderInfos;

public:
  PCHInfoCollector(LangOptions &LangOpt, HeaderSearch &HSI,
                   std::string &TargetTriple, std::string &Predefines,
                   unsigned &Counter)
    : LangOpt(LangOpt), HSI(HSI), TargetTriple(TargetTriple),
      Predefines(Predefines), Counter(Counter), NumHeaderInfos(0) {}

  virtual bool ReadLanguageOptions(const LangOptions &LangOpts) {
    LangOpt = LangOpts;
    return false;
  }

  virtual bool ReadTargetTriple(llvm::StringRef Triple) {
    TargetTriple = Triple;
    return false;
  }

  virtual bool ReadPredefinesBuffer(llvm::StringRef PCHPredef,
                                    FileID PCHBufferID,
                                    llvm::StringRef OriginalFileName,
                                    std::string &SuggestedPredefines) {
    Predefines = PCHPredef;
    return false;
  }

  virtual void ReadHeaderFileInfo(const HeaderFileInfo &HFI) {
    HSI.setHeaderFileInfoForUID(HFI, NumHeaderInfos++);
  }

  virtual void ReadCounter(unsigned Value) {
    Counter = Value;
  }
};

} // anonymous namespace

const std::string &ASTUnit::getOriginalSourceFileName() {
  return dyn_cast<PCHReader>(Ctx->getExternalSource())->getOriginalSourceFile();
}

const std::string &ASTUnit::getPCHFileName() {
  return dyn_cast<PCHReader>(Ctx->getExternalSource())->getFileName();
}

ASTUnit *ASTUnit::LoadFromPCHFile(const std::string &Filename,
                                  std::string *ErrMsg,
                                  DiagnosticClient *diagClient,
                                  bool OnlyLocalDecls,
                                  bool UseBumpAllocator) {
  llvm::OwningPtr<ASTUnit> AST(new ASTUnit(diagClient));
  AST->OnlyLocalDecls = OnlyLocalDecls;
  AST->HeaderInfo.reset(new HeaderSearch(AST->getFileManager()));

  // Gather Info for preprocessor construction later on.

  LangOptions LangInfo;
  HeaderSearch &HeaderInfo = *AST->HeaderInfo.get();
  std::string TargetTriple;
  std::string Predefines;
  unsigned Counter;

  llvm::OwningPtr<PCHReader> Reader;
  llvm::OwningPtr<ExternalASTSource> Source;

  Reader.reset(new PCHReader(AST->getSourceManager(), AST->getFileManager(),
                             AST->Diags));
  Reader->setListener(new PCHInfoCollector(LangInfo, HeaderInfo, TargetTriple,
                                           Predefines, Counter));

  switch (Reader->ReadPCH(Filename)) {
  case PCHReader::Success:
    break;

  case PCHReader::Failure:
  case PCHReader::IgnorePCH:
    if (ErrMsg)
      *ErrMsg = "Could not load PCH file";
    return NULL;
  }

  // PCH loaded successfully. Now create the preprocessor.

  // Get information about the target being compiled for.
  //
  // FIXME: This is broken, we should store the TargetOptions in the PCH.
  TargetOptions TargetOpts;
  TargetOpts.ABI = "";
  TargetOpts.CPU = "";
  TargetOpts.Features.clear();
  TargetOpts.Triple = TargetTriple;
  AST->Target.reset(TargetInfo::CreateTargetInfo(AST->Diags, TargetOpts));
  AST->PP.reset(new Preprocessor(AST->Diags, LangInfo, *AST->Target.get(),
                                 AST->getSourceManager(), HeaderInfo));
  Preprocessor &PP = *AST->PP.get();

  PP.setPredefines(Reader->getSuggestedPredefines());
  PP.setCounterValue(Counter);
  Reader->setPreprocessor(PP);

  // Create and initialize the ASTContext.

  AST->Ctx.reset(new ASTContext(LangInfo,
                                AST->getSourceManager(),
                                *AST->Target.get(),
                                PP.getIdentifierTable(),
                                PP.getSelectorTable(),
                                PP.getBuiltinInfo(),
                                /* FreeMemory = */ !UseBumpAllocator,
                                /* size_reserve = */0));
  ASTContext &Context = *AST->Ctx.get();

  Reader->InitializeContext(Context);

  // Attach the PCH reader to the AST context as an external AST
  // source, so that declarations will be deserialized from the
  // PCH file as needed.
  Source.reset(Reader.take());
  Context.setExternalSource(Source);

  return AST.take();
}
