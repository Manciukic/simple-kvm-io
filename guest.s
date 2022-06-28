_guest_start:
	movq %cr0, %rdx
	smswl %eax
	hlt
