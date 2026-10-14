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
 * backtrace on fatal signals is the only way to localize a crash.
 * Addresses resolve via: addr2line -e linux/build/bblit <addr>
 * NOTE: under AddressSanitizer our handler runs instead of ASan's own
 * report (ASan respects an installed SIGSEGV handler unless
 * handle_segv=1 steals it back).  For ASan diagnostics, unset this
 * handler: ASAN_OPTIONS=handle_segv=1 runs ASan's reporter. */
static void bblit_crash(int sig)
{
    /* With ASan, let the sanitizer report first (it interposes the
     * handler); the backtrace below is the fallback for plain builds.
     * ASan sets allow_user_segv_handler=0, so raise() through the default
     * disposition after printing. */
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
    /* No crash handler under ASan: its own reporter needs the signal
     * (our handler would pre-empt it).  Plain builds keep the native
     * backtrace, the only debugger available in this sandbox. */
#ifndef __SANITIZE_ADDRESS__
    signal(SIGSEGV, bblit_crash);
    signal(SIGABRT, bblit_crash);
    signal(SIGFPE, bblit_crash);
    signal(SIGILL, bblit_crash);
    signal(SIGBUS, bblit_crash);
#endif
    bblit_port_init();
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
    /* WinMain(hInst=NULL-ish, hPrev=NULL, lpCmdLine=NULL, nShowCmd=SW_SHOW) */
    return (int)winmain(0, 0, 0, 1);
}
