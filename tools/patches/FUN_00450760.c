/* FUN_00450760 @ 00450760 -- hand-port maintained in tools/patches, spliced into
 * linux/bblit_game.c by tools/fix_loop.py.  The Ghidra pseudocode is wrong in
 * four places, verified against objdump disassembly of .text @00450760:
 *   1. `cVar1 = (char)lpBuffer` truncates the POINTER and
 *      `(char)((uint)lpBuffer >> 8)` reads the pointer's second byte; the
 *      assembly (`mov eax,[esp+4]; cmp al,0x5c/0x2f; cmp al,ah`) compares
 *      buffer[0] with buffer[1]: a UNC `\\` / `//` prefix check.
 *   2. `SetEnvironmentVariableA(lpBuffer,"BBLIT")` has the args backwards
 *      and the name wrong: the assembly builds "=X:" at esp+0 from '=',
 *      the uppercased first char (via FUN_00454480), ':' and NUL, then
 *      calls SetEnvironmentVariableA("=X:", buffer) -- the DOS per-drive
 *      cwd variable.  setenv rejects '=' in names on Linux, so this takes
 *      the fail path there; the caller ignores the return either way.
 *   3. the tail stores (0x4507e6/0x4507ec) are x86 frame-slot markers; on
 *      LP64 the store at puVar4-4 underflows the gcc local (ASan WRITE
 *      size 4, FUN_00450760:32491).
 *   4. the winmain call site drops the argument; the assembly shows
 *      `lea eax,[ebp-0x148]; push eax` before `call 00450760` at 0x405b49,
 *      i.e. FUN_00450760(local_14c) -- fixed at the call site.
 * Frame per assembly: sub esp,0x10c with the 268-byte buffer at esp+4.
 */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 FUN_00450760(LPCSTR param_1) {
  CHAR local_10c[268];
  CHAR eqdrive[4];
  CHAR first;
  BOOL BVar2;
  DWORD DVar3;
  BVar2 = SetCurrentDirectoryA(param_1);
  if (BVar2 == 0) goto fail;
  DVar3 = GetCurrentDirectoryA(0x105,local_10c);
  if (DVar3 == 0) goto fail;
  if (((local_10c[0] == '\\') || (local_10c[0] == '/')) && (local_10c[0] == local_10c[1])) {
    return 0;
  }
  /* "=X:" name; FUN_00454480 is a locale-aware toupper, ASCII-equal here */
  first = local_10c[0];
  eqdrive[0] = '=';
  eqdrive[1] = (CHAR)((first >= 'a' && first <= 'z') ? first - ('a' - 'A') : first);
  eqdrive[2] = ':';
  eqdrive[3] = '\0';
  BVar2 = SetEnvironmentVariableA(eqdrive,local_10c);
  if (BVar2 == 0) goto fail;
  return 0;
fail:
  DVar3 = GetLastError();
  FUN_00452de0(DVar3);
  return 0xffffffff;
}
