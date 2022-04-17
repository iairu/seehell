.intel_syntax noprefix  # use INTEL ASM syntax instead of the default GCC AT&T
.text
    .globl sc_syscall

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

