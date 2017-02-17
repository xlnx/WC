; ModuleID = 'LRparser'

; Function Attrs: noinline nounwind
define i32 @sum(i32 %a, i32 %b) #0 {
alloc:
  %retval = alloca i32
  %a1 = alloca i32
  %b2 = alloca i32
  %result = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  store i32 %a, i32* %a1
  store i32 %b, i32* %b2
  store i32 0, i32* %result
  br label %block

loop_next:                                        ; preds = %while_body
  %Load = load i32* %a1
  %Load3 = load i32* %b2
  %ICmpSLE = icmp sle i32 %Load, %Load3
  br i1 %ICmpSLE, label %while_body, label %loop_end

while_body:                                       ; preds = %loop_next
  br label %block
  %Load4 = load i32* %result
  %Load5 = load i32* %a1
  %0 = add i32 %Load4, %Load5
  store volatile i32 %0, i32* %result
  %Load6 = load i32* %a1
  %1 = add i32 %Load6, 1
  store volatile i32 %1, i32* %a1
  br label %loop_next

loop_end:                                         ; preds = %loop_next
  %Load7 = load i32* %result
  store i32 %Load7, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %loop_end, %loop_end
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

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
  %Sub = sub i32 %Load2, 1
  %Call = call i32 @fib(i32 %Sub)
  %Load3 = load i32* %x1
  %Sub4 = sub i32 %Load3, 2
  %Call5 = call i32 @fib(i32 %Sub4)
  %Add = add i32 %Call, %Call5
  store i32 %Add, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %endif, %endif, %then
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

attributes #0 = { noinline nounwind }
