	.text
	.def	 main;
	.scl	2;
	.type	32;
	.endef
	.globl	main
	.align	16, 0x90
main:                                   # @main
.Ltmp0:
.seh_proc main
# BB#0:                                 # %entry
	pushq	%rbp
.Ltmp1:
	.seh_pushreg 5
	movq	%rsp, %rbp
	pushq	%rax
.Ltmp2:
	.seh_setframe 5, 0
.Ltmp3:
	.seh_endprologue
	callq	__main
	movl	$0, -4(%rbp)
	xorl	%eax, %eax
	addq	$8, %rsp
	popq	%rbp
	retq
.Leh_func_end0:
.Ltmp4:
	.seh_endproc


