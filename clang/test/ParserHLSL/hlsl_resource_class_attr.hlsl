// RUN: %clang_cc1 -triple dxil-pc-shadermodel6.0-compute -x hlsl -ast-dump -o - %s | FileCheck %s

// CHECK: CXXRecordDecl 0x{{[0-9a-f]+}} {{.*}} struct MyBuffer definition
// CHECK{LITERAL}: <line:6:3, col:51> col:51 h '__hlsl_resource_t [[hlsl::resource_class(UAV)]]':'__hlsl_resource_t'
struct MyBuffer {
  __hlsl_resource_t [[hlsl::resource_class(UAV)]] h;
};

// CHECK{LITERAL}: <line:10:1, col:49> col:49 res '__hlsl_resource_t [[hlsl::resource_class(SRV)]]':'__hlsl_resource_t'
__hlsl_resource_t [[hlsl::resource_class(SRV)]] res;

// CHECK: FunctionDecl 0x{{[0-9a-f]+}} <line:14:1, line:16:1> line:14:6 f 'void ()
// CHECK{LITERAL}: <col:3, col:55> col:55 r '__hlsl_resource_t [[hlsl::resource_class(Sampler)]]':'__hlsl_resource_t'
void f() {
  __hlsl_resource_t [[hlsl::resource_class(Sampler)]] r;
}
