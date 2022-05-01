#!/bin/bash
# alternativa k makefile
echo "####################################"
echo "####################################"
echo "####################################"
# with debug symbols
gcc -Wall -g -c main.c -o obj/main_debug.c.o
gcc -Wall -g -c syscall.S -o obj/syscall_debug.S.o
gcc -Wall obj/main_debug.c.o obj/syscall_debug.S.o -o build/main_debug
# without (production build)
gcc -Wall -c main.c -o obj/main.c.o
gcc -Wall -c syscall.S -o obj/syscall.S.o
gcc -Wall obj/main.c.o obj/syscall.S.o -o build/main
echo "####################################"
echo "####################################"
echo "####################################"
