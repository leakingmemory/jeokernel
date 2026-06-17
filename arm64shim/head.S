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
 * Milestone 1 just prints "hello world" over the PL011 UART, so all we do here
 * is set up a stack and call into C++. The DTB pointer in x0 is preserved for
 * later milestones (it is the arm64 equivalent of the x86 ACPI/MPFP tables).
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
	/* x0 = DTB physical address from the loader; keep it for later. */
	adrp	x1, __stack_top
	add	x1, x1, :lo12:__stack_top
	mov	sp, x1

	bl	shim_main

	/* shim_main should not return in milestone 1; park the core. */
hang:
	wfe
	b	hang
