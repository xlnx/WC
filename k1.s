	.text
	.def	 mul;
	.scl	2;
	.type	32;
	.endef
	.globl	mul
	.align	16, 0x90
mul:                                    # @mul
.Ltmp0:
.seh_proc mul
# BB#0:                                 # %entry
	subq	$16, %rsp
.Ltmp1:
	.seh_stackalloc 16
.Ltmp2:
	.seh_endprologue
	movl	%ecx, 12(%rsp)
	movl	%edx, 8(%rsp)
	imull	12(%rsp), %edx
	movl	%edx, 4(%rsp)
	movl	$5, %eax
	addq	$16, %rsp
	retq
.Leh_func_end0:
.Ltmp3:
	.seh_endproc

	.def	 main;
	.scl	2;
	.type	32;
	.endef
	.globl	main
	.align	16, 0x90
main:                                   # @main
.Ltmp4:
.seh_proc main
# BB#0:                                 # %entry
	pushq	%rbp
.Ltmp5:
	.seh_pushreg 5
	movq	%rsp, %rbp
	subq	$32, %rsp
.Ltmp6:
	.seh_setframe 5, 0
.Ltmp7:
	.seh_endprologue
	callq	__main
	movl	__unnamed_1(%rip), %ecx
	movl	$3, %edx
	callq	mul
	movl	__unnamed_1(%rip), %ecx
	movl	%ecx, %edx
	callq	mul
	movl	$6, %eax
	addq	$32, %rsp
	popq	%rbp
	retq
.Leh_func_end1:
.Ltmp8:
	.seh_endproc

	.data
	.globl	__unnamed_1             # @0
	.align	4
__unnamed_1:
	.long	10                      # 0xa


