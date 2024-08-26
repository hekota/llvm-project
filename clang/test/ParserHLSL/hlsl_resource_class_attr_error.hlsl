// RUN: %clang_cc1 -triple dxil-pc-shadermodel6.0-compute -x hlsl -o - %s -verify

struct SomeType {
  int i;  
};

// expected-error@+1{{'resource_class' attribute requires an identifier}}
SomeType [[hlsl::resource_class()]] e1;

// expected-warning@+1{{ResourceClass attribute argument not supported: gibberish}}
SomeType [[hlsl::resource_class(gibberish)]] e2;
