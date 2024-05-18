//===- SemaHLSL.cpp - Semantic Analysis for HLSL constructs ---------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This implements Semantic Analysis for HLSL constructs.
//===----------------------------------------------------------------------===//

#include "clang/Sema/SemaHLSL.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/DiagnosticSema.h"
#include "clang/Basic/LLVM.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Sema/Sema.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/TargetParser/Triple.h"
#include <iterator>

using namespace clang;

SemaHLSL::SemaHLSL(Sema &S) : SemaBase(S) {}

Decl *SemaHLSL::ActOnStartBuffer(Scope *BufferScope, bool CBuffer,
                                 SourceLocation KwLoc, IdentifierInfo *Ident,
                                 SourceLocation IdentLoc,
                                 SourceLocation LBrace) {
  // For anonymous namespace, take the location of the left brace.
  DeclContext *LexicalParent = SemaRef.getCurLexicalContext();
  HLSLBufferDecl *Result = HLSLBufferDecl::Create(
      getASTContext(), LexicalParent, CBuffer, KwLoc, Ident, IdentLoc, LBrace);

  SemaRef.PushOnScopeChains(Result, BufferScope);
  SemaRef.PushDeclContext(BufferScope, Result);

  return Result;
}

// Calculate the size of a legacy cbuffer type based on
// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-packing-rules
static unsigned calculateLegacyCbufferSize(const ASTContext &Context,
                                           QualType T) {
  unsigned Size = 0;
  constexpr unsigned CBufferAlign = 128;
  if (const RecordType *RT = T->getAs<RecordType>()) {
    const RecordDecl *RD = RT->getDecl();
    for (const FieldDecl *Field : RD->fields()) {
      QualType Ty = Field->getType();
      unsigned FieldSize = calculateLegacyCbufferSize(Context, Ty);
      unsigned FieldAlign = 32;
      if (Ty->isAggregateType())
        FieldAlign = CBufferAlign;
      Size = llvm::alignTo(Size, FieldAlign);
      Size += FieldSize;
    }
  } else if (const ConstantArrayType *AT = Context.getAsConstantArrayType(T)) {
    if (unsigned ElementCount = AT->getSize().getZExtValue()) {
      unsigned ElementSize =
          calculateLegacyCbufferSize(Context, AT->getElementType());
      unsigned AlignedElementSize = llvm::alignTo(ElementSize, CBufferAlign);
      Size = AlignedElementSize * (ElementCount - 1) + ElementSize;
    }
  } else if (const VectorType *VT = T->getAs<VectorType>()) {
    unsigned ElementCount = VT->getNumElements();
    unsigned ElementSize =
        calculateLegacyCbufferSize(Context, VT->getElementType());
    Size = ElementSize * ElementCount;
  } else {
    Size = Context.getTypeSize(T);
  }
  return Size;
}

void SemaHLSL::ActOnFinishBuffer(Decl *Dcl, SourceLocation RBrace) {
  auto *BufDecl = cast<HLSLBufferDecl>(Dcl);
  BufDecl->setRBraceLoc(RBrace);

  // Validate packoffset.
  llvm::SmallVector<std::pair<VarDecl *, HLSLPackOffsetAttr *>> PackOffsetVec;
  bool HasPackOffset = false;
  bool HasNonPackOffset = false;
  for (auto *Field : BufDecl->decls()) {
    VarDecl *Var = dyn_cast<VarDecl>(Field);
    if (!Var)
      continue;
    if (Field->hasAttr<HLSLPackOffsetAttr>()) {
      PackOffsetVec.emplace_back(Var, Field->getAttr<HLSLPackOffsetAttr>());
      HasPackOffset = true;
    } else {
      HasNonPackOffset = true;
    }
  }

  if (HasPackOffset && HasNonPackOffset)
    Diag(BufDecl->getLocation(), diag::warn_hlsl_packoffset_mix);

  if (HasPackOffset) {
    ASTContext &Context = getASTContext();
    // Make sure no overlap in packoffset.
    // Sort PackOffsetVec by offset.
    std::sort(PackOffsetVec.begin(), PackOffsetVec.end(),
              [](const std::pair<VarDecl *, HLSLPackOffsetAttr *> &LHS,
                 const std::pair<VarDecl *, HLSLPackOffsetAttr *> &RHS) {
                return LHS.second->getOffset() < RHS.second->getOffset();
              });

    for (unsigned i = 0; i < PackOffsetVec.size() - 1; i++) {
      VarDecl *Var = PackOffsetVec[i].first;
      HLSLPackOffsetAttr *Attr = PackOffsetVec[i].second;
      unsigned Size = calculateLegacyCbufferSize(Context, Var->getType());
      unsigned Begin = Attr->getOffset() * 32;
      unsigned End = Begin + Size;
      unsigned NextBegin = PackOffsetVec[i + 1].second->getOffset() * 32;
      if (End > NextBegin) {
        VarDecl *NextVar = PackOffsetVec[i + 1].first;
        Diag(NextVar->getLocation(), diag::err_hlsl_packoffset_overlap)
            << NextVar << Var;
      }
    }
  }

  SemaRef.PopDeclContext();
}

HLSLNumThreadsAttr *SemaHLSL::mergeNumThreadsAttr(Decl *D,
                                                  const AttributeCommonInfo &AL,
                                                  int X, int Y, int Z) {
  if (HLSLNumThreadsAttr *NT = D->getAttr<HLSLNumThreadsAttr>()) {
    if (NT->getX() != X || NT->getY() != Y || NT->getZ() != Z) {
      Diag(NT->getLocation(), diag::err_hlsl_attribute_param_mismatch) << AL;
      Diag(AL.getLoc(), diag::note_conflicting_attribute);
    }
    return nullptr;
  }
  return ::new (getASTContext())
      HLSLNumThreadsAttr(getASTContext(), AL, X, Y, Z);
}

HLSLShaderAttr *
SemaHLSL::mergeShaderAttr(Decl *D, const AttributeCommonInfo &AL,
                          HLSLShaderAttr::ShaderType ShaderType) {
  if (HLSLShaderAttr *NT = D->getAttr<HLSLShaderAttr>()) {
    if (NT->getType() != ShaderType) {
      Diag(NT->getLocation(), diag::err_hlsl_attribute_param_mismatch) << AL;
      Diag(AL.getLoc(), diag::note_conflicting_attribute);
    }
    return nullptr;
  }
  return HLSLShaderAttr::Create(getASTContext(), ShaderType, AL);
}

HLSLParamModifierAttr *
SemaHLSL::mergeParamModifierAttr(Decl *D, const AttributeCommonInfo &AL,
                                 HLSLParamModifierAttr::Spelling Spelling) {
  // We can only merge an `in` attribute with an `out` attribute. All other
  // combinations of duplicated attributes are ill-formed.
  if (HLSLParamModifierAttr *PA = D->getAttr<HLSLParamModifierAttr>()) {
    if ((PA->isIn() && Spelling == HLSLParamModifierAttr::Keyword_out) ||
        (PA->isOut() && Spelling == HLSLParamModifierAttr::Keyword_in)) {
      D->dropAttr<HLSLParamModifierAttr>();
      SourceRange AdjustedRange = {PA->getLocation(), AL.getRange().getEnd()};
      return HLSLParamModifierAttr::Create(
          getASTContext(), /*MergedSpelling=*/true, AdjustedRange,
          HLSLParamModifierAttr::Keyword_inout);
    }
    Diag(AL.getLoc(), diag::err_hlsl_duplicate_parameter_modifier) << AL;
    Diag(PA->getLocation(), diag::note_conflicting_attribute);
    return nullptr;
  }
  return HLSLParamModifierAttr::Create(getASTContext(), AL);
}

void SemaHLSL::ActOnTopLevelFunction(FunctionDecl *FD) {
  auto &TargetInfo = getASTContext().getTargetInfo();

  if (FD->getName() != TargetInfo.getTargetOpts().HLSLEntry)
    return;

  StringRef Env = TargetInfo.getTriple().getEnvironmentName();
  HLSLShaderAttr::ShaderType ShaderType;
  if (HLSLShaderAttr::ConvertStrToShaderType(Env, ShaderType)) {
    if (const auto *Shader = FD->getAttr<HLSLShaderAttr>()) {
      // The entry point is already annotated - check that it matches the
      // triple.
      if (Shader->getType() != ShaderType) {
        Diag(Shader->getLocation(), diag::err_hlsl_entry_shader_attr_mismatch)
            << Shader;
        FD->setInvalidDecl();
      }
    } else {
      // Implicitly add the shader attribute if the entry function isn't
      // explicitly annotated.
      FD->addAttr(HLSLShaderAttr::CreateImplicit(getASTContext(), ShaderType,
                                                 FD->getBeginLoc()));
    }
  } else {
    switch (TargetInfo.getTriple().getEnvironment()) {
    case llvm::Triple::UnknownEnvironment:
    case llvm::Triple::Library:
      break;
    default:
      llvm_unreachable("Unhandled environment in triple");
    }
  }
}

void SemaHLSL::CheckEntryPoint(FunctionDecl *FD) {
  const auto *ShaderAttr = FD->getAttr<HLSLShaderAttr>();
  assert(ShaderAttr && "Entry point has no shader attribute");
  HLSLShaderAttr::ShaderType ST = ShaderAttr->getType();

  switch (ST) {
  case HLSLShaderAttr::Pixel:
  case HLSLShaderAttr::Vertex:
  case HLSLShaderAttr::Geometry:
  case HLSLShaderAttr::Hull:
  case HLSLShaderAttr::Domain:
  case HLSLShaderAttr::RayGeneration:
  case HLSLShaderAttr::Intersection:
  case HLSLShaderAttr::AnyHit:
  case HLSLShaderAttr::ClosestHit:
  case HLSLShaderAttr::Miss:
  case HLSLShaderAttr::Callable:
    if (const auto *NT = FD->getAttr<HLSLNumThreadsAttr>()) {
      DiagnoseAttrStageMismatch(NT, ST,
                                {HLSLShaderAttr::Compute,
                                 HLSLShaderAttr::Amplification,
                                 HLSLShaderAttr::Mesh});
      FD->setInvalidDecl();
    }
    break;

  case HLSLShaderAttr::Compute:
  case HLSLShaderAttr::Amplification:
  case HLSLShaderAttr::Mesh:
    if (!FD->hasAttr<HLSLNumThreadsAttr>()) {
      Diag(FD->getLocation(), diag::err_hlsl_missing_numthreads)
          << HLSLShaderAttr::ConvertShaderTypeToStr(ST);
      FD->setInvalidDecl();
    }
    break;
  }

  for (ParmVarDecl *Param : FD->parameters()) {
    if (const auto *AnnotationAttr = Param->getAttr<HLSLAnnotationAttr>()) {
      CheckSemanticAnnotation(FD, Param, AnnotationAttr);
    } else {
      // FIXME: Handle struct parameters where annotations are on struct fields.
      // See: https://github.com/llvm/llvm-project/issues/57875
      Diag(FD->getLocation(), diag::err_hlsl_missing_semantic_annotation);
      Diag(Param->getLocation(), diag::note_previous_decl) << Param;
      FD->setInvalidDecl();
    }
  }
  // FIXME: Verify return type semantic annotation.
}

void SemaHLSL::CheckSemanticAnnotation(
    FunctionDecl *EntryPoint, const Decl *Param,
    const HLSLAnnotationAttr *AnnotationAttr) {
  auto *ShaderAttr = EntryPoint->getAttr<HLSLShaderAttr>();
  assert(ShaderAttr && "Entry point has no shader attribute");
  HLSLShaderAttr::ShaderType ST = ShaderAttr->getType();

  switch (AnnotationAttr->getKind()) {
  case attr::HLSLSV_DispatchThreadID:
  case attr::HLSLSV_GroupIndex:
    if (ST == HLSLShaderAttr::Compute)
      return;
    DiagnoseAttrStageMismatch(AnnotationAttr, ST, {HLSLShaderAttr::Compute});
    break;
  default:
    llvm_unreachable("Unknown HLSLAnnotationAttr");
  }
}

void SemaHLSL::DiagnoseAttrStageMismatch(
    const Attr *A, HLSLShaderAttr::ShaderType Stage,
    std::initializer_list<HLSLShaderAttr::ShaderType> AllowedStages) {
  SmallVector<StringRef, 8> StageStrings;
  llvm::transform(AllowedStages, std::back_inserter(StageStrings),
                  [](HLSLShaderAttr::ShaderType ST) {
                    return StringRef(
                        HLSLShaderAttr::ConvertShaderTypeToStr(ST));
                  });
  Diag(A->getLoc(), diag::err_hlsl_attr_unsupported_in_stage)
      << A << HLSLShaderAttr::ConvertShaderTypeToStr(Stage)
      << (AllowedStages.size() != 1) << join(StageStrings, ", ");
}

namespace {

/// This class implements HLSL availability diagnostics for default
/// and relaxed mode
///
/// The goal of this diagnostic is to emit an error or warning when an
/// unavailable API is found in a code that is reachable from the shader
/// entry function or from an exported function (when compiling shader
/// library).
///
/// This is done by traversing the AST of all shader entry point functions
/// and of all exported functions, and any functions that are refrenced
/// from this AST. In other words, any function that are reachable from
/// the entry points.
class DiagnoseHLSLAvailability
    : public RecursiveASTVisitor<DiagnoseHLSLAvailability> {
  // HEKOTAS this is probably not needed
  // typedef RecursiveASTVisitor<DiagnoseHLSLAvailability> Base;

  Sema &SemaRef;

  // Stack of functions to be scaned
  llvm::SmallVector<const FunctionDecl *, 8> DeclsToScan;

  // List of functions that were already scanned and in which environment.
  //
  // Maps FunctionDecl to a unsigned number that represents a set of shader
  // environments the function has been scanned for.
  // Since HLSLShaderAttr::ShaderType enum is generated from Attr.td and is
  // defined without any assigned values, it is guaranteed to be numbered
  // sequentially from 0 up and we can use it to 'index' individual bits
  // in the set.
  // The N'th bit in the set will be set if the function has been scanned
  // in shader environment whose ShaderType integer value equals N.
  // For example, if a function has been scanned in compute and pixel stage
  // environment, the value will be 0x21 (100001 binary) because
  // (int)HLSLShaderAttr::ShaderType::Pixel == 1 and
  // (int)HLSLShaderAttr::ShaderType::Compute == 5.
  llvm::DenseMap<const FunctionDecl *, unsigned> ScannedDecls;

  // Do not access these directly, use the get/set methods below to make
  // sure the values are in sync
  llvm::Triple::EnvironmentType CurrentShaderEnvironment;
  unsigned CurrentShaderStageBit;

  // True if scanning a function that was already scanned in a different
  // shader stage context, and therefore we should not report issues that
  // depend only on shader model version because they would be duplicate.
  bool ReportOnlyShaderStageIssues;

  void SetShaderStageContext(HLSLShaderAttr::ShaderType ShaderType) {
    assert((((unsigned)1) << (unsigned)ShaderType) != 0 &&
           "ShaderType is too big for this bitmap");
    CurrentShaderEnvironment = HLSLShaderAttr::getTypeAsEnvironment(ShaderType);
    CurrentShaderStageBit = (1 << ShaderType);
  }
  void SetUnknownShaderStageContext() {
    CurrentShaderEnvironment =
        llvm::Triple::EnvironmentType::UnknownEnvironment;
    CurrentShaderStageBit = (1 << 31);
  }
  llvm::Triple::EnvironmentType GetCurrentShaderEnvironment() {
    return CurrentShaderEnvironment;
  }
  bool InUnknownShaderStageContext() {
    return CurrentShaderEnvironment ==
           llvm::Triple::EnvironmentType::UnknownEnvironment;
  }

  // Scanning methods
  void HandleFunctionOrMethodRef(FunctionDecl *FD, Expr *RefExpr);
  void CheckDeclAvailability(NamedDecl *D, const AvailabilityAttr *AA,
                             SourceRange Range);
  const AvailabilityAttr *FindAvailabilityAttr(const Decl *D);
  bool HasMatchingEnvironmentOrNone(const AvailabilityAttr *AA);
  bool WasAlreadyScannedInCurrentShaderStage(const FunctionDecl *FD,
                                             bool *WasNeverScanned = nullptr);
  void AddToScannedFunctions(const FunctionDecl *FD);

public:
  DiagnoseHLSLAvailability(Sema &SemaRef) : SemaRef(SemaRef) {}

  // AST traversal methods
  void RunOnTranslationUnit(const TranslationUnitDecl *TU);
  void RunOnFunction(const FunctionDecl *FD);

  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    FunctionDecl *FD = llvm::dyn_cast<FunctionDecl>(DRE->getDecl());
    if (FD)
      HandleFunctionOrMethodRef(FD, DRE);
    return true;
  }

  bool VisitMemberExpr(MemberExpr *ME) {
    FunctionDecl *FD = llvm::dyn_cast<FunctionDecl>(ME->getMemberDecl());
    if (FD)
      HandleFunctionOrMethodRef(FD, ME);
    return true;
  }
};

// Returns true if the function has already been scanned in the current
// shader environment. WasNeverScanned will be set to true if the function
// has never been scanned before for any shader environment.
bool DiagnoseHLSLAvailability::WasAlreadyScannedInCurrentShaderStage(
    const FunctionDecl *FD, bool *WasNeverScanned) {
  const unsigned &ScannedStages = ScannedDecls.getOrInsertDefault(FD);
  if (WasNeverScanned)
    *WasNeverScanned = (ScannedStages == 0);
  return ScannedStages & CurrentShaderStageBit;
}

// Marks the function as scanned in the current shader environment
void DiagnoseHLSLAvailability::AddToScannedFunctions(const FunctionDecl *FD) {
  unsigned &ScannedStages = ScannedDecls.getOrInsertDefault(FD);
  ScannedStages |= CurrentShaderStageBit;
}

void DiagnoseHLSLAvailability::HandleFunctionOrMethodRef(FunctionDecl *FD,
                                                         Expr *RefExpr) {
  assert((isa<DeclRefExpr>(RefExpr) || isa<MemberExpr>(RefExpr)) &&
         "expected DeclRefExpr or MemberExpr");

  // has a definition -> add to stack to be scanned
  const FunctionDecl *FDWithBody = nullptr;
  if (FD->hasBody(FDWithBody)) {
    if (!WasAlreadyScannedInCurrentShaderStage(FDWithBody))
      DeclsToScan.push_back(FDWithBody);
    return;
  }

  // no body -> diagnose availability
  const AvailabilityAttr *AA = FindAvailabilityAttr(FD);
  if (AA)
    CheckDeclAvailability(
        FD, AA, SourceRange(RefExpr->getBeginLoc(), RefExpr->getEndLoc()));
}

void DiagnoseHLSLAvailability::RunOnTranslationUnit(
    const TranslationUnitDecl *TU) {
  // Iterate over all shader entry functions and library exports, and for those
  // that have a body (definiton), run diag scan on each, setting appropriate
  // shader environment context based on whether it is a shader entry function
  // or an exported function.
  for (auto &D : TU->decls()) {
    const FunctionDecl *FD = llvm::dyn_cast<FunctionDecl>(D);
    if (!FD || !FD->isThisDeclarationADefinition())
      continue;

    // shader entry point
    auto ShaderAttr = FD->getAttr<HLSLShaderAttr>();
    if (ShaderAttr) {
      SetShaderStageContext(ShaderAttr->getType());
      RunOnFunction(FD);
      continue;
    }
    // exported library function with definition
    // FIXME: tracking issue #92073
#if 0
    if (FD->getFormalLinkage() == Linkage::External) {
      SetUnknownShaderStageContext();
      RunOnFunction(FD);
    }
#endif
  }
}

void DiagnoseHLSLAvailability::RunOnFunction(const FunctionDecl *FD) {
  assert(DeclsToScan.empty() && "DeclsToScan should be empty");
  DeclsToScan.push_back(FD);

  while (!DeclsToScan.empty()) {
    // Take one decl from the stack and check it by traversing its AST.
    // For any CallExpr found during the traversal add it's callee to the top of
    // the stack to be processed next. Functions already processed are stored in
    // ScannedDecls.
    const FunctionDecl *FD = DeclsToScan.back();
    DeclsToScan.pop_back();

    // Decl was already scanned
    bool WasNeverScanned;
    if (WasAlreadyScannedInCurrentShaderStage(FD, &WasNeverScanned))
      continue;

    ReportOnlyShaderStageIssues = !WasNeverScanned;

    AddToScannedFunctions(FD);

    Stmt *Body = FD->getBody();
    assert(Body && "full definition with body expected here");

    TraverseStmt(Body);
  }
}

bool DiagnoseHLSLAvailability::HasMatchingEnvironmentOrNone(
    const AvailabilityAttr *AA) {
  IdentifierInfo *IIEnvironment = AA->getEnvironment();
  if (!IIEnvironment)
    return true;

  llvm::Triple::EnvironmentType CurrentEnv = GetCurrentShaderEnvironment();
  if (CurrentEnv == llvm::Triple::UnknownEnvironment)
    return false;

  llvm::Triple::EnvironmentType AttrEnv =
      AvailabilityAttr::getEnvironmentType(IIEnvironment->getName());

  return CurrentEnv == AttrEnv;
}

const AvailabilityAttr *
DiagnoseHLSLAvailability::FindAvailabilityAttr(const Decl *D) {
  AvailabilityAttr const *PartialMatch = nullptr;
  // Check each AvailabilityAttr to find the one for this platform.
  // For multiple attributes with the same platform try to find one for this
  // environment.
  for (const auto *A : D->attrs()) {
    if (const auto *Avail = dyn_cast<AvailabilityAttr>(A)) {
      StringRef AttrPlatform = Avail->getPlatform()->getName();
      StringRef TargetPlatform =
          SemaRef.getASTContext().getTargetInfo().getPlatformName();

      // Match the platform name.
      if (AttrPlatform == TargetPlatform) {
        // Find the best matching attribute for this environment
        if (HasMatchingEnvironmentOrNone(Avail))
          return Avail;
        PartialMatch = Avail;
      }
    }
  }
  return PartialMatch;
}

// Check availability against target shader model version and current shader
// stage and emit diagnostic
void DiagnoseHLSLAvailability::CheckDeclAvailability(NamedDecl *D,
                                                     const AvailabilityAttr *AA,
                                                     SourceRange Range) {

  IdentifierInfo *IIEnv = AA->getEnvironment();

  if (!IIEnv) {
    // The availability attribute does not have environment -> it depends only
    // on shader model version and not on specific the shader stage.

    // Skip emitting the diagnostics if the diagnostic mode is not set to
    // strict (-fhlsl-strict-availability) because all relevant diagnostics
    // were already emitted in the DiagnoseUnguardedAvailability scan
    // (SemaAvailability.cpp).
    if (SemaRef.getLangOpts().HLSLStrictAvailability)
      return;

    // Do not report shader-stage-independent issues if scanning a function
    // that was already scanned in a different shader stage context (they would
    // be duplicate)
    if (ReportOnlyShaderStageIssues)
      return;

  } else {
    // The availability attribute has environment -> we need to know
    // the current stage context to property diagnose it.
    if (InUnknownShaderStageContext())
      return;
  }

  // Check introduced version and if environment matches
  bool EnvironmentMatches = HasMatchingEnvironmentOrNone(AA);
  VersionTuple Introduced = AA->getIntroduced();
  VersionTuple TargetVersion =
      SemaRef.Context.getTargetInfo().getPlatformMinVersion();

  if (TargetVersion >= Introduced && EnvironmentMatches)
    return;

  // Emit diagnostic message
  const TargetInfo &TI = SemaRef.getASTContext().getTargetInfo();
  llvm::StringRef PlatformName(
      AvailabilityAttr::getPrettyPlatformName(TI.getPlatformName()));

  llvm::StringRef CurrentEnvStr = AvailabilityAttr::getPrettyEnviromentName(
      AvailabilityAttr::getEnvironmentString(GetCurrentShaderEnvironment()));

  bool UseEnvironment = (IIEnv != nullptr);
  llvm::StringRef AttrEnvStr =
      UseEnvironment
          ? AvailabilityAttr::getPrettyEnviromentName(IIEnv->getName())
          : "";

  if (EnvironmentMatches) {
    SemaRef.Diag(Range.getBegin(), diag::warn_hlsl_availability)
        << Range << D << PlatformName << Introduced.getAsString()
        << UseEnvironment << CurrentEnvStr;
  } else {
    SemaRef.Diag(Range.getBegin(), diag::warn_hlsl_availability_unavailable)
        << Range << D;
  }

  SemaRef.Diag(D->getLocation(), diag::note_partial_availability_specified_here)
      << D << PlatformName << Introduced.getAsString()
      << SemaRef.Context.getTargetInfo().getPlatformMinVersion().getAsString()
      << UseEnvironment << AttrEnvStr << CurrentEnvStr;
}

} // namespace

void SemaHLSL::DiagnoseAvailabilityViolations(TranslationUnitDecl *TU) {
  // Skip running the diagnostics scan if the diagnostic mode is
  // strict (-fhlsl-strict-availability) and the target shader stage is known
  // because all relevant diagnostics were already emitted in the
  // DiagnoseUnguardedAvailability scan (SemaAvailability.cpp).
  const TargetInfo &TI = SemaRef.getASTContext().getTargetInfo();
  if (SemaRef.getLangOpts().HLSLStrictAvailability &&
      TI.getTriple().getEnvironment() != llvm::Triple::EnvironmentType::Library)
    return;

  DiagnoseHLSLAvailability(SemaRef).RunOnTranslationUnit(TU);
}
