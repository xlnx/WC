; ModuleID = 'LRparser'

define i32 @test() {
alloc:
  %0 = alloca [10 x i32]
  %1 = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  %2 = getelementptr inbounds [10 x i32]* %0, i32 0, i32 0
  store i32 1, i32* %2
  %3 = getelementptr inbounds [10 x i32]* %0, i32 0, i32 0
  %4 = getelementptr inbounds i32* %3, i32 1
  store i32 1, i32* %4
  store i32 0, i32* %1

while_cond:                                       ; preds = %while_body
  %5 = load i32* %1
  %6 = icmp slt i32 %5, 10
  br i1 %6, label %while_body, label %endwhile

while_body:                                       ; preds = %while_cond
  %7 = load i32* %1
  %8 = getelementptr inbounds [10 x i32]* %0, i32 0, i32 %7
  %9 = load i32* %8
  %10 = load i32* %1
  %11 = sub i32 %10, 1
  %12 = getelementptr inbounds [10 x i32]* %0, i32 0, i32 %11
  %13 = load i32* %12
  %14 = load i32* %1
  %15 = sub i32 %14, 2
  %16 = getelementptr inbounds [10 x i32]* %0, i32 0, i32 %15
  %17 = load i32* %16
  %18 = add i32 %13, %17
  %19 = add i32 %9, %18
  store i32 %19, i32* %8
  br label %while_cond

endwhile:                                         ; preds = %while_cond
  %20 = getelementptr inbounds [10 x i32]* %0, i32 0, i32 9
  %21 = load i32* %20
  ret i32 %21
}

define i32 @main() {
alloc:
  %0 = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  %1 = call i32 @test()
  store i32 %1, i32* %0
  ret i32 0
}
