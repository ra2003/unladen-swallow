//===--- ToolChains.cpp - ToolChain Implementations ---------------------*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ToolChains.h"

#include "clang/Driver/Arg.h"
#include "clang/Driver/ArgList.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/HostInfo.h"
#include "clang/Driver/OptTable.h"
#include "clang/Driver/Option.h"
#include "clang/Driver/Options.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/System/Path.h"

#include <cstdlib> // ::getenv

using namespace clang::driver;
using namespace clang::driver::toolchains;

/// Darwin - Darwin tool chain for i386 and x86_64.

Darwin::Darwin(const HostInfo &Host, const llvm::Triple& Triple,
               const unsigned (&_DarwinVersion)[3], bool _IsIPhoneOS)
  : ToolChain(Host, Triple),
    IsIPhoneOS(_IsIPhoneOS)
{
  DarwinVersion[0] = _DarwinVersion[0];
  DarwinVersion[1] = _DarwinVersion[1];
  DarwinVersion[2] = _DarwinVersion[2];

  llvm::raw_string_ostream(MacosxVersionMin)
    << "10." << std::max(0, (int)DarwinVersion[0] - 4) << '.'
    << DarwinVersion[1];

  // FIXME: Lift default up.
  IPhoneOSVersionMin = "3.0";
}

DarwinGCC::DarwinGCC(const HostInfo &Host, const llvm::Triple& Triple,
                     const unsigned (&DarwinVersion)[3],
                     const unsigned (&_GCCVersion)[3], bool IsIPhoneOS)
  : Darwin(Host, Triple, DarwinVersion, IsIPhoneOS)
{
  GCCVersion[0] = _GCCVersion[0];
  GCCVersion[1] = _GCCVersion[1];
  GCCVersion[2] = _GCCVersion[2];

  // Set up the tool chain paths to match gcc.
  ToolChainDir = "i686-apple-darwin";
  ToolChainDir += llvm::utostr(DarwinVersion[0]);
  ToolChainDir += "/";
  ToolChainDir += llvm::utostr(GCCVersion[0]);
  ToolChainDir += '.';
  ToolChainDir += llvm::utostr(GCCVersion[1]);
  ToolChainDir += '.';
  ToolChainDir += llvm::utostr(GCCVersion[2]);

  // Try the next major version if that tool chain dir is invalid.
  std::string Tmp = "/usr/lib/gcc/" + ToolChainDir;
  if (!llvm::sys::Path(Tmp).exists()) {
    std::string Next = "i686-apple-darwin";
    Next += llvm::utostr(DarwinVersion[0] + 1);
    Next += "/";
    Next += llvm::utostr(GCCVersion[0]);
    Next += '.';
    Next += llvm::utostr(GCCVersion[1]);
    Next += '.';
    Next += llvm::utostr(GCCVersion[2]);

    // Use that if it exists, otherwise hope the user isn't linking.
    //
    // FIXME: Drop dependency on gcc's tool chain.
    Tmp = "/usr/lib/gcc/" + Next;
    if (llvm::sys::Path(Tmp).exists())
      ToolChainDir = Next;
  }

  std::string Path;
  if (getArchName() == "x86_64") {
    Path = getHost().getDriver().Dir;
    Path += "/../lib/gcc/";
    Path += ToolChainDir;
    Path += "/x86_64";
    getFilePaths().push_back(Path);

    Path = "/usr/lib/gcc/";
    Path += ToolChainDir;
    Path += "/x86_64";
    getFilePaths().push_back(Path);
  }

  Path = getHost().getDriver().Dir;
  Path += "/../lib/gcc/";
  Path += ToolChainDir;
  getFilePaths().push_back(Path);

  Path = "/usr/lib/gcc/";
  Path += ToolChainDir;
  getFilePaths().push_back(Path);

  Path = getHost().getDriver().Dir;
  Path += "/../libexec/gcc/";
  Path += ToolChainDir;
  getProgramPaths().push_back(Path);

  Path = "/usr/libexec/gcc/";
  Path += ToolChainDir;
  getProgramPaths().push_back(Path);

  Path = getHost().getDriver().Dir;
  Path += "/../libexec";
  getProgramPaths().push_back(Path);

  getProgramPaths().push_back(getHost().getDriver().Dir);
}

Darwin::~Darwin() {
  // Free tool implementations.
  for (llvm::DenseMap<unsigned, Tool*>::iterator
         it = Tools.begin(), ie = Tools.end(); it != ie; ++it)
    delete it->second;
}

Tool &Darwin::SelectTool(const Compilation &C, const JobAction &JA) const {
  Action::ActionClass Key;
  if (getHost().getDriver().ShouldUseClangCompiler(C, JA, getTriple()))
    Key = Action::AnalyzeJobClass;
  else
    Key = JA.getKind();

  Tool *&T = Tools[Key];
  if (!T) {
    switch (Key) {
    case Action::InputClass:
    case Action::BindArchClass:
      assert(0 && "Invalid tool kind.");
    case Action::PreprocessJobClass:
      T = new tools::darwin::Preprocess(*this); break;
    case Action::AnalyzeJobClass:
      T = new tools::Clang(*this); break;
    case Action::PrecompileJobClass:
    case Action::CompileJobClass:
      T = new tools::darwin::Compile(*this); break;
    case Action::AssembleJobClass:
      T = new tools::darwin::Assemble(*this); break;
    case Action::LinkJobClass:
      T = new tools::darwin::Link(*this); break;
    case Action::LipoJobClass:
      T = new tools::darwin::Lipo(*this); break;
    }
  }

  return *T;
}

void DarwinGCC::AddLinkSearchPathArgs(const ArgList &Args,
                                      ArgStringList &CmdArgs) const {
  // FIXME: Derive these correctly.
  if (getArchName() == "x86_64") {
    CmdArgs.push_back(Args.MakeArgString("-L/usr/lib/gcc/" + ToolChainDir +
                                         "/x86_64"));
    // Intentionally duplicated for (temporary) gcc bug compatibility.
    CmdArgs.push_back(Args.MakeArgString("-L/usr/lib/gcc/" + ToolChainDir +
                                         "/x86_64"));
  }
  CmdArgs.push_back(Args.MakeArgString("-L/usr/lib/" + ToolChainDir));
  CmdArgs.push_back(Args.MakeArgString("-L/usr/lib/gcc/" + ToolChainDir));
  // Intentionally duplicated for (temporary) gcc bug compatibility.
  CmdArgs.push_back(Args.MakeArgString("-L/usr/lib/gcc/" + ToolChainDir));
  CmdArgs.push_back(Args.MakeArgString("-L/usr/lib/gcc/" + ToolChainDir +
                                       "/../../../" + ToolChainDir));
  CmdArgs.push_back(Args.MakeArgString("-L/usr/lib/gcc/" + ToolChainDir +
                                       "/../../.."));
}

void DarwinGCC::AddLinkRuntimeLibArgs(const ArgList &Args,
                                      ArgStringList &CmdArgs) const {
  unsigned MacosxVersionMin[3];
  getMacosxVersionMin(Args, MacosxVersionMin);

  // Derived from libgcc and lib specs but refactored.
  if (Args.hasArg(options::OPT_static)) {
    CmdArgs.push_back("-lgcc_static");
  } else {
    if (Args.hasArg(options::OPT_static_libgcc)) {
      CmdArgs.push_back("-lgcc_eh");
    } else if (Args.hasArg(options::OPT_miphoneos_version_min_EQ)) {
      // Derived from darwin_iphoneos_libgcc spec.
      if (isIPhoneOS()) {
        CmdArgs.push_back("-lgcc_s.1");
      } else {
        CmdArgs.push_back("-lgcc_s.10.5");
      }
    } else if (Args.hasArg(options::OPT_shared_libgcc) ||
               // FIXME: -fexceptions -fno-exceptions means no exceptions
               Args.hasArg(options::OPT_fexceptions) ||
               Args.hasArg(options::OPT_fgnu_runtime)) {
      // FIXME: This is probably broken on 10.3?
      if (isMacosxVersionLT(MacosxVersionMin, 10, 5))
        CmdArgs.push_back("-lgcc_s.10.4");
      else if (isMacosxVersionLT(MacosxVersionMin, 10, 6))
        CmdArgs.push_back("-lgcc_s.10.5");
    } else {
      if (isMacosxVersionLT(MacosxVersionMin, 10, 3, 9))
        ; // Do nothing.
      else if (isMacosxVersionLT(MacosxVersionMin, 10, 5))
        CmdArgs.push_back("-lgcc_s.10.4");
      else if (isMacosxVersionLT(MacosxVersionMin, 10, 6))
        CmdArgs.push_back("-lgcc_s.10.5");
    }

    if (isIPhoneOS() || isMacosxVersionLT(MacosxVersionMin, 10, 6)) {
      CmdArgs.push_back("-lgcc");
      CmdArgs.push_back("-lSystem");
    } else {
      CmdArgs.push_back("-lSystem");
      CmdArgs.push_back("-lgcc");
    }
  }
}

DarwinClang::DarwinClang(const HostInfo &Host, const llvm::Triple& Triple,
                         const unsigned (&DarwinVersion)[3],
                         bool IsIPhoneOS)
  : Darwin(Host, Triple, DarwinVersion, IsIPhoneOS)
{
  // Add the relative libexec dir (for clang-cc).
  //
  // FIXME: We should sink clang-cc into libexec/clang/<version>/.
  std::string Path = getHost().getDriver().Dir;
  Path += "/../libexec";
  getProgramPaths().push_back(Path);

  // We expect 'as', 'ld', etc. to be adjacent to our install dir.
  getProgramPaths().push_back(getHost().getDriver().Dir);
}

void DarwinClang::AddLinkSearchPathArgs(const ArgList &Args,
                                       ArgStringList &CmdArgs) const {
  // The Clang toolchain uses explicit paths for internal libraries.
}

void DarwinClang::AddLinkRuntimeLibArgs(const ArgList &Args,
                                        ArgStringList &CmdArgs) const {
  // Check for static linking.
  if (Args.hasArg(options::OPT_static)) {
    // FIXME: We need to have compiler-rt available (perhaps as
    // libclang_static.a) to link against.
    return;
  }

  // Reject -static-libgcc for now, we can deal with this when and if someone
  // cares. This is useful in situations where someone wants to statically link
  // something like libstdc++, and needs its runtime support routines.
  if (const Arg *A = Args.getLastArg(options::OPT_static_libgcc)) {
    getHost().getDriver().Diag(clang::diag::err_drv_unsupported_opt)
      << A->getAsString(Args);
    return;
  }

  // Otherwise link libSystem, which should have the support routines.
  //
  // FIXME: This is only true for 10.6 and beyond. Legacy support isn't
  // critical, but it should work... we should just link in the static
  // compiler-rt library.
  CmdArgs.push_back("-lSystem");
}

void Darwin::getMacosxVersionMin(const ArgList &Args,
                                 unsigned (&Res)[3]) const {
  if (Arg *A = Args.getLastArg(options::OPT_mmacosx_version_min_EQ)) {
    bool HadExtra;
    if (!Driver::GetReleaseVersion(A->getValue(Args), Res[0], Res[1], Res[2],
                                   HadExtra) ||
        HadExtra) {
      const Driver &D = getHost().getDriver();
      D.Diag(clang::diag::err_drv_invalid_version_number)
        << A->getAsString(Args);
    }
  } else
    return getMacosxVersion(Res);
}

DerivedArgList *Darwin::TranslateArgs(InputArgList &Args,
                                      const char *BoundArch) const {
  DerivedArgList *DAL = new DerivedArgList(Args, false);
  const OptTable &Opts = getHost().getDriver().getOpts();

  // FIXME: We really want to get out of the tool chain level argument
  // translation business, as it makes the driver functionality much
  // more opaque. For now, we follow gcc closely solely for the
  // purpose of easily achieving feature parity & testability. Once we
  // have something that works, we should reevaluate each translation
  // and try to push it down into tool specific logic.

  Arg *OSXVersion =
    Args.getLastArgNoClaim(options::OPT_mmacosx_version_min_EQ);
  Arg *iPhoneVersion =
    Args.getLastArgNoClaim(options::OPT_miphoneos_version_min_EQ);
  if (OSXVersion && iPhoneVersion) {
    getHost().getDriver().Diag(clang::diag::err_drv_argument_not_allowed_with)
          << OSXVersion->getAsString(Args)
          << iPhoneVersion->getAsString(Args);
  } else if (!OSXVersion && !iPhoneVersion) {
    // Chose the default version based on the arch.
    //
    // FIXME: Are there iPhone overrides for this?

    if (!isIPhoneOS()) {
      // Look for MACOSX_DEPLOYMENT_TARGET, otherwise use the version
      // from the host.
      const char *Version = ::getenv("MACOSX_DEPLOYMENT_TARGET");
      if (!Version)
        Version = MacosxVersionMin.c_str();
      const Option *O = Opts.getOption(options::OPT_mmacosx_version_min_EQ);
      DAL->append(DAL->MakeJoinedArg(0, O, Version));
    } else {
      const char *Version = IPhoneOSVersionMin.c_str();
      const Option *O = Opts.getOption(options::OPT_miphoneos_version_min_EQ);
      DAL->append(DAL->MakeJoinedArg(0, O, Version));
    }
  }

  for (ArgList::iterator it = Args.begin(), ie = Args.end(); it != ie; ++it) {
    Arg *A = *it;

    if (A->getOption().matches(options::OPT_Xarch__)) {
      // FIXME: Canonicalize name.
      if (getArchName() != A->getValue(Args, 0))
        continue;

      // FIXME: The arg is leaked here, and we should have a nicer
      // interface for this.
      unsigned Prev, Index = Prev = A->getIndex() + 1;
      Arg *XarchArg = Opts.ParseOneArg(Args, Index);

      // If the argument parsing failed or more than one argument was
      // consumed, the -Xarch_ argument's parameter tried to consume
      // extra arguments. Emit an error and ignore.
      //
      // We also want to disallow any options which would alter the
      // driver behavior; that isn't going to work in our model. We
      // use isDriverOption() as an approximation, although things
      // like -O4 are going to slip through.
      if (!XarchArg || Index > Prev + 1 ||
          XarchArg->getOption().isDriverOption()) {
       getHost().getDriver().Diag(clang::diag::err_drv_invalid_Xarch_argument)
          << A->getAsString(Args);
        continue;
      }

      XarchArg->setBaseArg(A);
      A = XarchArg;
    }

    // Sob. These is strictly gcc compatible for the time being. Apple
    // gcc translates options twice, which means that self-expanding
    // options add duplicates.
    switch ((options::ID) A->getOption().getID()) {
    default:
      DAL->append(A);
      break;

    case options::OPT_mkernel:
    case options::OPT_fapple_kext:
      DAL->append(A);
      DAL->append(DAL->MakeFlagArg(A, Opts.getOption(options::OPT_static)));
      DAL->append(DAL->MakeFlagArg(A, Opts.getOption(options::OPT_static)));
      break;

    case options::OPT_dependency_file:
      DAL->append(DAL->MakeSeparateArg(A, Opts.getOption(options::OPT_MF),
                                       A->getValue(Args)));
      break;

    case options::OPT_gfull:
      DAL->append(DAL->MakeFlagArg(A, Opts.getOption(options::OPT_g_Flag)));
      DAL->append(DAL->MakeFlagArg(A,
             Opts.getOption(options::OPT_fno_eliminate_unused_debug_symbols)));
      break;

    case options::OPT_gused:
      DAL->append(DAL->MakeFlagArg(A, Opts.getOption(options::OPT_g_Flag)));
      DAL->append(DAL->MakeFlagArg(A,
             Opts.getOption(options::OPT_feliminate_unused_debug_symbols)));
      break;

    case options::OPT_fterminated_vtables:
    case options::OPT_findirect_virtual_calls:
      DAL->append(DAL->MakeFlagArg(A,
                                   Opts.getOption(options::OPT_fapple_kext)));
      DAL->append(DAL->MakeFlagArg(A, Opts.getOption(options::OPT_static)));
      break;

    case options::OPT_shared:
      DAL->append(DAL->MakeFlagArg(A, Opts.getOption(options::OPT_dynamiclib)));
      break;

    case options::OPT_fconstant_cfstrings:
      DAL->append(DAL->MakeFlagArg(A,
                             Opts.getOption(options::OPT_mconstant_cfstrings)));
      break;

    case options::OPT_fno_constant_cfstrings:
      DAL->append(DAL->MakeFlagArg(A,
                          Opts.getOption(options::OPT_mno_constant_cfstrings)));
      break;

    case options::OPT_Wnonportable_cfstrings:
      DAL->append(DAL->MakeFlagArg(A,
                     Opts.getOption(options::OPT_mwarn_nonportable_cfstrings)));
      break;

    case options::OPT_Wno_nonportable_cfstrings:
      DAL->append(DAL->MakeFlagArg(A,
                  Opts.getOption(options::OPT_mno_warn_nonportable_cfstrings)));
      break;

    case options::OPT_fpascal_strings:
      DAL->append(DAL->MakeFlagArg(A,
                                 Opts.getOption(options::OPT_mpascal_strings)));
      break;

    case options::OPT_fno_pascal_strings:
      DAL->append(DAL->MakeFlagArg(A,
                              Opts.getOption(options::OPT_mno_pascal_strings)));
      break;
    }
  }

  if (getTriple().getArch() == llvm::Triple::x86 ||
      getTriple().getArch() == llvm::Triple::x86_64)
    if (!Args.hasArgNoClaim(options::OPT_mtune_EQ))
      DAL->append(DAL->MakeJoinedArg(0, Opts.getOption(options::OPT_mtune_EQ),
                                     "core2"));

  // Add the arch options based on the particular spelling of -arch, to match
  // how the driver driver works.
  if (BoundArch) {
    llvm::StringRef Name = BoundArch;
    const Option *MCpu = Opts.getOption(options::OPT_mcpu_EQ);
    const Option *MArch = Opts.getOption(options::OPT_march_EQ);

    // This code must be kept in sync with LLVM's getArchTypeForDarwinArch,
    // which defines the list of which architectures we accept.
    if (Name == "ppc")
      ;
    else if (Name == "ppc601")
      DAL->append(DAL->MakeJoinedArg(0, MCpu, "601"));
    else if (Name == "ppc603")
      DAL->append(DAL->MakeJoinedArg(0, MCpu, "603"));
    else if (Name == "ppc604")
      DAL->append(DAL->MakeJoinedArg(0, MCpu, "604"));
    else if (Name == "ppc604e")
      DAL->append(DAL->MakeJoinedArg(0, MCpu, "604e"));
    else if (Name == "ppc750")
      DAL->append(DAL->MakeJoinedArg(0, MCpu, "750"));
    else if (Name == "ppc7400")
      DAL->append(DAL->MakeJoinedArg(0, MCpu, "7400"));
    else if (Name == "ppc7450")
      DAL->append(DAL->MakeJoinedArg(0, MCpu, "7450"));
    else if (Name == "ppc970")
      DAL->append(DAL->MakeJoinedArg(0, MCpu, "970"));

    else if (Name == "ppc64")
      DAL->append(DAL->MakeFlagArg(0, Opts.getOption(options::OPT_m64)));

    else if (Name == "i386")
      ;
    else if (Name == "i486")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "i486"));
    else if (Name == "i586")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "i586"));
    else if (Name == "i686")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "i686"));
    else if (Name == "pentium")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "pentium"));
    else if (Name == "pentium2")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "pentium2"));
    else if (Name == "pentpro")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "pentiumpro"));
    else if (Name == "pentIIm3")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "pentium2"));

    else if (Name == "x86_64")
      DAL->append(DAL->MakeFlagArg(0, Opts.getOption(options::OPT_m64)));

    else if (Name == "arm")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "armv4t"));
    else if (Name == "armv4t")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "armv4t"));
    else if (Name == "armv5")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "armv5tej"));
    else if (Name == "xscale")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "xscale"));
    else if (Name == "armv6")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "armv6k"));
    else if (Name == "armv7")
      DAL->append(DAL->MakeJoinedArg(0, MArch, "armv7a"));

    else
      llvm::llvm_unreachable("invalid Darwin arch");
  }

  return DAL;
}

bool Darwin::IsMathErrnoDefault() const {
  return false;
}

bool Darwin::IsUnwindTablesDefault() const {
  // FIXME: Gross; we should probably have some separate target
  // definition, possibly even reusing the one in clang.
  return getArchName() == "x86_64";
}

const char *Darwin::GetDefaultRelocationModel() const {
  return "pic";
}

const char *Darwin::GetForcedPicModel() const {
  if (getArchName() == "x86_64")
    return "pic";
  return 0;
}

/// Generic_GCC - A tool chain using the 'gcc' command to perform
/// all subcommands; this relies on gcc translating the majority of
/// command line options.

Generic_GCC::Generic_GCC(const HostInfo &Host, const llvm::Triple& Triple)
  : ToolChain(Host, Triple) {
  std::string Path(getHost().getDriver().Dir);
  Path += "/../libexec";
  getProgramPaths().push_back(Path);

  getProgramPaths().push_back(getHost().getDriver().Dir);
}

Generic_GCC::~Generic_GCC() {
  // Free tool implementations.
  for (llvm::DenseMap<unsigned, Tool*>::iterator
         it = Tools.begin(), ie = Tools.end(); it != ie; ++it)
    delete it->second;
}

Tool &Generic_GCC::SelectTool(const Compilation &C,
                              const JobAction &JA) const {
  Action::ActionClass Key;
  if (getHost().getDriver().ShouldUseClangCompiler(C, JA, getTriple()))
    Key = Action::AnalyzeJobClass;
  else
    Key = JA.getKind();

  Tool *&T = Tools[Key];
  if (!T) {
    switch (Key) {
    case Action::InputClass:
    case Action::BindArchClass:
      assert(0 && "Invalid tool kind.");
    case Action::PreprocessJobClass:
      T = new tools::gcc::Preprocess(*this); break;
    case Action::PrecompileJobClass:
      T = new tools::gcc::Precompile(*this); break;
    case Action::AnalyzeJobClass:
      T = new tools::Clang(*this); break;
    case Action::CompileJobClass:
      T = new tools::gcc::Compile(*this); break;
    case Action::AssembleJobClass:
      T = new tools::gcc::Assemble(*this); break;
    case Action::LinkJobClass:
      T = new tools::gcc::Link(*this); break;

      // This is a bit ungeneric, but the only platform using a driver
      // driver is Darwin.
    case Action::LipoJobClass:
      T = new tools::darwin::Lipo(*this); break;
    }
  }

  return *T;
}

bool Generic_GCC::IsMathErrnoDefault() const {
  return true;
}

bool Generic_GCC::IsUnwindTablesDefault() const {
  // FIXME: Gross; we should probably have some separate target
  // definition, possibly even reusing the one in clang.
  return getArchName() == "x86_64";
}

const char *Generic_GCC::GetDefaultRelocationModel() const {
  return "static";
}

const char *Generic_GCC::GetForcedPicModel() const {
  return 0;
}

DerivedArgList *Generic_GCC::TranslateArgs(InputArgList &Args,
                                           const char *BoundArch) const {
  return new DerivedArgList(Args, true);
}

/// OpenBSD - OpenBSD tool chain which can call as(1) and ld(1) directly.

OpenBSD::OpenBSD(const HostInfo &Host, const llvm::Triple& Triple)
  : Generic_GCC(Host, Triple) {
  getFilePaths().push_back(getHost().getDriver().Dir + "/../lib");
  getFilePaths().push_back("/usr/lib");
}

Tool &OpenBSD::SelectTool(const Compilation &C, const JobAction &JA) const {
  Action::ActionClass Key;
  if (getHost().getDriver().ShouldUseClangCompiler(C, JA, getTriple()))
    Key = Action::AnalyzeJobClass;
  else
    Key = JA.getKind();

  Tool *&T = Tools[Key];
  if (!T) {
    switch (Key) {
    case Action::AssembleJobClass:
      T = new tools::openbsd::Assemble(*this); break;
    case Action::LinkJobClass:
      T = new tools::openbsd::Link(*this); break;
    default:
      T = &Generic_GCC::SelectTool(C, JA);
    }
  }

  return *T;
}

/// FreeBSD - FreeBSD tool chain which can call as(1) and ld(1) directly.

FreeBSD::FreeBSD(const HostInfo &Host, const llvm::Triple& Triple, bool Lib32)
  : Generic_GCC(Host, Triple) {
  if (Lib32) {
    getFilePaths().push_back(getHost().getDriver().Dir + "/../lib32");
    getFilePaths().push_back("/usr/lib32");
  } else {
    getFilePaths().push_back(getHost().getDriver().Dir + "/../lib");
    getFilePaths().push_back("/usr/lib");
  }
}

Tool &FreeBSD::SelectTool(const Compilation &C, const JobAction &JA) const {
  Action::ActionClass Key;
  if (getHost().getDriver().ShouldUseClangCompiler(C, JA, getTriple()))
    Key = Action::AnalyzeJobClass;
  else
    Key = JA.getKind();

  Tool *&T = Tools[Key];
  if (!T) {
    switch (Key) {
    case Action::AssembleJobClass:
      T = new tools::freebsd::Assemble(*this); break;
    case Action::LinkJobClass:
      T = new tools::freebsd::Link(*this); break;
    default:
      T = &Generic_GCC::SelectTool(C, JA);
    }
  }

  return *T;
}

/// AuroraUX - AuroraUX tool chain which can call as(1) and ld(1) directly.

AuroraUX::AuroraUX(const HostInfo &Host, const llvm::Triple& Triple)
  : Generic_GCC(Host, Triple) {

  // Path mangling to find libexec
  std::string Path(getHost().getDriver().Dir);

  Path += "/../libexec";
  getProgramPaths().push_back(Path);
  getProgramPaths().push_back(getHost().getDriver().Dir);

  getFilePaths().push_back(getHost().getDriver().Dir + "/../lib");
  getFilePaths().push_back("/usr/lib");
  getFilePaths().push_back("/usr/sfw/lib");
  getFilePaths().push_back("/opt/gcc4/lib");
  getFilePaths().push_back("/opt/gcc4/lib/gcc/i386-pc-solaris2.11/4.2.4");

}

Tool &AuroraUX::SelectTool(const Compilation &C, const JobAction &JA) const {
  Action::ActionClass Key;
  if (getHost().getDriver().ShouldUseClangCompiler(C, JA, getTriple()))
    Key = Action::AnalyzeJobClass;
  else
    Key = JA.getKind();

  Tool *&T = Tools[Key];
  if (!T) {
    switch (Key) {
    case Action::AssembleJobClass:
      T = new tools::auroraux::Assemble(*this); break;
    case Action::LinkJobClass:
      T = new tools::auroraux::Link(*this); break;
    default:
      T = &Generic_GCC::SelectTool(C, JA);
    }
  }

  return *T;
}


/// Linux toolchain (very bare-bones at the moment).

Linux::Linux(const HostInfo &Host, const llvm::Triple& Triple)
  : Generic_GCC(Host, Triple) {
  getFilePaths().push_back(getHost().getDriver().Dir + "/../lib/clang/1.0/");
  getFilePaths().push_back("/lib/");
  getFilePaths().push_back("/usr/lib/");

  // Depending on the Linux distribution, any combination of lib{,32,64} is
  // possible. E.g. Debian uses lib and lib32 for mixed i386/x86-64 systems,
  // openSUSE uses lib and lib64 for the same purpose.
  getFilePaths().push_back("/lib32/");
  getFilePaths().push_back("/usr/lib32/");
  getFilePaths().push_back("/lib64/");
  getFilePaths().push_back("/usr/lib64/");

  // FIXME: Figure out some way to get gcc's libdir
  // (e.g. /usr/lib/gcc/i486-linux-gnu/4.3/ for Ubuntu 32-bit); we need
  // crtbegin.o/crtend.o/etc., and want static versions of various
  // libraries. If we had our own crtbegin.o/crtend.o/etc, we could probably
  // get away with using shared versions in /usr/lib, though.
  // We could fall back to the approach we used for includes (a massive
  // list), but that's messy at best.
}

/// DragonFly - DragonFly tool chain which can call as(1) and ld(1) directly.

DragonFly::DragonFly(const HostInfo &Host, const llvm::Triple& Triple)
  : Generic_GCC(Host, Triple) {

  // Path mangling to find libexec
  std::string Path(getHost().getDriver().Dir);

  Path += "/../libexec";
  getProgramPaths().push_back(Path);
  getProgramPaths().push_back(getHost().getDriver().Dir);

  getFilePaths().push_back(getHost().getDriver().Dir + "/../lib");
  getFilePaths().push_back("/usr/lib");
  getFilePaths().push_back("/usr/lib/gcc41");
}

Tool &DragonFly::SelectTool(const Compilation &C, const JobAction &JA) const {
  Action::ActionClass Key;
  if (getHost().getDriver().ShouldUseClangCompiler(C, JA, getTriple()))
    Key = Action::AnalyzeJobClass;
  else
    Key = JA.getKind();

  Tool *&T = Tools[Key];
  if (!T) {
    switch (Key) {
    case Action::AssembleJobClass:
      T = new tools::dragonfly::Assemble(*this); break;
    case Action::LinkJobClass:
      T = new tools::dragonfly::Link(*this); break;
    default:
      T = &Generic_GCC::SelectTool(C, JA);
    }
  }

  return *T;
}
