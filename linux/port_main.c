/* port_main.c -- Linux entry for the ported game.
 *
 * bblit_port_init() maps the original PE image (data, rdata, imports' thunk
 * tables) at its fixed base 0x400000, plants jump trampolines at the original
 * .text function addresses so the image's pointer slots and vtables reach the
 * ported 64-bit functions, and widens the 4-byte pointer slots to 8-byte
 * pointers.  After that, winmain is the game's WinMain.
 */
#include "data_syms.h"
#include "bblit_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

/* Crash backtrace: this sandbox forbids ptrace and core dumps, so a native
 * backtrace on fatal signals is the only way to localize a crash.
 * Addresses resolve via: addr2line -e linux/build/bblit <addr>
 * NOTE: under AddressSanitizer our handler runs instead of ASan's own
 * report (ASan respects an installed SIGSEGV handler unless
 * handle_segv=1 steals it back).  For ASan diagnostics, unset this
 * handler: ASAN_OPTIONS=handle_segv=1 runs ASan's reporter. */
/* Async-signal-safe fault address: backtrace() mallocs and dies on a
 * corrupted heap (observed: silent SIGSEGV with no report), so print
 * sig/pc/addr with write() first, then attempt the backtrace. */
#include <ucontext.h>
#include <string.h>
static void hex16(char *b, unsigned long v)
{
    for (int i = 15; i >= 0; i--) {
        b[i] = "0123456789abcdef"[v & 15];
        v >>= 4;
    }
}
static void bblit_crash_sa(int sig, siginfo_t *si, void *uc)
{
    char msg[80];
#if defined(__x86_64__)
    ucontext_t *c = (ucontext_t *)uc;
    unsigned long pc = (unsigned long)c->uc_mcontext.gregs[16];
#else
    unsigned long pc = 0;
#endif
    unsigned long addr = (unsigned long)si->si_addr;
    void *bt[64];
    int n;
    char *p = msg;
    for (int i = 0; i < 6; i++) *p++ = "bblit: "[i];
    (void)si; (void)uc;
    hex16(p, (unsigned long)sig); p += 16;
    *p++ = ' ';
    hex16(p, pc); p += 16;
    *p++ = ' ';
    hex16(p, addr); p += 16;
    *p++ = '\n';
    (void)!write(STDERR_FILENO, msg, (size_t)(p - msg));
    n = backtrace(bt, 64);
    backtrace_symbols_fd(bt, n, STDERR_FILENO);
    _exit(139);
}
static void bblit_crash(int sig)
{
    char msg[64];
    int len = snprintf(msg, sizeof msg, "bblit: fatal signal %d\n", sig);
    (void)!write(STDERR_FILENO, msg, (size_t)len);
    {
        void *bt[64];
        int n = backtrace(bt, 64);
        backtrace_symbols_fd(bt, n, STDERR_FILENO);
    }
    _exit(139);
}

int main(int argc, char **argv)
{
    int crt_ok;
    /* Unbuffered stderr: crash diagnostics must not sit in a lost buffer. */
    setbuf(stderr, 0);
    (void)argc;
    (void)argv;
    /* No crash handler under ASan: its own reporter needs the signal
     * (our handler would pre-empt it).  Plain builds keep the native
     * backtrace, the only debugger available in this sandbox. */
#ifndef __SANITIZE_ADDRESS__
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = bblit_crash_sa;
        sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
        sigaction(SIGSEGV, &sa, 0);
        sigaction(SIGBUS, &sa, 0);
        sigaction(SIGILL, &sa, 0);
    }
    (void)bblit_crash;
    signal(SIGABRT, bblit_crash);
    signal(SIGFPE, bblit_crash);
    signal(SIGILL, bblit_crash);
    signal(SIGBUS, bblit_crash);
#endif
    bblit_port_init();
    cb1a_init();
    /* VC++6 CRT startup prefix, straight from the original entry (see the
     * GetVersion block in bugs_decompiled.c): version word, heap and
     * handle-table init, then the stdio/locale setup WinMain assumes.
     * The generator drops these as CRT, so without them the first
     * formatted print faults on the never-initialised handle table
     * (observed: SIGSEGV si_addr=0x4 in FUN_00453d90 via
     * sysinfo_dump -> FUN_00451a50 -> game_vfprintf_core). */
    crt_ok = (int)FUN_004520a0();
    if (!crt_ok) __amsg_exit(0x1c);
    FUN_004530b0();
    FUN_00457600();
    { const char *e = getenv("BBLIT_DEBUG"); if (e) fprintf(stderr, "[init] 0904=%#x 0900=%#x 1e70=%#x\n", (unsigned)DAT_004b0904, (unsigned)*(data_u32 *)0x4b0900ul, (unsigned)DAT_004b1e70); }
    /* MSVC _ioinit equivalent for fds 0/1/2: one 32-record chunk of
     * {osfhnd, osfile} records; free records read back -1.  Without it the
     * first stdio flush reads _osfhnd[1] as INVALID_HANDLE_VALUE and dies
     * in fwrite (observed: sysinfo flush, WriteFile h=0xffffffff).  Small
     * handles route to stdio in the ReadFile/WriteFile shims.  An existing
     * chunk (opened by the config phase) is kept, only 0/1/2 are seeded. */
    {
        static uintptr_t stdio_chunk[32 * 2];
        static int stdio_chunk_done = 0;
        data_u32 *chunk0 = (&DAT_009ca9e0)[0];
        if (!chunk0) {
            for (int i = 0; i < 32; i++) {
                stdio_chunk[2 * i] = (uintptr_t)(int)-1;
                stdio_chunk[2 * i + 1] = 0;
            }
            stdio_chunk_done = 1;
            chunk0 = (data_u32 *)stdio_chunk;
            (&DAT_009ca9e0)[0] = chunk0;
        }
        (void)stdio_chunk_done;
        for (int fd = 0; fd < 3; fd++) {
            ((uintptr_t *)chunk0)[2 * fd] = (uintptr_t)fd;
            ((unsigned char *)chunk0)[fd * 8 + 4] = 1;
        }
    }
    /* Print buffers for the two static FILE structs (&DAT_004ae668/688).
     * FUN_00453f00 would HeapAlloc them, but a glibc heap address does not
     * fit the struct's 32-bit cursor/buffer slots (observed: cursor
     * truncated to 0x97274020, SEGV in FUN_00453d90).  .bss here is below
     * 4 GiB (like vf_frame), so static buffers keep every downstream
     * 4-byte int flow exact with no layout churn.  Pre-seeding the
     * (&DAT_004b1e90) slots also skips the sub-allocator entirely. */
    {
        static unsigned char print_bufs[2][0x1000];
        (&DAT_004b1e90)[0] = (data_u32)(uintptr_t)&print_bufs[0];
        (&DAT_004b1e90)[1] = (data_u32)(uintptr_t)&print_bufs[1];
    }
    /* WinMain(hInst=NULL-ish, hPrev=NULL, lpCmdLine=NULL, nShowCmd=SW_SHOW) */
    return (int)winmain(0, 0, 0, 1);
}
