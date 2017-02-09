; ModuleID = 'LRparser'

@0 = global i32 10

define i32 @mul(i32 %a, i32 %b) {
entry:
  %0 = alloca i32
  store i32 %a, i32* %0
  %1 = alloca i32
  store i32 %b, i32* %1
  %2 = load i32* %0
  %3 = load i32* %1
  %4 = mul i32 %2, %3
  %5 = alloca i32
  store i32 %4, i32* %5
  ret i32 5
}

define i32 @main() {
entry:
  %0 = load i32* @0
  %1 = call i32 @mul(i32 %0, i32 3)
  %2 = load i32* @0
  %3 = load i32* @0
  %4 = call i32 @mul(i32 %2, i32 %3)
  ret i32 6
}
