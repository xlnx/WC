; ModuleID = 'LRparser'

%A = type { i32, double }

; Function Attrs: noinline nounwind
define void @A.exec(%A*) #0 {
alloc:
  br label %entry

entry:                                            ; preds = %alloc
  br label %return

return:                                           ; preds = %entry
  ret void
}

; Function Attrs: noinline nounwind
define i32 @sum(i32 %a, i32 %b) #0 {
alloc:
  %retval = alloca i32
  %a1 = alloca i32
  %b2 = alloca i32
  %result = alloca i32
  %a_inst = alloca %A
  %pa = alloca %A*
  br label %entry

entry:                                            ; preds = %alloc
  store i32 %a, i32* %a1
  store i32 %b, i32* %b2
  store i32 0, i32* %result
  %PElem = getelementptr inbounds %A* %a_inst, i64 0, i32 0
  store i32 1, i32* %PElem
  %Load = load %A** %pa
  %PElem3 = getelementptr inbounds %A* %Load, i64 0, i32 1
  store double 9.700000e+01, double* %PElem3
  br label %block

block:                                            ; preds = %entry
  br label %loop_next

loop_next:                                        ; preds = %endif, %block
  %Load4 = load i32* %a1
  %Load5 = load i32* %b2
  %ICmpSLE = icmp sle i32 %Load4, %Load5
  br i1 %ICmpSLE, label %while_body, label %loop_finally

while_body:                                       ; preds = %loop_next
  br label %block6

block6:                                           ; preds = %while_body
  %Load7 = load i32* %result
  %Load8 = load i32* %a1
  %Add = add i32 %Load7, %Load8
  store i32 %Add, i32* %result
  %Load9 = load i32* %a1
  %Add10 = add i32 %Load9, 1
  store i32 %Add10, i32* %a1
  %Load11 = load i32* %a1
  %ICmpEQ = icmp eq i32 %Load11, 10
  br i1 %ICmpEQ, label %then, label %endif

then:                                             ; preds = %block6
  br label %loop_end
  br label %endif

endif:                                            ; preds = %then, %block6
  br label %loop_next

loop_finally:                                     ; preds = %loop_next
  br label %block12

block12:                                          ; preds = %loop_finally
  %Load13 = load i32* %result
  %Or = or i32 %Load13, 1
  store i32 %Or, i32* %result
  br label %loop_end

loop_end:                                         ; preds = %block12, %then
  %Load14 = load i32* %result
  store i32 %Load14, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %loop_end, %loop_end
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
define i32 @swh(i32 %a, i32 %b) #0 {
alloc:
  %retval = alloca i32
  %a1 = alloca i32
  %b2 = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  store i32 %a, i32* %a1
  store i32 %b, i32* %b2
  br label %block

block:                                            ; preds = %entry
  %Load = load i32* %a1
  br label %switch_body
  br label %block3

block3:                                           ; preds = %switch_body, %switch_body, %block
  %Load4 = load i32* %a1
  %Add = add i32 %Load4, 1
  store i32 %Add, i32* %a1
  br label %block5

block5:                                           ; preds = %switch_body, %block3
  %Load6 = load i32* %b2
  %Add7 = add i32 %Load6, 1
  store i32 %Add7, i32* %b2
  br label %switch_end

switch_body:                                      ; preds = %block
  switch i32 %Load, label %switch_end [
    i32 0, label %block3
    i32 2, label %block3
    i32 1, label %block5
    i32 5, label %switch_end
  ]

switch_end:                                       ; preds = %switch_body, %switch_body, %block5
  %Load8 = load i32* %a1
  %Neg = sub i32 0, %Load8
  %Load9 = load i32* %b2
  %Or = or i32 %Neg, %Load9
  store i32 %Or, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %switch_end, %switch_end
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
  br i1 %ICmpSLT, label %then, label %else

then:                                             ; preds = %entry
  store i32 1, i32* %retval
  br label %return
  br label %endif

else:                                             ; preds = %entry
  %Load2 = load i32* %x1
  %Sub = sub i32 %Load2, 1
  %Call = call i32 @fib(i32 %Sub)
  %Load3 = load i32* %x1
  %Sub4 = sub i32 %Load3, 2
  %Call5 = call i32 @fib(i32 %Sub4)
  %Add = add i32 %Call, %Call5
  store i32 %Add, i32* %retval
  br label %return
  br label %endif

endif:                                            ; preds = %else, %then
  br label %return

return:                                           ; preds = %endif, %else, %then
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define i32 @calc(i32 (i32, i32)* %f, i32 %L, i32 %R) #0 {
alloc:
  %retval = alloca i32
  %f1 = alloca i32 (i32, i32)*
  %L2 = alloca i32
  %R3 = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  store i32 (i32, i32)* %f, i32 (i32, i32)** %f1
  store i32 %L, i32* %L2
  store i32 %R, i32* %R3
  br label %return

return:                                           ; preds = %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

attributes #0 = { noinline nounwind }
