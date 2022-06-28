_guest_start:
	// print hello world
	leaq hello_world(%rip), %rdi
	call print

	// check cr0
	movq %cr0, %rdx
	smswl %eax

	// bye bye
	hlt

// =============================================================================
// Strings
// =============================================================================

.align 8
hello_world: .string "Hello world!\n\0"

// =============================================================================
// Serial port
// =============================================================================

.set SERIAL_OUT, 0x10

// -----------------------------------------------------------------------------
// prints string pointed by %rdi to vmm console
print:
	pushq %rbx
	pushq %rcx

	movq $0, %rcx

_printdw:
	movq (%rdi,%rcx,8), %rax
	movq $8, %rbx

_printb:
	cmpb $0, %bl
	je _print_next_dw

	cmpb $0, %al
	je _print_exit

	outb %al, $SERIAL_OUT
	shrq $8, %rax
	decb %bl
	jmp _printb

_print_next_dw:
	incq %rcx
	jmp _printdw

_print_exit:
	popq %rcx
	popq %rbx
	ret
