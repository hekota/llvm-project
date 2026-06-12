// RUN: %clang_cc1 -triple dxil-pc-shadermodel6.6-library -emit-llvm -disable-llvm-passes -o - %s | llvm-cxxfilt | FileCheck %s

struct MyStruct {
  int a;
  RWBuffer<float> Buf;
};

struct WrappaStruct {
  float b;
  MyStruct s[2];
  StructuredBuffer<int> BufArray[2];
};

cbuffer CB {
  MyStruct cbs;
  WrappaStruct cbw;
}

void useMyStruct(MyStruct s) {}

void useWrappaStruct(WrappaStruct w) {}

// Simple struct with one resource - local initialization
// CHECK-LABEL: case1
void case1() {
// CHECK: %s1 = alloca %struct.MyStruct, align 4

// s1.a - copy from cbuffer
// CHECK-NEXT: [[S1_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr %s1, i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_A:%.*]] = load i32, ptr addrspace(2) @cbs, align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_A]], ptr [[S1_A_PTR]], align 4

// s1.Buf - use global resource @cbs.Buf
// CHECK-NEXT: [[S1_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr %s1, i32 0, i32 1
// CHECK-NEXT: store ptr @cbs.Buf, ptr [[S1_BUF_PTR]], align 4

  MyStruct s1 = cbs;
}

// Simple struct with one resource - assignment
// CHECK-LABEL: case2
void case2() {
// CHECK: %s2 = alloca %struct.MyStruct, align 4
// CHECK-NEXT: [[TMP:%.*]] = alloca %struct.MyStruct, align 4

// s2.a - copy from cbuffer
// CHECK-NEXT: [[S2_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr %s2, i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_A:%.*]] = load i32, ptr addrspace(2) @cbs, align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_A]], ptr [[S2_A_PTR]], align 4

// s2.Buf - use global resource @cbs.Buf
// CHECK-NEXT: [[S2_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr %s2, i32 0, i32 1
// CHECK-NEXT: store ptr @cbs.Buf, ptr [[S2_BUF_PTR]], align 4

// result of the assignment expression passed along in a temporary
// CHECK-NEXT: call void @llvm.memcpy.p0.p0.i32(ptr align 4 [[TMP]], ptr align 4 %s2, i32 8, i1 false)

  MyStruct s2;
  s2 = cbs;
}

// Simple struct with one resource - function argument from cbuffer
// CHECK-LABEL: case3
void case3() {
// CHECK: [[TMP:%.*]] = alloca %struct.MyStruct, align 4

// tmp.a - copy from cbuffer
// CHECK-NEXT: [[TMP_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[TMP]], i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_A:%.*]] = load i32, ptr addrspace(2) @cbs, align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_A]], ptr [[TMP_A_PTR]], align 4

// tmp.Buf - use global resource @cbs.Buf
// CHECK-NEXT: [[TMP_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[TMP]], i32 0, i32 1
// CHECK-NEXT: store ptr @cbs.Buf, ptr [[TMP_BUF_PTR]], align 4

// call useMyStruct with the temporary
// CHECK-NEXT: call void @useMyStruct(MyStruct)(ptr noundef dead_on_return [[TMP]])

  useMyStruct(cbs); 
}

// Complex struct with multiple resources and arrays - local initialization from cbuffer
void case4() {
// CHECK: %w1 = alloca %struct.WrappaStruct, align 1

// w1.b - copy from cbuffer
// CHECK-NEXT: [[W1_B_PTR:%.*]] = getelementptr inbounds %struct.WrappaStruct, ptr %w1, i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_B:%.*]] = load float, ptr addrspace(2) @cbw, align 4
// CHECK-NEXT: store float [[CBUF_LOAD_B]], ptr [[W1_B_PTR]], align 4

// w1.s
// CHECK-NEXT: [[W1_S_PTR:%.*]] = getelementptr inbounds %struct.WrappaStruct, ptr %w1, i32 0, i32 1

// w1.s[0]
// CHECK-NEXT: [[W1_S_0_PTR:%.*]] = getelementptr inbounds [2 x %struct.MyStruct], ptr [[W1_S_PTR]], i32 0, i32 0

// w1.s[0].a - copy from cbuffer
// CHECK-NEXT: [[W1_S_0_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[W1_S_0_PTR]], i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_S_0_A:%.*]] = load i32, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) @cbw, i32 16), align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_S_0_A]], ptr [[W1_S_0_A_PTR]], align 4
  
// w1.s[0].Buf - use global resource @cbw.s[0].Buf
// CHECK-NEXT: [[W1_S_0_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[W1_S_0_PTR]], i32 0, i32 1
// CHECK-NEXT: store ptr @cbw.s.0.Buf, ptr [[W1_S_0_BUF_PTR]], align 4

// w1.s[1]
// CHECK-NEXT: [[W1_S_1_PTR:%.*]] = getelementptr inbounds [2 x %struct.MyStruct], ptr [[W1_S_PTR]], i32 0, i32 1

// w1.s[1].a - copy from cbuffer
// CHECK-NEXT: [[W1_S_1_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[W1_S_1_PTR]], i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_S_1_A:%.*]] = load i32, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) @cbw, i32 32), align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_S_1_A]], ptr [[W1_S_1_A_PTR]], align 4

// w1.s[1].Buf - use global resource @cbw.s.1.Buf
// CHECK-NEXT: [[W1_S_1_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[W1_S_1_PTR]], i32 0, i32 1
// CHECK-NEXT: store ptr @cbw.s.1.Buf, ptr [[W1_S_1_BUF_PTR]], align 4

// w1.BufArray
// CHECK-NEXT: [[W1_BUFARRAY_PTR:%.*]] = getelementptr inbounds %struct.WrappaStruct, ptr %w1, i32 0, i32 2

// w1.BufArray[0] - initialize resource array element from binding
// CHECK-NEXT: [[W1_BUFARRAY_0_PTR:%.*]] = getelementptr [2 x %"class.hlsl::StructuredBuffer"], ptr [[W1_BUFARRAY_PTR]], i32 0, i32 0
// CHECK-NEXT: call void @hlsl::StructuredBuffer<int>::__createFromImplicitBinding(unsigned int, unsigned int, int, unsigned int, char const*)
// CHECK-SAME: (ptr {{.*}} [[W1_BUFARRAY_0_PTR]], i32 noundef 3, i32 noundef 0, i32 noundef 2, i32 noundef 0, ptr noundef @cbw.BufArray.str)

// w1.BufArray[1] - initialize resource array element from binding
// CHECK-NEXT: [[W1_BUFARRAY_1_PTR:%.*]] = getelementptr [2 x %"class.hlsl::StructuredBuffer"], ptr [[W1_BUFARRAY_PTR]], i32 0, i32 1
// CHECK-NEXT: call void @hlsl::StructuredBuffer<int>::__createFromImplicitBinding(unsigned int, unsigned int, int, unsigned int, char const*)
// CHECK-SAME: (ptr {{.*}} [[W1_BUFARRAY_1_PTR]], i32 noundef 3, i32 noundef 0, i32 noundef 2, i32 noundef 1, ptr noundef @cbw.BufArray.str)

  WrappaStruct w1 = cbw;
}

// Complex struct with multiple resources and arrays - assignment from cbuffer
// CHECK-LABEL: case5
void case5() {
// CHECK: %w2 = alloca %struct.WrappaStruct, align 1
// CHECK-NEXT: [[TMP:%.*]] = alloca %struct.WrappaStruct, align 1

// w2.b - copy from cbuffer
// CHECK-NEXT: [[W2_B_PTR:%.*]] = getelementptr inbounds %struct.WrappaStruct, ptr %w2, i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_B:%.*]] = load float, ptr addrspace(2) @cbw, align 4
// CHECK-NEXT: store float [[CBUF_LOAD_B]], ptr [[W2_B_PTR]], align 4

// w2.s
// CHECK-NEXT: [[W2_S_PTR:%.*]] = getelementptr inbounds %struct.WrappaStruct, ptr %w2, i32 0, i32 1

// w2.s[0]
// CHECK-NEXT: [[W2_S_0_PTR:%.*]] = getelementptr inbounds [2 x %struct.MyStruct], ptr [[W2_S_PTR]], i32 0, i32 0

// w2.s[0].a - copy from cbuffer
// CHECK-NEXT: [[W2_S_0_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[W2_S_0_PTR]], i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_S_0_A:%.*]] = load i32, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) @cbw, i32 16), align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_S_0_A]], ptr [[W2_S_0_A_PTR]], align 4
  
// w2.s[0].Buf - use global resource @cbw.s[0].Buf
// CHECK-NEXT: [[W2_S_0_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[W2_S_0_PTR]], i32 0, i32 1
// CHECK-NEXT: store ptr @cbw.s.0.Buf, ptr [[W2_S_0_BUF_PTR]], align 4

// w2.s[1]
// CHECK-NEXT: [[W2_S_1_PTR:%.*]] = getelementptr inbounds [2 x %struct.MyStruct], ptr [[W2_S_PTR]], i32 0, i32 1

// w2.s[1].a - copy from cbuffer
// CHECK-NEXT: [[W2_S_1_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[W2_S_1_PTR]], i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_S_1_A:%.*]] = load i32, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) @cbw, i32 32), align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_S_1_A]], ptr [[W2_S_1_A_PTR]], align 4

// w2.s[1].Buf - use global resource @cbw.s.1.Buf
// CHECK-NEXT: [[W2_S_1_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[W2_S_1_PTR]], i32 0, i32 1
// CHECK-NEXT: store ptr @cbw.s.1.Buf, ptr [[W2_S_1_BUF_PTR]], align 4

// w2.BufArray
// CHECK-NEXT: [[W2_BUFARRAY_PTR:%.*]] = getelementptr inbounds %struct.WrappaStruct, ptr %w2, i32 0, i32 2

// w2.BufArray[0] - initialize resource array element from binding
// CHECK-NEXT: [[W2_BUFARRAY_0_PTR:%.*]] = getelementptr [2 x %"class.hlsl::StructuredBuffer"], ptr [[W2_BUFARRAY_PTR]], i32 0, i32 0
// CHECK-NEXT: call void @hlsl::StructuredBuffer<int>::__createFromImplicitBinding(unsigned int, unsigned int, int, unsigned int, char const*)
// CHECK-SAME: (ptr {{.*}} [[W2_BUFARRAY_0_PTR]], i32 noundef 3, i32 noundef 0, i32 noundef 2, i32 noundef 0, ptr noundef @cbw.BufArray.str)

// w2.BufArray[1] - initialize resource array element from binding
// CHECK-NEXT: [[W2_BUFARRAY_1_PTR:%.*]] = getelementptr [2 x %"class.hlsl::StructuredBuffer"], ptr [[W2_BUFARRAY_PTR]], i32 0, i32 1
// CHECK-NEXT: call void @hlsl::StructuredBuffer<int>::__createFromImplicitBinding(unsigned int, unsigned int, int, unsigned int, char const*)
// CHECK-SAME: (ptr {{.*}} [[W2_BUFARRAY_1_PTR]], i32 noundef 3, i32 noundef 0, i32 noundef 2, i32 noundef 1, ptr noundef @cbw.BufArray.str)

// result of the assignment expression passed along in a temporary
// CHECK-NEXT: call void @llvm.memcpy.p0.p0.i32(ptr align 1 [[TMP]], ptr align 1 %w2, i32 28, i1 false)

  WrappaStruct w2;
  w2 = cbw;
}

// Complex struct with multiple resources and arrays - function argument from cbuffer
// CHECK-LABEL: case6
void case6() {
// CHECK: [[TMP:%.*]] = alloca %struct.WrappaStruct, align 1

// tmp.b - copy from cbuffer
// CHECK-NEXT: [[TMP_B_PTR:%.*]] = getelementptr inbounds %struct.WrappaStruct, ptr [[TMP]], i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_B:%.*]] = load float, ptr addrspace(2) @cbw, align 4
// CHECK-NEXT: store float [[CBUF_LOAD_B]], ptr [[TMP_B_PTR]], align 4

// tmp.s
// CHECK-NEXT: [[TMP_S_PTR:%.*]] = getelementptr inbounds %struct.WrappaStruct, ptr [[TMP]], i32 0, i32 1

// tmp.s[0]
// CHECK-NEXT: [[TMP_S_0_PTR:%.*]] = getelementptr inbounds [2 x %struct.MyStruct], ptr [[TMP_S_PTR]], i32 0, i32 0

// tmp.s[0].a - copy from cbuffer
// CHECK-NEXT: [[TMP_S_0_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[TMP_S_0_PTR]], i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_S_0_A:%.*]] = load i32, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) @cbw, i32 16), align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_S_0_A]], ptr [[TMP_S_0_A_PTR]], align 4
  
// tmp.s[0].Buf - use global resource @cbw.s[0].Buf
// CHECK-NEXT: [[TMP_S_0_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[TMP_S_0_PTR]], i32 0, i32 1
// CHECK-NEXT: store ptr @cbw.s.0.Buf, ptr [[TMP_S_0_BUF_PTR]], align 4

// tmp.s[1]
// CHECK-NEXT: [[TMP_S_1_PTR:%.*]] = getelementptr inbounds [2 x %struct.MyStruct], ptr [[TMP_S_PTR]], i32 0, i32 1

// tmp.s[1].a - copy from cbuffer
// CHECK-NEXT: [[TMP_S_1_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[TMP_S_1_PTR]], i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_S_1_A:%.*]] = load i32, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) @cbw, i32 32), align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_S_1_A]], ptr [[TMP_S_1_A_PTR]], align 4

// tmp.s[1].Buf - use global resource @cbw.s.1.Buf
// CHECK-NEXT: [[TMP_S_1_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[TMP_S_1_PTR]], i32 0, i32 1
// CHECK-NEXT: store ptr @cbw.s.1.Buf, ptr [[TMP_S_1_BUF_PTR]], align 4

// tmp.BufArray
// CHECK-NEXT: [[TMP_BUFARRAY_PTR:%.*]] = getelementptr inbounds %struct.WrappaStruct, ptr [[TMP]], i32 0, i32 2

// tmp.BufArray[0] - initialize resource array element from binding
// CHECK-NEXT: [[TMP_BUFARRAY_0_PTR:%.*]] = getelementptr [2 x %"class.hlsl::StructuredBuffer"], ptr [[TMP_BUFARRAY_PTR]], i32 0, i32 0
// CHECK-NEXT: call void @hlsl::StructuredBuffer<int>::__createFromImplicitBinding(unsigned int, unsigned int, int, unsigned int, char const*)
// CHECK-SAME: (ptr {{.*}} [[TMP_BUFARRAY_0_PTR]], i32 noundef 3, i32 noundef 0, i32 noundef 2, i32 noundef 0, ptr noundef @cbw.BufArray.str)

// tmp.BufArray[1] - initialize resource array element from binding
// CHECK-NEXT: [[TMP_BUFARRAY_1_PTR:%.*]] = getelementptr [2 x %"class.hlsl::StructuredBuffer"], ptr [[TMP_BUFARRAY_PTR]], i32 0, i32 1
// CHECK-NEXT: call void @hlsl::StructuredBuffer<int>::__createFromImplicitBinding(unsigned int, unsigned int, int, unsigned int, char const*)
// CHECK-SAME: (ptr {{.*}} [[TMP_BUFARRAY_1_PTR]], i32 noundef 3, i32 noundef 0, i32 noundef 2, i32 noundef 1, ptr noundef @cbw.BufArray.str)

// call useWrappaStruct with the temporary
// CHECK-NEXT: call void @useWrappaStruct(WrappaStruct)(ptr noundef dead_on_return [[TMP]])

  useWrappaStruct(cbw);
}

// Member access in a complex cbuffer struct with resources - local initialization
// CHECK-LABEL: case7
void case7() {
// CHECK: %s3 = alloca %struct.MyStruct, align 4

// s3.a - copy from cbuffer (cbw.s[0].a)
// CHECK-NEXT: [[S3_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr %s3, i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_S_0_A:%.*]] = load i32, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) @cbw, i32 16), align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_S_0_A]], ptr [[S3_A_PTR]], align 4

// s3.Buf - use global resource @cbw.s.0.Buf
// CHECK-NEXT: [[S3_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr %s3, i32 0, i32 1
// CHECK-NEXT: store ptr @cbw.s.0.Buf, ptr [[S3_BUF_PTR]], align 4
  MyStruct s3 = cbw.s[0];
}

// Member access in a complex cbuffer struct with resources - assignment
// CHECK-LABEL: case8
void case8() {
// CHECK: %s4 = alloca %struct.MyStruct, align 4
// CHECK: [[TMP:%.*]] = alloca %struct.MyStruct, align 4

// s4.a - copy from cbuffer (cbw.s[1].a)
// CHECK-NEXT: [[S4_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr %s4, i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_S_1_A:%.*]] = load i32, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) @cbw, i32 32), align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_S_1_A]], ptr [[S4_A_PTR]], align 4

// s4.Buf - use global resource @cbw.s.1.Buf
// CHECK-NEXT: [[S4_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr %s4, i32 0, i32 1
// CHECK-NEXT: store ptr @cbw.s.1.Buf, ptr [[S4_BUF_PTR]], align 4

// result of the assignment expression passed along in a temporary
// CHECK-NEXT: call void @llvm.memcpy.p0.p0.i32(ptr align 4 [[TMP]], ptr align 4 %s4, i32 8, i1 false)

  MyStruct s4;
  s4 = cbw.s[1];
}

// Member access in a complex cbuffer struct with resources - function argument
// CHECK-LABEL: case9
void case9() {
// CHECK: [[TMP:%.*]] = alloca %struct.MyStruct, align 4

// tmp.a - copy from cbuffer (cbw.s[0].a)
// CHECK-NEXT: [[TMP_A_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[TMP]], i32 0, i32 0
// CHECK-NEXT: [[CBUF_LOAD_S_0_A:%.*]] = load i32, ptr addrspace(2) getelementptr inbounds nuw (i8, ptr addrspace(2) @cbw, i32 16), align 4
// CHECK-NEXT: store i32 [[CBUF_LOAD_S_0_A]], ptr [[TMP_A_PTR]], align 4

// tmp.Buf - use global resource @cbw.s.0.Buf
// CHECK-NEXT: [[TMP_BUF_PTR:%.*]] = getelementptr inbounds %struct.MyStruct, ptr [[TMP]], i32 0, i32 1
// CHECK-NEXT: store ptr @cbw.s.0.Buf, ptr [[TMP_BUF_PTR]], align 4

// call useMyStruct with the temporary
// CHECK-NEXT: call void @useMyStruct(MyStruct)(ptr noundef dead_on_return [[TMP]])

  useMyStruct(cbw.s[0]);
}
