; ModuleID = 'LRparser'

; Function Attrs: noinline nounwind
define i32 @sum(i32 %a, i32 %b) #0 {
alloc:
  %retval = alloca i32
  %a1 = alloca i32
  %b2 = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  store i32 %a, i32* %a1
  store i32 %b, i32* %b2
  %Load = load i32* %a1
  %Load3 = load i32* %b2
  %Add = add i32 %Load, %Load3
  store i32 %Add, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define i32 @sum1(i32 %a, i32 %b, i32 %c) #0 {
alloc:
  %retval = alloca i32
  %a1 = alloca i32
  %b2 = alloca i32
  %c3 = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  store i32 %a, i32* %a1
  store i32 %b, i32* %b2
  store i32 %c, i32* %c3
  %Load = load i32* %a1
  %Load4 = load i32* %b2
  %Add = add i32 %Load, %Load4
  %Load5 = load i32* %c3
  %Add6 = add i32 %Add, %Load5
  store i32 %Add6, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define i32 @shit() #0 {
alloc:
  %retval = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  %Call = call i32 @sum1(i32 1, i32 2, i32 3)
  store i32 %Call, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

attributes #0 = { noinline nounwind }
