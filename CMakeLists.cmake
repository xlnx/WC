#project w compiler
PROJECT(w_compiler)
INCLUDE_DIRECTORIES(
	D:/MINGW64/mingw64/include
	D:/MINGW64/mingw64/x86_64-w64-mingw32/include
	D:/MINGW64/mingw64/lib/gcc/x86_64-w64-mingw32/6.3.0/include
	D:/MINGW64/mingw64/lib/gcc/x86_64-w64-mingw32/6.3.0/include/c++
	E:/LLVM/inlcude
)

AUX_SOURCE_DIRECTORY(src DIR_SRCS)

LINK_DIRECTORIES(
	D:/MINGW64/mingw64/lib
	D:/MINGW64/mingw64/x86_64-w64-mingw32/lib
	E:/LLVM/lib
)