#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "bpftrace.h"
#include "codegen_llvm.h"
#include "driver.h"
#include "semantic_analyser.h"

namespace bpftrace {
namespace test {
namespace codegen {

using ::testing::_;

std::string header = R"HEAD(; ModuleID = 'bpftrace'
source_filename = "bpftrace"
target datalayout = "e-m:e-p:64:64-i64:64-n32:64-S128"
target triple = "bpf-pc-linux"

)HEAD";

void test(const std::string &input, const std::string expected_output)
{
  BPFtrace bpftrace;
  Driver driver;

  ASSERT_EQ(driver.parse_str(input), 0);

  ast::SemanticAnalyser semantics(driver.root_, bpftrace);
  ASSERT_EQ(semantics.analyse(), 0);
  ASSERT_EQ(semantics.create_maps(), 0);

  std::stringstream out;
  ast::CodegenLLVM codegen(driver.root_, bpftrace);
  ASSERT_EQ(codegen.compile(true, out), 0);

  std::string full_expected_output = header + expected_output;
  EXPECT_EQ(full_expected_output, out.str());
}

TEST(codegen, empty_function)
{
  test("kprobe:f { 1; }",

R"EXPECTED(; Function Attrs: norecurse nounwind readnone
define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr #0 section "kprobe:f" {
entry:
  ret i64 0
}

attributes #0 = { norecurse nounwind readnone }
)EXPECTED");
}

TEST(codegen, map_assign_int)
{
  test("kprobe:f { @x = 1; }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_val" = alloca i64, align 8
  %"@x_key" = alloca i64, align 8
  %1 = bitcast i64* %"@x_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %1)
  store i64 0, i64* %"@x_key", align 8
  %2 = bitcast i64* %"@x_val" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %2)
  store i64 1, i64* %"@x_val", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo, i64* nonnull %"@x_key", i64* nonnull %"@x_val", i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %1)
  call void @llvm.lifetime.end(i64 -1, i8* %2)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, map_assign_string)
{
  test("kprobe:f { @x = \"blah\"; }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i32, i1) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_key" = alloca i64, align 8
  %str = alloca [64 x i8], align 1
  %1 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 0
  call void @llvm.lifetime.start(i64 -1, i8* %1)
  store i8 98, i8* %1, align 1
  %str.repack1 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 1
  store i8 108, i8* %str.repack1, align 1
  %str.repack2 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 2
  store i8 97, i8* %str.repack2, align 1
  %str.repack3 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 3
  store i8 104, i8* %str.repack3, align 1
  %str.repack4 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 4
  %2 = bitcast i64* %"@x_key" to i8*
  call void @llvm.memset.p0i8.i64(i8* %str.repack4, i8 0, i64 60, i32 1, i1 false)
  call void @llvm.lifetime.start(i64 -1, i8* %2)
  store i64 0, i64* %"@x_key", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo, i64* nonnull %"@x_key", [64 x i8]* nonnull %str, i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %2)
  call void @llvm.lifetime.end(i64 -1, i8* %1)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, map_key_int)
{
  test("kprobe:f { @x[11,22,33] = 44 }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_val" = alloca i64, align 8
  %"@x_key" = alloca [24 x i8], align 8
  %1 = getelementptr [24 x i8], [24 x i8]* %"@x_key", i64 0, i64 0
  call void @llvm.lifetime.start(i64 -1, i8* %1)
  store i64 11, i8* %1, align 8
  %2 = getelementptr [24 x i8], [24 x i8]* %"@x_key", i64 0, i64 8
  store i64 22, i8* %2, align 8
  %3 = getelementptr [24 x i8], [24 x i8]* %"@x_key", i64 0, i64 16
  store i64 33, i8* %3, align 8
  %4 = bitcast i64* %"@x_val" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %4)
  store i64 44, i64* %"@x_val", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo, [24 x i8]* nonnull %"@x_key", i64* nonnull %"@x_val", i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %1)
  call void @llvm.lifetime.end(i64 -1, i8* %4)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}
TEST(codegen, map_key_string)
{
  test("kprobe:f { @x[\"a\", \"b\"] = 44 }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i32, i1) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_val" = alloca i64, align 8
  %"@x_key" = alloca [128 x i8], align 1
  %1 = getelementptr inbounds [128 x i8], [128 x i8]* %"@x_key", i64 0, i64 0
  call void @llvm.lifetime.start(i64 -1, i8* %1)
  store i8 97, i8* %1, align 1
  %str.sroa.3.0..sroa_idx = getelementptr inbounds [128 x i8], [128 x i8]* %"@x_key", i64 0, i64 1
  %str1.sroa.0.0..sroa_idx = getelementptr inbounds [128 x i8], [128 x i8]* %"@x_key", i64 0, i64 64
  call void @llvm.memset.p0i8.i64(i8* %str.sroa.3.0..sroa_idx, i8 0, i64 63, i32 1, i1 false)
  store i8 98, i8* %str1.sroa.0.0..sroa_idx, align 1
  %str1.sroa.3.0..sroa_idx = getelementptr inbounds [128 x i8], [128 x i8]* %"@x_key", i64 0, i64 65
  %2 = bitcast i64* %"@x_val" to i8*
  call void @llvm.memset.p0i8.i64(i8* %str1.sroa.3.0..sroa_idx, i8 0, i64 63, i32 1, i1 false)
  call void @llvm.lifetime.start(i64 -1, i8* %2)
  store i64 44, i64* %"@x_val", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo, [128 x i8]* nonnull %"@x_key", i64* nonnull %"@x_val", i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %1)
  call void @llvm.lifetime.end(i64 -1, i8* %2)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, builtin_pid)
{
  test("kprobe:f { @x = pid }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_val" = alloca i64, align 8
  %"@x_key" = alloca i64, align 8
  %get_pid_tgid = tail call i64 inttoptr (i64 14 to i64 ()*)()
  %1 = lshr i64 %get_pid_tgid, 32
  %2 = bitcast i64* %"@x_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %2)
  store i64 0, i64* %"@x_key", align 8
  %3 = bitcast i64* %"@x_val" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %3)
  store i64 %1, i64* %"@x_val", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo, i64* nonnull %"@x_key", i64* nonnull %"@x_val", i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %2)
  call void @llvm.lifetime.end(i64 -1, i8* %3)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, builtin_quantize)
{
  test("kprobe:f { @x = quantize(pid) }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_val" = alloca i64, align 8
  %"@x_key" = alloca i64, align 8
  %get_pid_tgid = tail call i64 inttoptr (i64 14 to i64 ()*)()
  %1 = lshr i64 %get_pid_tgid, 32
  %2 = icmp ugt i64 %get_pid_tgid, 281474976710655
  %3 = zext i1 %2 to i64
  %4 = shl nuw nsw i64 %3, 4
  %5 = lshr i64 %1, %4
  %6 = icmp sgt i64 %5, 255
  %7 = zext i1 %6 to i64
  %8 = shl nuw nsw i64 %7, 3
  %9 = lshr i64 %5, %8
  %10 = or i64 %8, %4
  %11 = icmp sgt i64 %9, 15
  %12 = zext i1 %11 to i64
  %13 = shl nuw nsw i64 %12, 2
  %14 = lshr i64 %9, %13
  %15 = or i64 %10, %13
  %16 = icmp sgt i64 %14, 3
  %17 = zext i1 %16 to i64
  %18 = shl nuw nsw i64 %17, 1
  %19 = lshr i64 %14, %18
  %20 = or i64 %15, %18
  %21 = icmp sgt i64 %19, 1
  %22 = zext i1 %21 to i64
  %23 = or i64 %20, %22
  %24 = bitcast i64* %"@x_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %24)
  store i64 %23, i64* %"@x_key", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %lookup_elem = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i64 %pseudo, i64* nonnull %"@x_key")
  %map_lookup_cond = icmp eq i8* %lookup_elem, null
  br i1 %map_lookup_cond, label %lookup_merge, label %lookup_success

lookup_success:                                   ; preds = %entry
  %25 = load i64, i8* %lookup_elem, align 8
  %phitmp = add i64 %25, 1
  br label %lookup_merge

lookup_merge:                                     ; preds = %entry, %lookup_success
  %lookup_elem_val.0 = phi i64 [ %phitmp, %lookup_success ], [ 1, %entry ]
  %26 = bitcast i64* %"@x_val" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %26)
  store i64 %lookup_elem_val.0, i64* %"@x_val", align 8
  %pseudo1 = call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo1, i64* nonnull %"@x_key", i64* nonnull %"@x_val", i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %24)
  call void @llvm.lifetime.end(i64 -1, i8* %26)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, builtin_count)
{
  test("kprobe:f { @x = count() }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_val" = alloca i64, align 8
  %"@x_key" = alloca i64, align 8
  %1 = bitcast i64* %"@x_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %1)
  store i64 0, i64* %"@x_key", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %lookup_elem = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i64 %pseudo, i64* nonnull %"@x_key")
  %map_lookup_cond = icmp eq i8* %lookup_elem, null
  br i1 %map_lookup_cond, label %lookup_merge, label %lookup_success

lookup_success:                                   ; preds = %entry
  %2 = load i64, i8* %lookup_elem, align 8
  %phitmp = add i64 %2, 1
  br label %lookup_merge

lookup_merge:                                     ; preds = %entry, %lookup_success
  %lookup_elem_val.0 = phi i64 [ %phitmp, %lookup_success ], [ 1, %entry ]
  %3 = bitcast i64* %"@x_val" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %3)
  store i64 %lookup_elem_val.0, i64* %"@x_val", align 8
  %pseudo1 = call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo1, i64* nonnull %"@x_key", i64* nonnull %"@x_val", i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %1)
  call void @llvm.lifetime.end(i64 -1, i8* %3)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, builtin_str)
{
  test("kprobe:f { @x = str(arg0) }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i32, i1) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8*) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_key" = alloca i64, align 8
  %arg0 = alloca i64, align 8
  %str = alloca [64 x i8], align 1
  %1 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 0
  call void @llvm.lifetime.start(i64 -1, i8* %1)
  %memset = call void @llvm.memset.p0i8.i64([64 x i8]* nonnull %str, i8 0, i64 64, i32 1, i1 false)
  %2 = bitcast i64* %arg0 to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %2)
  %3 = getelementptr i8, i8* %0, i64 112
  %probe_read = call i64 inttoptr (i64 4 to i64 (i8*, i64, i8*)*)(i64* nonnull %arg0, i64 8, i8* %3)
  %4 = load i64, i64* %arg0, align 8
  call void @llvm.lifetime.end(i64 -1, i8* %2)
  %probe_read_str = call i64 inttoptr (i64 45 to i64 (i8*, i64, i8*)*)([64 x i8]* nonnull %str, i64 64, i64 %4)
  %5 = bitcast i64* %"@x_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %5)
  store i64 0, i64* %"@x_key", align 8
  %pseudo = call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo, i64* nonnull %"@x_key", [64 x i8]* nonnull %str, i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %5)
  call void @llvm.lifetime.end(i64 -1, i8* %1)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, builtin_delete)
{
  test("kprobe:f { @x = delete() }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_key" = alloca i64, align 8
  %1 = bitcast i64* %"@x_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %1)
  store i64 0, i64* %"@x_key", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 -1)
  %delete_elem = call i64 inttoptr (i64 3 to i64 (i8*, i8*)*)(i64 %pseudo, i64* nonnull %"@x_key")
  call void @llvm.lifetime.end(i64 -1, i8* %1)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, int_propagation)
{
  test("kprobe:f { @x = 1234; @y = @x }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@y_val" = alloca i64, align 8
  %"@y_key" = alloca i64, align 8
  %"@x_key1" = alloca i64, align 8
  %"@x_val" = alloca i64, align 8
  %"@x_key" = alloca i64, align 8
  %1 = bitcast i64* %"@x_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %1)
  store i64 0, i64* %"@x_key", align 8
  %2 = bitcast i64* %"@x_val" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %2)
  store i64 1234, i64* %"@x_val", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo, i64* nonnull %"@x_key", i64* nonnull %"@x_val", i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %1)
  call void @llvm.lifetime.end(i64 -1, i8* %2)
  %3 = bitcast i64* %"@x_key1" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %3)
  store i64 0, i64* %"@x_key1", align 8
  %pseudo2 = call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %lookup_elem = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i64 %pseudo2, i64* nonnull %"@x_key1")
  %map_lookup_cond = icmp eq i8* %lookup_elem, null
  br i1 %map_lookup_cond, label %lookup_merge, label %lookup_success

lookup_success:                                   ; preds = %entry
  %4 = load i64, i8* %lookup_elem, align 8
  br label %lookup_merge

lookup_merge:                                     ; preds = %entry, %lookup_success
  %lookup_elem_val.0 = phi i64 [ %4, %lookup_success ], [ 0, %entry ]
  call void @llvm.lifetime.end(i64 -1, i8* %3)
  %5 = bitcast i64* %"@y_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %5)
  store i64 0, i64* %"@y_key", align 8
  %6 = bitcast i64* %"@y_val" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %6)
  store i64 %lookup_elem_val.0, i64* %"@y_val", align 8
  %pseudo3 = call i64 @llvm.bpf.pseudo(i64 1, i64 4)
  %update_elem4 = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo3, i64* nonnull %"@y_key", i64* nonnull %"@y_val", i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %5)
  call void @llvm.lifetime.end(i64 -1, i8* %6)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, string_propagation)
{
  test("kprobe:f { @x = \"asdf\"; @y = @x }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture writeonly, i8* nocapture readonly, i64, i32, i1) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i32, i1) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@y_key" = alloca i64, align 8
  %lookup_elem_val = alloca [64 x i8], align 1
  %"@x_key1" = alloca i64, align 8
  %"@x_key" = alloca i64, align 8
  %str = alloca [64 x i8], align 1
  %1 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 0
  call void @llvm.lifetime.start(i64 -1, i8* %1)
  store i8 97, i8* %1, align 1
  %str.repack5 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 1
  store i8 115, i8* %str.repack5, align 1
  %str.repack6 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 2
  store i8 100, i8* %str.repack6, align 1
  %str.repack7 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 3
  store i8 102, i8* %str.repack7, align 1
  %str.repack8 = getelementptr inbounds [64 x i8], [64 x i8]* %str, i64 0, i64 4
  %2 = bitcast i64* %"@x_key" to i8*
  call void @llvm.memset.p0i8.i64(i8* %str.repack8, i8 0, i64 60, i32 1, i1 false)
  call void @llvm.lifetime.start(i64 -1, i8* %2)
  store i64 0, i64* %"@x_key", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo, i64* nonnull %"@x_key", [64 x i8]* nonnull %str, i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %2)
  call void @llvm.lifetime.end(i64 -1, i8* %1)
  %3 = bitcast i64* %"@x_key1" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %3)
  store i64 0, i64* %"@x_key1", align 8
  %pseudo2 = call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %lookup_elem = call i8* inttoptr (i64 1 to i8* (i8*, i8*)*)(i64 %pseudo2, i64* nonnull %"@x_key1")
  %4 = getelementptr inbounds [64 x i8], [64 x i8]* %lookup_elem_val, i64 0, i64 0
  call void @llvm.lifetime.start(i64 -1, i8* %4)
  %map_lookup_cond = icmp eq i8* %lookup_elem, null
  br i1 %map_lookup_cond, label %lookup_failure, label %lookup_success

lookup_success:                                   ; preds = %entry
  %memcpy = call void @llvm.memcpy.p0i8.p0i8.i64([64 x i8]* nonnull %lookup_elem_val, i8* nonnull %lookup_elem, i64 64, i32 1, i1 false)
  br label %lookup_merge

lookup_failure:                                   ; preds = %entry
  %memset = call void @llvm.memset.p0i8.i64([64 x i8]* nonnull %lookup_elem_val, i8 0, i64 64, i32 1, i1 false)
  br label %lookup_merge

lookup_merge:                                     ; preds = %lookup_failure, %lookup_success
  call void @llvm.lifetime.end(i64 -1, i8* %3)
  %5 = bitcast i64* %"@y_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %5)
  store i64 0, i64* %"@y_key", align 8
  %pseudo3 = call i64 @llvm.bpf.pseudo(i64 1, i64 4)
  %update_elem4 = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo3, i64* nonnull %"@y_key", [64 x i8]* nonnull %lookup_elem_val, i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %5)
  call void @llvm.lifetime.end(i64 -1, i8* nonnull %4)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

TEST(codegen, pred_binop)
{
  test("kprobe:f / pid == 1234 / { @x = 1 }",

R"EXPECTED(; Function Attrs: nounwind
declare i64 @llvm.bpf.pseudo(i64, i64) #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #1

define i64 @"kprobe:f"(i8* nocapture readnone) local_unnamed_addr section "kprobe:f" {
entry:
  %"@x_val" = alloca i64, align 8
  %"@x_key" = alloca i64, align 8
  %get_pid_tgid = tail call i64 inttoptr (i64 14 to i64 ()*)()
  %.mask = and i64 %get_pid_tgid, -4294967296
  %1 = icmp eq i64 %.mask, 5299989643264
  br i1 %1, label %pred_true, label %pred_false

pred_false:                                       ; preds = %entry
  ret i64 0

pred_true:                                        ; preds = %entry
  %2 = bitcast i64* %"@x_key" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %2)
  store i64 0, i64* %"@x_key", align 8
  %3 = bitcast i64* %"@x_val" to i8*
  call void @llvm.lifetime.start(i64 -1, i8* %3)
  store i64 1, i64* %"@x_val", align 8
  %pseudo = tail call i64 @llvm.bpf.pseudo(i64 1, i64 3)
  %update_elem = call i64 inttoptr (i64 2 to i64 (i8*, i8*, i8*, i64)*)(i64 %pseudo, i64* nonnull %"@x_key", i64* nonnull %"@x_val", i64 0)
  call void @llvm.lifetime.end(i64 -1, i8* %2)
  call void @llvm.lifetime.end(i64 -1, i8* %3)
  ret i64 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #1

attributes #0 = { nounwind }
attributes #1 = { argmemonly nounwind }
)EXPECTED");
}

} // namespace codegen
} // namespace test
} // namespace bpftrace
