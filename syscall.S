.intel_syntax noprefix  # use INTEL ASM syntax instead of the default GCC AT&T
.text
    .globl sc_syscall # _start

    # in case -nostdlib used but then c include doesn't work
    # otherwise stdlib has _start
    # _start:             # GCC entry point
    #     xor ebp, ebp    # xoring a value with itself = 0 
    #     pop edi         # edi = argc 
    #                     # the pop instruction already added 8 to esp 
    #     mov esi, esp    # rest of the stack as an array of char ptr 
    #     and esp, -16
    #     call main       # call main function
    #
    #     # man 2 _exit
    #     mov ebx, eax     # syscall param 1 = rax (ret value of main) 
    #     mov eax, 1       # SYS_exit
    #     int 0x80
    #     ret

    sc_syscall:
        # Register mapping: https://en.wikibooks.org/wiki/X86_Assembly/Interfacing_with_Linux#Via_interrupt
        # 32-bit 0x80 => eax, ebx, ecx, edx, esi, edi
        # 64-bit syscall would be different!
        # figured out how C arguments are handled we get here using GDB (hint: they're all pushed onto stack)
        pop edi         # stack pos.1 = return address (in eax right now as well) 
                        # (must be saved to access the rest of the stack (in register => one param less possible like this though))
        
        pop eax         # param 1
        pop ebx         # param 2
        pop ecx         # param 3
        pop edx         # param 4
        pop esi         # param 5

        push edi        # restore return address for ret

        int 0x80        # SYSCALL 32-bit interrupt-based
        ret

