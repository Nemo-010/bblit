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
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

/* Crash backtrace: this sandbox forbids ptrace and core dumps, so a native
 * backtrace on SIGSEGV/SIGABRT is the only way to localize a crash.
 * Addresses resolve via: addr2line -e linux/build/bblit <addr> */
static void bblit_crash(int sig)
{
    void *bt[64];
    int n = backtrace(bt, 64);
    char msg[64];
    int len = snprintf(msg, sizeof msg, "bblit: fatal signal %d\n", sig);
    (void)!write(STDERR_FILENO, msg, (size_t)len);
    backtrace_symbols_fd(bt, n, STDERR_FILENO);
    _exit(139);
}

int main(int argc, char **argv)
{
    int crt_ok;
    (void)argc;
    (void)argv;
    signal(SIGSEGV, bblit_crash);
    signal(SIGABRT, bblit_crash);
    signal(SIGFPE, bblit_crash);
    signal(SIGILL, bblit_crash);
    signal(SIGBUS, bblit_crash);
    bblit_port_init();
    /* VC++6 CRT startup prefix, straight from the original entry (see
     * bugs_decompiled.c entry order): version gate, handle-table init,
     * stdio init, command-line capture, then heap setup and the rest of
     * what WinMain assumes.  The generator dropped these as CRT because
     * the decompile's bodies are uncompilable register puzzles, and
     * nothing called them -- so the first formatted print crashed
     * dereferencing the never-initialised handle table. */
    crt_ok = (int)FUN_004520a0();
    fprintf(stderr, "[crt] 520a0=%d heap=%p\n", crt_ok, (void *)(uintptr_t)DAT_009caae4);
    if (!crt_ok) __amsg_exit(0x1c);
    FUN_004530b0();
    FUN_00457600();
    /* WinMain(hInst=NULL-ish, hPrev=NULL, lpCmdLine=NULL, nShowCmd=SW_SHOW) */
    return (int)winmain(0, 0, 0, 1);
}
