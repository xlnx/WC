	.text
	.def	 shit;
	.scl	2;
	.type	32;
	.endef
	.globl	shit
	.align	16, 0x90
shit:                                   # @shit
# BB#0:                                 # %alloc
	xorl	%eax, %eax
	retq

	.def	 main;
	.scl	2;
	.type	32;
	.endef
	.globl	main
	.align	16, 0x90
main:                                   # @main
# BB#0:                                 # %alloc
	pushq	%rbp
	movq	%rsp, %rbp
	callq	__main
	xorl	%eax, %eax
	popq	%rbp
	retq


