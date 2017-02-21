; ModuleID = 'C:\Users\xlnx\Desktop\WC\exclang.c'
source_filename = "C:\5CUsers\5Cxlnx\5CDesktop\5CWC\5Cexclang.c"
target datalayout = "e-m:x-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "i686-pc-windows-msvc18.0.40629"

; Function Attrs: noinline nounwind
define i32 @add() #0 {
entry:
  %pf = alloca i32 ()*, align 4
  store i32 ()* @add, i32 ()** %pf, align 4
  %0 = load i32 ()*, i32 ()** %pf, align 4
  %call = call i32 %0()
  ret i32 %call
}

attributes #0 = { noinline nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="pentium4" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 4.0.0 (trunk)"}
