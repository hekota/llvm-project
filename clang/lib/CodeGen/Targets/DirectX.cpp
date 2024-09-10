//===- DirectX.cpp---------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ABIInfoImpl.h"
#include "TargetInfo.h"
#include "clang/AST/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/DXILABI.h"
#include "llvm/Support/ErrorHandling.h"

using namespace clang;
using namespace clang::CodeGen;

//===----------------------------------------------------------------------===//
// Target codegen info implementation for DirectX.
//===----------------------------------------------------------------------===//

namespace {

class DirectXTargetCodeGenInfo : public TargetCodeGenInfo {
public:
  DirectXTargetCodeGenInfo(CodeGen::CodeGenTypes &CGT)
      : TargetCodeGenInfo(std::make_unique<DefaultABIInfo>(CGT)) {}

  llvm::Type *getHLSLType(CodeGenModule &CGM, const Type *T) const override;
};

llvm::Type *DirectXTargetCodeGenInfo::getHLSLType(CodeGenModule &CGM,
                                                  const Type *Ty) const {
  const HLSLAttributedResourceType *ResType =
      dyn_cast<HLSLAttributedResourceType>(Ty);
  if (!ResType)
    return nullptr;

  llvm::LLVMContext &Ctx = CGM.getLLVMContext();
  const HLSLAttributedResourceType::Attributes &ResAttrs = ResType->getAttrs();
  switch (ResAttrs.ResourceClass) {
  case llvm::dxil::ResourceClass::UAV:
  case llvm::dxil::ResourceClass::SRV: {
    // convert element type
    QualType ContainedTy = ResAttrs.ContainedType;
    llvm::Type *ElemType = nullptr;
    if (!ContainedTy.isNull())
      ElemType = CGM... ConvertType(ContainedTy);

    if (ResAttrs.RowAccess) {
      unsigned Flags[] = {/*IsWriteable*/ ResAttrs.ResourceClass ==
                              llvm::dxil::ResourceClass::UAV,
                          /*IsROV*/ ResAttrs.IsROV,
                          /*IsSigned*/ ContainedTy->isSignedIntegerType()};
      return llvm::TargetExtType::get(Ctx, "dx.TypedBuffer", {ElemType}, Flags);
    }

    unsigned Flags[] = {/*IsWriteable*/ ResAttrs.ResourceClass ==
                            llvm::dxil::ResourceClass::UAV,
                        /*IsROV*/ ResAttrs.IsROV};
    return llvm::TargetExtType::get(Ctx, "dx.RawBuffer", {ElemType}, Flags);
  }
  case llvm::dxil::ResourceClass::CBuffer:
    llvm_unreachable("dx.CBuffer handles are not implemented yet");
    break;
  case llvm::dxil::ResourceClass::Sampler:
    llvm_unreachable("dx.Sampler handles are not implemented yet");
    break;
  }
}

} // namespace

std::unique_ptr<TargetCodeGenInfo>
CodeGen::createDirectXTargetCodeGenInfo(CodeGenModule &CGM) {
  return std::make_unique<DirectXTargetCodeGenInfo>(CGM.getTypes());
}
