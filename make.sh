#!/bin/bash
echo "####################################"
echo "####################################"
echo "####################################"
# # doesn't save debugging symbols even though it has been told to
# gcc -Wall -g -m32 -s -nostdlib syscall.S syscall.c -o syscall.out
#
# # separate compilation with symbols saved for gdb debugging
#
# # -nostdlib with custom _start in syscall.S
# gcc -Wall -g -c syscall.c -o syscall.c.o
# as -Wall -g syscall.S -o syscall.S.o 1>/dev/null
# # ld syscall.c.o syscall.S.o -o syscall.out # not recommended
# gcc -Wall -nostdlib syscall.c.o syscall.S.o -o syscall.out
#
# use system's _start and allow #include in c
# with debug symbols
gcc -Wall -g -c main.c -o obj/main_debug.c.o
gcc -Wall -g -c syscall.S -o obj/syscall_debug.S.o
gcc -Wall obj/main_debug.c.o obj/syscall_debug.S.o -o build/main_debug
# without (production build)
gcc -Wall -c main.c -o obj/main.c.o
gcc -Wall -c syscall.S -o obj/syscall.S.o
gcc -Wall obj/main.c.o obj/syscall.S.o -o build/main
#
# # launch gdb debugger
# gdb syscall.out
# layout split          # show C and ASM code alongside
# b c_line_number_here  # set a breakpoint
# run                   # start the process
echo "####################################"
echo "####################################"
echo "####################################"
