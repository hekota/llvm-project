// RUN: %clang_cc1 -triple dxil-pc-shadermodel6.0-compute -x hlsl -ast-dump -o - %s | FileCheck %s

struct SomeType {
  int i;  
};

// CHECK: -VarDecl 0x{{[0-9a-f]+}} <line:8:1, col:27> col:27 e1 'SomeType __attribute__((HLSLROVAttr))':'SomeType' callinit
SomeType [[hlsl::is_rov]] e1;
