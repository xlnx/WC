; ModuleID = 'LRparser'

%A = type { i32, i32 }

; Function Attrs: noinline nounwind
define i32 @A__mid(%A*) #0 {
alloc:
  %retval = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  %PElem = getelementptr inbounds %A* %0, i64 0, i32 0
  %Load = load i32* %PElem
  %PElem1 = getelementptr inbounds %A* %0, i64 0, i32 1
  %Load2 = load i32* %PElem1
  %Add = add i32 %Load, %Load2
  %SDiv = sdiv i32 %Add, 2
  store i32 %SDiv, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define i32 @figure(i32 %x, i32 %y) #0 {
alloc:
  %retval = alloca i32
  %x1 = alloca i32
  %y2 = alloca i32
  %a_inst = alloca %A
  br label %entry

entry:                                            ; preds = %alloc
  store i32 %x, i32* %x1
  store i32 %y, i32* %y2
  %PElem = getelementptr inbounds %A* %a_inst, i64 0, i32 0
  %Load = load i32* %x1
  store i32 %Load, i32* %PElem
  %PElem3 = getelementptr inbounds %A* %a_inst, i64 0, i32 1
  %Load4 = load i32* %y2
  store i32 %Load4, i32* %PElem3
  %Call = call i32 @A__mid(%A* %a_inst)
  store i32 %Call, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

attributes #0 = { noinline nounwind }
