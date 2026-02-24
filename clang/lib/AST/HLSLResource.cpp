//===--- HLSLResource.cpp - Routines for HLSL resources and bindings
//-------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides shared routines to help analyze HLSL resources and
// theirs bindings during Sema and CodeGen.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/HLSLResource.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Type.h"

using namespace clang;

namespace clang {
namespace hlsl {

void EmbeddedResourceNameBuilder::pushName(llvm::StringRef N,
                                           llvm::StringRef Delim) {
  Offsets.push_back(Name.size());
  if (!Name.empty())
    Name.append(Delim);
  Name.append(N);
}

void EmbeddedResourceNameBuilder::pushBaseNameHierarchy(
    CXXRecordDecl *DerivedRD, CXXRecordDecl *BaseRD) {
  Offsets.push_back(Name.size());
  while (BaseRD != DerivedRD) {
    assert(DerivedRD->getNumBases() == 1 && "exactly one base expected");
    const auto *BasesIt = DerivedRD->bases_begin();
    DerivedRD = BasesIt->getType()->getAsCXXRecordDecl();
    Name.append(BaseDelim);
    Name.append(DerivedRD->getName());
  }
}

} // namespace hlsl
} // namespace clang
