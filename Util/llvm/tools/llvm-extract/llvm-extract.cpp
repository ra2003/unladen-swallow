//===- llvm-extract.cpp - LLVM function extraction utility ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This utility changes the input module to only contain a single function,
// which is primarily used for debugging transformations.
//
//===----------------------------------------------------------------------===//

#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/System/Signals.h"
#include <memory>
using namespace llvm;

// InputFilename - The filename to read from.
static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
              cl::init("-"), cl::value_desc("filename"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Specify output filename"),
               cl::value_desc("filename"), cl::init("-"));

static cl::opt<bool>
Force("f", cl::desc("Enable binary output on terminals"));

static cl::opt<bool>
DeleteFn("delete", cl::desc("Delete specified Globals from Module"));

static cl::opt<bool>
Relink("relink",
       cl::desc("Turn external linkage for callees of function to delete"));

// ExtractFunc - The function to extract from the module... 
static cl::opt<std::string>
ExtractFunc("func", cl::desc("Specify function to extract"), cl::init(""),
            cl::value_desc("function"));

// ExtractGlobal - The global to extract from the module...
static cl::opt<std::string>
ExtractGlobal("glob", cl::desc("Specify global to extract"), cl::init(""),
              cl::value_desc("global"));

static cl::opt<bool>
OutputAssembly("S",
               cl::desc("Write output as LLVM assembly"), cl::Hidden);

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "llvm extractor\n");

  SMDiagnostic Err;
  std::auto_ptr<Module> M;
  M.reset(ParseIRFile(InputFilename, Err, Context));

  if (M.get() == 0) {
    Err.Print(argv[0], errs());
    return 1;
  }

  // Figure out which function we should extract
  GlobalVariable *G = !ExtractGlobal.empty() ?
    M.get()->getNamedGlobal(ExtractGlobal) : 0;

  // Figure out which function we should extract
  if (ExtractFunc.empty() && ExtractGlobal.empty()) ExtractFunc = "main";
  Function *F = M.get()->getFunction(ExtractFunc);

  if (F == 0 && G == 0) {
    errs() << argv[0] << ": program doesn't contain function named '"
           << ExtractFunc << "' or a global named '" << ExtractGlobal << "'!\n";
    return 1;
  }

  // In addition to deleting all other functions, we also want to spiff it
  // up a little bit.  Do this now.
  PassManager Passes;
  Passes.add(new TargetData(M.get())); // Use correct TargetData
  // Either isolate the function or delete it from the Module
  std::vector<GlobalValue*> GVs;
  if (F) GVs.push_back(F);
  if (G) GVs.push_back(G);

  Passes.add(createGVExtractionPass(GVs, DeleteFn, Relink));
  if (!DeleteFn)
    Passes.add(createGlobalDCEPass());           // Delete unreachable globals
  Passes.add(createDeadTypeEliminationPass());   // Remove dead types...
  Passes.add(createStripDeadPrototypesPass());   // Remove dead func decls

  // Make sure that the Output file gets unlinked from the disk if we get a
  // SIGINT
  sys::RemoveFileOnSignal(sys::Path(OutputFilename));

  std::string ErrorInfo;
  raw_fd_ostream Out(OutputFilename.c_str(), ErrorInfo,
                     raw_fd_ostream::F_Binary);
  if (!ErrorInfo.empty()) {
    errs() << ErrorInfo << '\n';
    return 1;
  }

  if (OutputAssembly)
    Passes.add(createPrintModulePass(&Out));
  else if (Force || !CheckBitcodeOutputToConsole(Out, true))
    Passes.add(createBitcodeWriterPass(Out));

  Passes.run(*M.get());

  return 0;
}
