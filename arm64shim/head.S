/*
 * First-stage arm64 boot shim entry point.
 *
 * The very first bytes of the flat image are the Linux arm64 "Image" header
 * (Documentation/arm64/booting.rst). QEMU's -kernel loader, U-Boot's booti and
 * the Pi4 firmware loader all recognise this header, load the flat image into
 * RAM with the MMU off, and jump to its start with:
 *
 *   x0 = physical address of the device tree blob (DTB)
 *   x1..x3 = 0 (reserved)
 *
 * Early multi-CPU gating:
 * An atomic fetch-and-add on cpu_count assigns a unique zero-based cpu_id
 * to each core. The winning core (cpu_id == 0) becomes the primary core, sets
 * up its stack, and executes shim_main. All other cores (cpu_id > 0) enter a
 * WFE wait loop until shim_main completes initialization, sets release_flag,
 * and issues SEV. The secondary cores then load their assigned stage stack and
 * ArmStageContext pointer from the arrays populated by shim_main and branch to
 * the stageloader entrypoint.
 */

	.section .text.boot, "ax"
	.globl _start
_start:
	b	real_start		/* code0: branch past the 64-byte header  */
	.long	0			/* code1                                   */
	.quad	0			/* text_offset (ignored when image_size=0) */
	.quad	0			/* image_size (0 => loader loads at RAM+2M)*/
	.quad	0			/* flags                                   */
	.quad	0			/* res2                                    */
	.quad	0			/* res3                                    */
	.quad	0			/* res4                                    */
	.ascii	"ARM\x64"		/* magic 0x644d5241 (offset 56)            */
	.long	0			/* res5: PE/COFF header offset (none)      */

real_start:
	/* Atomic fetch-and-add on cpu_count.
	 * Returns previous value in w1 (0 for primary, 1..N for secondaries).
	 * Preserves x0 (DTB address).
	 */
	adrp	x5, cpu_count
	add	x5, x5, :lo12:cpu_count
1:
	ldaxr	w1, [x5]
	add	w2, w1, #1
	stlxr	w3, w2, [x5]
	cbnz	w3, 1b

	/* If w1 != 0, this is a secondary core */
	cbnz	w1, secondary_cpu_wait

	/* Primary CPU (cpu_id == 0) */
	adrp	x1, __stack_top
	add	x1, x1, :lo12:__stack_top
	mov	sp, x1

	/* Call shim_main with x0 = DTB address */
	bl	shim_main

	/* shim_main should not return; park the core */
hang:
	wfe
	b	hang

secondary_cpu_wait:
	/* Save cpu_id (in callee-saved w19) */
	mov	w19, w1

	cmp	w19, #256
	b.hs	hang

	adrp	x20, release_flag
	add	x20, x20, :lo12:release_flag

2:
	ldar	w21, [x20]
	cbnz	w21, 3f
	wfe
	b	2b

3:
	/* Ensure all prior memory writes from primary CPU are visible */
	dsb	sy
	isb

	/* x0 = stage_contexts[cpu_id] */
	adrp	x22, stage_contexts
	add	x22, x22, :lo12:stage_contexts
	uxtw	x19, w19
	ldr	x0, [x22, x19, lsl #3]

	/* sp = stage_stacks[cpu_id] */
	adrp	x23, stage_stacks
	add	x23, x23, :lo12:stage_stacks
	ldr	x1, [x23, x19, lsl #3]
	mov	sp, x1

	/* x2 = stageloader_entry */
	adrp	x24, stageloader_entry
	add	x24, x24, :lo12:stageloader_entry
	ldr	x2, [x24]

	/* Jump to stageloader */
	br	x2

	.section .data
	.align 2
	.globl cpu_count
cpu_count:
	.word	0

	.align 2
	.globl release_flag
release_flag:
	.word	0

	.align 3
	.globl stageloader_entry
stageloader_entry:
	.quad	0

	.align 3
	.globl stage_contexts
stage_contexts:
	.space	8 * 256

	.align 3
	.globl stage_stacks
stage_stacks:
	.space	8 * 256
