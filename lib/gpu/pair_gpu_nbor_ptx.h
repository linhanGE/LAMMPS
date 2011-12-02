const char * pair_gpu_nbor_kernel = 
"	.version 2.3\n"
"	.target sm_20\n"
"	.address_size 64\n"
"	.entry kernel_unpack (\n"
"		.param .u64 __cudaparm_kernel_unpack_dev_nbor,\n"
"		.param .u64 __cudaparm_kernel_unpack_dev_ij,\n"
"		.param .s32 __cudaparm_kernel_unpack_inum)\n"
"	{\n"
"	.reg .u32 %r<11>;\n"
"	.reg .u64 %rd<27>;\n"
"	.reg .pred %p<5>;\n"
"	.loc	16	29	0\n"
"$LDWbegin_kernel_unpack:\n"
"	mov.u32 	%r1, %ctaid.x;\n"
"	mov.u32 	%r2, %ntid.x;\n"
"	mul.lo.u32 	%r3, %r1, %r2;\n"
"	mov.u32 	%r4, %tid.x;\n"
"	add.u32 	%r5, %r4, %r3;\n"
"	ld.param.s32 	%r6, [__cudaparm_kernel_unpack_inum];\n"
"	setp.le.s32 	%p1, %r6, %r5;\n"
"	@%p1 bra 	$Lt_0_2050;\n"
"	.loc	16	35	0\n"
"	cvt.s64.s32 	%rd1, %r6;\n"
"	ld.param.u64 	%rd2, [__cudaparm_kernel_unpack_dev_nbor];\n"
"	cvt.s64.s32 	%rd3, %r5;\n"
"	add.u64 	%rd4, %rd3, %rd1;\n"
"	mul.lo.u64 	%rd5, %rd4, 4;\n"
"	add.u64 	%rd6, %rd2, %rd5;\n"
"	ld.global.s32 	%r7, [%rd6+0];\n"
"	.loc	16	36	0\n"
"	mul.wide.s32 	%rd7, %r6, 4;\n"
"	add.u64 	%rd8, %rd6, %rd7;\n"
"	mov.s64 	%rd9, %rd8;\n"
"	.loc	16	37	0\n"
"	ld.param.u64 	%rd10, [__cudaparm_kernel_unpack_dev_ij];\n"
"	ld.global.s32 	%r8, [%rd8+0];\n"
"	cvt.s64.s32 	%rd11, %r8;\n"
"	mul.wide.s32 	%rd12, %r8, 4;\n"
"	add.u64 	%rd13, %rd10, %rd12;\n"
"	.loc	16	38	0\n"
"	cvt.s64.s32 	%rd14, %r7;\n"
"	mul.wide.s32 	%rd15, %r7, 4;\n"
"	add.u64 	%rd16, %rd15, %rd13;\n"
"	setp.le.u64 	%p2, %rd16, %rd13;\n"
"	@%p2 bra 	$Lt_0_2562;\n"
"	add.u64 	%rd17, %rd15, 3;\n"
"	shr.s64 	%rd18, %rd17, 63;\n"
"	mov.s64 	%rd19, 3;\n"
"	and.b64 	%rd20, %rd18, %rd19;\n"
"	add.s64 	%rd21, %rd20, %rd17;\n"
"	shr.s64 	%rd22, %rd21, 2;\n"
"	mov.s64 	%rd23, 1;\n"
"	max.s64 	%rd24, %rd22, %rd23;\n"
"	mov.s64 	%rd25, %rd24;\n"
"$Lt_0_3074:\n"
"	.loc	16	41	0\n"
"	ld.global.s32 	%r9, [%rd13+0];\n"
"	st.global.s32 	[%rd9+0], %r9;\n"
"	.loc	16	42	0\n"
"	add.u64 	%rd9, %rd7, %rd9;\n"
"	.loc	16	40	0\n"
"	add.u64 	%rd13, %rd13, 4;\n"
"	setp.gt.u64 	%p3, %rd16, %rd13;\n"
"	@%p3 bra 	$Lt_0_3074;\n"
"$Lt_0_2562:\n"
"$Lt_0_2050:\n"
"	.loc	16	45	0\n"
"	exit;\n"
"$LDWend_kernel_unpack:\n"
"	}\n"
;