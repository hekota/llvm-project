//===----- hlsl_resources.h - HLSL definitions for resources ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _HLSL_HLSL_RESOURCES_H_
#define _HLSL_HLSL_RESOURCES_H_

namespace hlsl {

#define _HLSL_AVAILABILITY(platform, version)                                  \
  __attribute__((availability(platform, introduced = version)))

template <bool IsSamplerHeap> struct DescriptorHeapStruct {
  __hlsl_heap_resource_info operator[](uint32_t Index) {
    return __hlsl_heap_resource_info{Index, IsSamplerHeap};
  }
};

_HLSL_AVAILABILITY(shadermodel, 6.6)
static DescriptorHeapStruct<false> ResourceDescriptorHeap;
_HLSL_AVAILABILITY(shadermodel, 6.6)
static DescriptorHeapStruct<true> SamplerDescriptorHeap;

} // namespace hlsl
#endif //_HLSL_HLSL_RESOURCES_H_
