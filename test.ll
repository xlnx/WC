; ModuleID = 'LRparser'

%A = type { i8388607*, i32 }
%B = type { %A, i8388607* }

@vtable = constant { i32 (%A*)* } { i32 (%A*)* @A.exec }
@vtable1 = constant { i32 (%B*)* } { i32 (%B*)* @B.exec }

; Function Attrs: noinline nounwind
define i32 @A.exec(%A*) #0 {
alloc:
  %retval = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  %PMember = getelementptr inbounds %A* %0, i64 0, i32 1
  %Load = load i32* %PMember
  %Add = add i32 %Load, 1
  store i32 %Add, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define void @A.init(%A*) #0 {
alloc:
  br label %entry

entry:                                            ; preds = %alloc
  %PMember = getelementptr inbounds %A* %0, i64 0, i32 1
  store i32 10, i32* %PMember
  br label %return

return:                                           ; preds = %entry
  ret void
}

; Function Attrs: noinline nounwind
define i32 @B.exec(%B*) #0 {
alloc:
  %retval = alloca i32
  br label %entry

entry:                                            ; preds = %alloc
  %PMember = getelementptr inbounds %B* %0, i64 0, i32 0
  %PMember1 = getelementptr inbounds %A* %PMember, i64 0, i32 1
  %Load = load i32* %PMember1
  %Sub = sub i32 %Load, 1
  store i32 %Sub, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define i32 @sub() #0 {
alloc:
  %retval = alloca i32
  %b = alloca %B
  %p = alloca %B*
  br label %entry

entry:                                            ; preds = %alloc
  %BitCast = bitcast { i32 (%B*)* }* @vtable1 to i8388607*
  %PMember = getelementptr inbounds %B* %b, i64 0, i32 1
  store i8388607* %BitCast, i8388607** %PMember
  %PMember1 = getelementptr inbounds %B* %b, i64 0, i32 0
  %BitCast2 = bitcast { i32 (%B*)* }* @vtable1 to i8388607*
  %PMember3 = getelementptr inbounds %A* %PMember1, i64 0, i32 0
  store i8388607* %BitCast2, i8388607** %PMember3
  store %B* %b, %B** %p
  %PMember4 = getelementptr inbounds %B* %b, i64 0, i32 0
  call void @A.init(%A* %PMember4)
  %Call = call i32 @B.exec(%B* %b)
  store i32 %Call, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

; Function Attrs: noinline nounwind
define i32 @add() #0 {
alloc:
  %retval = alloca i32
  %b = alloca %B
  %p = alloca %B*
  br label %entry

entry:                                            ; preds = %alloc
  %BitCast = bitcast { i32 (%B*)* }* @vtable1 to i8388607*
  %PMember = getelementptr inbounds %B* %b, i64 0, i32 1
  store i8388607* %BitCast, i8388607** %PMember
  %PMember1 = getelementptr inbounds %B* %b, i64 0, i32 0
  %BitCast2 = bitcast { i32 (%B*)* }* @vtable1 to i8388607*
  %PMember3 = getelementptr inbounds %A* %PMember1, i64 0, i32 0
  store i8388607* %BitCast2, i8388607** %PMember3
  store %B* %b, %B** %p
  %PMember4 = getelementptr inbounds %B* %b, i64 0, i32 0
  call void @A.init(%A* %PMember4)
  %Load = load %B** %p
  %PMember5 = getelementptr inbounds %B* %Load, i64 0, i32 1
  %Ptr = load i8388607** %PMember5
  %VMTCast = bitcast i8388607* %Ptr to { i32 (%B*)* }*
  %PMember6 = getelementptr inbounds { i32 (%B*)* }* %VMTCast, i64 0, i32 0
  %VMethod = load i32 (%B*)** %PMember6
  %Call = call i32 %VMethod(%B* %Load)
  store i32 %Call, i32* %retval
  br label %return
  br label %return

return:                                           ; preds = %entry, %entry
  %retval_load = load i32* %retval
  ret i32 %retval_load
}

attributes #0 = { noinline nounwind }
