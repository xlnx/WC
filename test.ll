; ModuleID = 'LRparser'

; Function Attrs: noinline nounwind
define i32 @fib(i32 %x) #0 {
alloc:
  %retval = alloca i32
  %x1 = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  store i32 %x, i32* %x1
  %Load = load i32* %x1
  %ICmpSLT = icmp slt i32 %Load, 3
  br i1 %ICmpSLT, label %then, label %endif

then:                                             ; preds = %entry
  store i32 1, i32* %retval
  br label %return
  br label %endif

endif:                                            ; preds = %then, %entry
  %Load2 = load i32* %x1
  %Add = add i32 %Load2, 1
  %Call = call i32 @fib(i32 %Add)
  %Load3 = load i32* %x1
  %Add4 = add i32 %Load3, 2
  %Call5 = call i32 @fib(i32 %Add4)
  %Add6 = add i32 %Call, %Call5
  store i32 %Add6, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %endif, %endif, %then
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define i32 @__main() #0 {
alloc:
  %retval = alloca i32
  %a = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  %Call = call i32 @fib(i32 100)
  store i32 %Call, i32* %a
  store i32 0, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

attributes #0 = { noinline nounwind }
