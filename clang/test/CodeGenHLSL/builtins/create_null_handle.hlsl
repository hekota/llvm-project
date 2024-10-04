// RUN: %clang_cc1 -triple dxil-pc-shadermodel6.3-library -x hlsl -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s

struct MyResource {
  __hlsl_resource_t [[hlsl::resource_class(UAV)]] [[hlsl::contained_type(float)]]h;

  void init() {
    h = __builtin_hlsl_create_null_handle(this);
  }
};

void f() {
  MyResource res;
}

// CHECK: call ptr @llvm.dx.create.handle()
