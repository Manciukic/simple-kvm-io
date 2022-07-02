.extern main
_guest_start:
	call main
	movq %cr0, %rdx
    smswl %eax
	hlt