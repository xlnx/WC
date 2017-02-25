; ModuleID = 'LRparser'

%A = type { i32, double, i32 }

; Function Attrs: noinline nounwind
define i32 @add() #0 {
alloc:
  %retval = alloca i32
  %a = alloca i32
  %b = alloca double
  %add = alloca i32 (i32, i32)*
  br label %entry

entry:                                            ; preds = %alloc
  store i32 0, i32* %a
  store double 1.500000e+00, double* %b
  store i32 (i32, i32)* @.lambda, i32 (i32, i32)** %add
  %Load = load i32* %a
  %Load1 = load double* %b
  %Load2 = load i32 (i32, i32)** %add
  %0 = fptosi double %Load1 to i32
  %Call = call i32 %Load2(i32 %Load, i32 %0)
  store i32 %Call, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define i32 @.lambda(i32 %x, i32 %y) #0 {
alloc:
  %retval = alloca i32
  %x1 = alloca i32
  %y2 = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  store i32 %x, i32* %x1
  store i32 %y, i32* %y2
  %Load = load i32* %x1
  %Load3 = load i32* %y2
  %Add = add i32 %Load, %Load3
  store i32 %Add, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define void @test() #0 {
alloc:
  %a = alloca [2 x [3 x i32]]
  %c = alloca %A
  br label %entry

entry:                                            ; preds = %alloc
  store [2 x [3 x i32]] [[3 x i32] [i32 1, i32 2, i32 3], [3 x i32] [i32 1, i32 3, i32 4]], [2 x [3 x i32]]* %a
  store %A { i32 1, double 2.000000e+00, i32 5 }, %A* %c
  %PElem = getelementptr inbounds [2 x [3 x i32]]* %a, i32 0, i32 0
  %PElem1 = getelementptr inbounds [3 x i32]* %PElem, i32 0, i32 0
  %Load = load i32* %PElem1
  %Add = add i32 %Load, 100
  store i32 %Add, i32* %PElem1
  br label %return

return:                                           ; preds = %entry
  ret void
}

attributes #0 = { noinline nounwind }
