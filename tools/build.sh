#!/bin/sh
# build.sh -- build the bblit Linux binary.
#
# -no-pie is REQUIRED at the link: the generated game code references image
# data by absolute original-VA immediates (mov 0x52fd00, %ecx), which are only
# valid when the executable loads at the non-PIE base that keeps 0x400000-
# 0x9da000 inside the bblit_image_blob mapping.  Under the default PIE build
# those immediates are never relocated and the process segfaults at the first
# global access (observed: FUN_00449d80, bblit_game.c:28551).
#
# -Ttext=0x48000000 keeps the ported code OUT of 0x400000-0x9da000, the window
# bblit_port_init mmaps MAP_FIXED for the original EXE image.  At the default
# -no-pie base 0x400000 the mmap overwrites the port's own code pages and the
# process dies at the cmp right after mmap (observed: SIGSEGV RIP=si_addr=0x44b8a9
# inside bblit_port_init, RAX=0x400000 RDX=3).
LDFLAGS="-no-pie -Wl,-Ttext-segment=0x48000000"
set -e
cd "$(dirname "$0")/.."
O=linux/build
mkdir -p "$O"
CFLAGS="-O1 -g -fno-strict-aliasing -w -Ilinux -std=gnu17"
# -std=gnu17 is REQUIRED: ghidra_types.h defines `code` as an old-style
# unprototyped function pointer so vtable call sites compile with their own
# arity.  C23 (GCC 15+ default) gives `()` a zero-arg meaning and every
# indirect call becomes "too many arguments" (100 errors on GCC 16).
gcc $CFLAGS -c linux/bblit_game.c        -o "$O/bblit_game.o"
gcc $CFLAGS -c linux/data_syms.c         -o "$O/data_syms.o"
gcc $CFLAGS -c linux/port_main.c         -o "$O/port_main.o"
gcc $CFLAGS -c linux/shim/winapi_table.c -o "$O/winapi_table.o"
gcc $CFLAGS -c linux/shim/winapi_stubs.c -o "$O/winapi_stubs.o"
gcc $CFLAGS -c linux/shim/shim_crt.c     -o "$O/shim_crt.o"
gcc $CFLAGS -c linux/shim/port_helpers.c -o "$O/port_helpers.o"
gcc $LDFLAGS "$O"/bblit_game.o "$O"/data_syms.o "$O"/port_main.o \
    "$O"/winapi_table.o "$O"/winapi_stubs.o "$O"/shim_crt.o "$O"/port_helpers.o \
    -o "$O/bblit" -lm
echo "built $O/bblit"
