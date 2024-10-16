; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --check-globals all --global-value-regex "x" --version 4
; RUN: opt < %s -S -passes=hwasan -mtriple=aarch64 -hwasan-globals=0 | FileCheck %s --check-prefixes=NOGLOB
; RUN: opt < %s -S -passes=hwasan -mtriple=aarch64 -hwasan-globals=1 | FileCheck %s

@x = dso_local global i32 0, align 4

;.
; NOGLOB: @x = dso_local global i32 0, align 4
;.
; CHECK: @x = alias i32, inttoptr (i64 add (i64 ptrtoint (ptr @x.hwasan to i64), i64 5260204364768739328) to ptr)
;.
define dso_local noundef i32 @_Z3tmpv() sanitize_hwaddress {
; NOGLOB-LABEL: define dso_local noundef i32 @_Z3tmpv(
; NOGLOB-SAME: ) #[[ATTR0:[0-9]+]] {
; NOGLOB-NEXT:  entry:
; NOGLOB-NEXT:    [[TMP12:%.*]] = load i64, ptr @__hwasan_tls, align 8
; NOGLOB-NEXT:    [[TMP1:%.*]] = or i64 [[TMP12]], 4294967295
; NOGLOB-NEXT:    [[HWASAN_SHADOW:%.*]] = add i64 [[TMP1]], 1
; NOGLOB-NEXT:    [[TMP2:%.*]] = inttoptr i64 [[HWASAN_SHADOW]] to ptr
; NOGLOB-NEXT:    [[TMP3:%.*]] = lshr i64 ptrtoint (ptr @x to i64), 56
; NOGLOB-NEXT:    [[TMP4:%.*]] = trunc i64 [[TMP3]] to i8
; NOGLOB-NEXT:    [[TMP5:%.*]] = and i64 ptrtoint (ptr @x to i64), 72057594037927935
; NOGLOB-NEXT:    [[TMP6:%.*]] = lshr i64 [[TMP5]], 4
; NOGLOB-NEXT:    [[TMP7:%.*]] = getelementptr i8, ptr [[TMP2]], i64 [[TMP6]]
; NOGLOB-NEXT:    [[TMP8:%.*]] = load i8, ptr [[TMP7]], align 1
; NOGLOB-NEXT:    [[TMP9:%.*]] = icmp ne i8 [[TMP4]], [[TMP8]]
; NOGLOB-NEXT:    br i1 [[TMP9]], label [[TMP10:%.*]], label [[TMP11:%.*]], !prof [[PROF1:![0-9]+]]
; NOGLOB:       10:
; NOGLOB-NEXT:    call void @llvm.hwasan.check.memaccess.shortgranules(ptr [[TMP2]], ptr @x, i32 2)
; NOGLOB-NEXT:    br label [[TMP11]]
; NOGLOB:       11:
; NOGLOB-NEXT:    [[TMP0:%.*]] = load i32, ptr @x, align 4
; NOGLOB-NEXT:    ret i32 [[TMP0]]
;
; CHECK-LABEL: define dso_local noundef i32 @_Z3tmpv(
; CHECK-SAME: ) #[[ATTR0:[0-9]+]] {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[TMP12:%.*]] = load i64, ptr @__hwasan_tls, align 8
; CHECK-NEXT:    [[TMP1:%.*]] = or i64 [[TMP12]], 4294967295
; CHECK-NEXT:    [[HWASAN_SHADOW:%.*]] = add i64 [[TMP1]], 1
; CHECK-NEXT:    [[TMP2:%.*]] = inttoptr i64 [[HWASAN_SHADOW]] to ptr
; CHECK-NEXT:    [[TMP3:%.*]] = lshr i64 ptrtoint (ptr @x to i64), 56
; CHECK-NEXT:    [[TMP4:%.*]] = trunc i64 [[TMP3]] to i8
; CHECK-NEXT:    [[TMP5:%.*]] = and i64 ptrtoint (ptr @x to i64), 72057594037927935
; CHECK-NEXT:    [[TMP6:%.*]] = lshr i64 [[TMP5]], 4
; CHECK-NEXT:    [[TMP7:%.*]] = getelementptr i8, ptr [[TMP2]], i64 [[TMP6]]
; CHECK-NEXT:    [[TMP8:%.*]] = load i8, ptr [[TMP7]], align 1
; CHECK-NEXT:    [[TMP9:%.*]] = icmp ne i8 [[TMP4]], [[TMP8]]
; CHECK-NEXT:    br i1 [[TMP9]], label [[TMP10:%.*]], label [[TMP11:%.*]], !prof [[PROF2:![0-9]+]]
; CHECK:       10:
; CHECK-NEXT:    call void @llvm.hwasan.check.memaccess.shortgranules(ptr [[TMP2]], ptr @x, i32 2)
; CHECK-NEXT:    br label [[TMP11]]
; CHECK:       11:
; CHECK-NEXT:    [[TMP0:%.*]] = load i32, ptr @x, align 4
; CHECK-NEXT:    ret i32 [[TMP0]]
;
entry:
  %0 = load i32, ptr @x, align 4
  ret i32 %0
}
