/* winmain @ 00405950 -- hand-port maintained in tools/patches, spliced into
 * linux/bblit_game.c by tools/fix_loop.py.  Reason for the hand
 * port is documented at the top of the body.
 */
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */

undefined4 winmain(HINSTANCE param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4) {
  char *winmain_frame; /* stable 4-byte frame base; see the note below */
  char cVar1;
  bool bVar2;
  undefined4 uVar3;
  HWND pHVar5;
  BOOL BVar6;
  LSTATUS LVar7;
  UINT UVar8;
  int iVar10;
  uint uVar11;
  uint uVar12;
  undefined1 *puVar14;
  undefined1 *puVar15;
  undefined1 *puVar16;
  undefined4 *puVar18;
  char *pcVar17;
  char *pcVar19;
  char *pcVar20;
  byte bVar21;
  CHAR local_250 [258];
  char acStack_14e [2];
  undefined1 local_14c [260];
  undefined1 local_48 [4];
  int local_44;
  undefined1 local_2c [4];
  int local_28;
  undefined4 local_24;
  DWORD local_10;
  DWORD local_c;
  HKEY local_8;
  static char winmain_slots[64];

  bVar21 = 0;
  /* The decompile addresses outgoing call arguments as offsets from the
   * frame base (stack0xfffffc3c).  On LP64 those offsets land outside any
   * object the compiler laid out, so the port gives them a private arena:
   * winmain_slots is 64 bytes and every slot access below stays within
   * [-0x30, 0).  The arena must outlive the function, hence static. */
  winmain_frame = winmain_slots + 0x30;
  LoadStringA(param_1,*(UINT *)(PTR_DAT_0045f344 + 8),local_250,0x104);
  LoadStringA((void *)(uintptr_t)(DAT_004b1ce8),*(UINT *)(PTR_DAT_0045f344 + 4),local_14c,0x104);
  pHVar5 = FindWindowA(STR_WINDOW_CLASS,local_14c);
  if (pHVar5 != (HWND)0x0) {
    BringWindowToTop(pHVar5);
    BVar6 = IsIconic(pHVar5);
    if (BVar6 == 0) {
      return 0;
    }
    ShowWindow(pHVar5,9);
    return 0;
  }
  DAT_007c95e4 = GetSystemMetrics(0);
  DAT_007c95e0 = GetSystemMetrics(1);
  read_bugs_ini();
  load_config();
  switch((uintptr_t)(_DAT_009ca828)) {
  case 0:
    PTR_DAT_0045f344 = &DAT_0045f2b8;
    break;
  case 1:
    PTR_DAT_0045f344 = &DAT_0045f2d0;
    break;
  case 2:
    PTR_DAT_0045f344 = &DAT_0045f2e8;
    break;
  case 3:
    PTR_DAT_0045f344 = &DAT_0045f300;
    break;
  case 4:
    PTR_DAT_0045f344 = &DAT_0045f318;
    break;
  case 5:
    PTR_DAT_0045f344 = &DAT_0045f330;
  }
  LVar7 = RegOpenKeyExA((HKEY)0x80000001,STR_REG_GAME_KEY,0,0x20019, &local_8);
  if (LVar7 != 0) {
    return 0;
  }
  local_14c[0] = DAT_004b1a34;
  puVar18 = (undefined4 *)(local_14c + 1);
  for (iVar10 = 0x40; iVar10 != 0; iVar10 = iVar10 + -1) {
    *puVar18 = 0;
    puVar18 = puVar18 + (long)(int)((uint)bVar21 * -2 + 1);
  }
  *(undefined2 *)puVar18 = 0;
  *(undefined1 *)((long)(uintptr_t)puVar18 + (long)(int)((uint)bVar21 * -4 + 2)) = 0;
  local_10 = 1;
  local_c = 0x104;
  LVar7 = RegQueryValueExA(local_8,STR_REG_INSTALL_PATH,(LPDWORD)0x0,&local_10,local_14c, &local_c);
  if (LVar7 != 0) {
    RegCloseKey(local_8);
    return 0;
  }
  uVar11 = 0xffffffff;
  pcVar17 = local_14c;
  do {
    if (uVar11 == 0) break;
    uVar11 = uVar11 - 1;
    cVar1 = *pcVar17;
    pcVar17 = pcVar17 + (long)(int)((uint)bVar21 * -2 + 1);
  } while (cVar1 != '\0');
  /* LP64: acStack_14e (2B at EBP-0x14e) packed exactly between local_250 and
   * local_14c, arrays index upward, so acStack_14e[i] is local_14c[i-2]; the
   * strlen loop above leaves ~uVar11 = len+1, i.e. this checks the last path
   * char for a trailing backslash.  The arrays are separate gcc locals here. */
  if (local_14c[(long)(int)(~uVar11) - 2] != '\\') {
    uVar11 = 0xffffffff;
    pcVar17 = &DAT_0045f370;
    do {
      pcVar19 = pcVar17;
      if (uVar11 == 0) break;
      uVar11 = uVar11 - 1;
      pcVar19 = pcVar17 + (long)(int)((uint)bVar21 * -2 + 1);
      cVar1 = *pcVar17;
      pcVar17 = pcVar19;
    } while (cVar1 != '\0');
    uVar11 = ~uVar11;
    iVar10 = -1;
    pcVar17 = local_14c;
    do {
      pcVar20 = pcVar17;
      if (iVar10 == 0) break;
      iVar10 = iVar10 + -1;
      pcVar20 = pcVar17 + (long)(int)((uint)bVar21 * -2 + 1);
      cVar1 = *pcVar17;
      pcVar17 = pcVar20;
    } while (cVar1 != '\0');
    pcVar17 = pcVar19 + (long)(int)(-uVar11);
    pcVar19 = pcVar20 + -1;
    for (uVar12 = uVar11 >> 2; uVar12 != 0; uVar12 = uVar12 - 1) {
      *(undefined4 *)pcVar19 = *(undefined4 *)pcVar17;
      pcVar17 = pcVar17 + (long)(int)((uint)bVar21 * -8 + 4);
      pcVar19 = pcVar19 + (long)(int)((uint)bVar21 * -8 + 4);
    }
    for (uVar11 = uVar11 & 3; uVar11 != 0; uVar11 = uVar11 - 1) {
      *pcVar19 = *pcVar17;
      pcVar17 = pcVar17 + (long)(int)((uint)bVar21 * -2 + 1);
      pcVar19 = pcVar19 + (long)(int)((uint)bVar21 * -2 + 1);
    }
  }
  uVar11 = 0xffffffff;
  pcVar17 = &DAT_0045f36c;
  do {
    pcVar19 = pcVar17;
    if (uVar11 == 0) break;
    uVar11 = uVar11 - 1;
    pcVar19 = pcVar17 + (long)(int)((uint)bVar21 * -2 + 1);
    cVar1 = *pcVar17;
    pcVar17 = pcVar19;
  } while (cVar1 != '\0');
  uVar11 = ~uVar11;
  iVar10 = -1;
  pcVar17 = local_14c;
  do {
    pcVar20 = pcVar17;
    if (iVar10 == 0) break;
    iVar10 = iVar10 + -1;
    pcVar20 = pcVar17 + (long)(int)((uint)bVar21 * -2 + 1);
    cVar1 = *pcVar17;
    pcVar17 = pcVar20;
  } while (cVar1 != '\0');
  pcVar17 = pcVar19 + (long)(int)(-uVar11);
  pcVar19 = pcVar20 + -1;
  for (uVar12 = uVar11 >> 2; uVar12 != 0; uVar12 = uVar12 - 1) {
    *(undefined4 *)pcVar19 = *(undefined4 *)pcVar17;
    pcVar17 = pcVar17 + (long)(int)((uint)bVar21 * -8 + 4);
    pcVar19 = pcVar19 + (long)(int)((uint)bVar21 * -8 + 4);
  }
  for (uVar11 = uVar11 & 3; uVar11 != 0; uVar11 = uVar11 - 1) {
    *pcVar19 = *pcVar17;
    pcVar17 = pcVar17 + (long)(int)((uint)bVar21 * -2 + 1);
    pcVar19 = pcVar19 + (long)(int)((uint)bVar21 * -2 + 1);
  }
  /* LP64/assembly: the decompile drops the argument; the binary pushes
   * local_14c (`lea eax,[ebp-0x148]; push eax` before `call 00450760`). */
  FUN_00450760(local_14c);
  RegCloseKey(local_8);
  bVar2 = false;
  UVar8 = SetErrorMode(1);
  /* Disc check, first pass.  Hand ported: see the header comment of
   * tools/patches/find_cd_drive.c for why the decompiled frame-slot form
   * cannot be used.  The 8-byte pointer store to slot -0x2c overwrote the
   * low half of the 4-byte needle stored at -0x28, so __strcmpi compared
   * the volume label against a stack address and never matched, which is
   * what kept the binary in the retry dialog until the frame corruption
   * jumped to RIP=0. */
  bVar2 = cd_scan_once() != 0;
  puVar15 = (void *)(char *)winmain_frame;
  puVar16 = (undefined1 *)winmain_frame;
  *(UINT *)(puVar16 + -4) = UVar8;
  *(undefined4 *)(puVar16 + -8) = 0x405c26;
  SetErrorMode(*(UINT *)(puVar16 + -4));
  if (!bVar2) {
    /* No BBLIT disc found.  The decompile shows the retry dialog followed by
     * a second, identical copy of the drive scan; both are hand ported for
     * the same frame-slot reason.  The shim's dialog cannot be answered
     * "retry", so it reports IDCANCEL and we exit as if the user pressed
     * Cancel rather than looping (measured: the unbounded loop ended in
     * SIGSEGV at RIP=0). */
    do {
      iVar10 = (int)(uintptr_t)error_message_box((long)(uintptr_t)local_250);
      if (iVar10 == 2) return 0;
      UVar8 = SetErrorMode(1);
      bVar2 = cd_scan_once() != 0;
    } while (!bVar2);
  }
  *(undefined4 *)(puVar15 + -4) = 0x405d24;
  parse_command_line();
  *(undefined4 *)(puVar15 + -4) = 0x405d29;
  sysinfo_dump();
  *(undefined4 *)(puVar15 + -4) = 0x405d2e;
  FUN_00409860();
  *(undefined4 *)(puVar15 + -4) = 0x405d33;
  FUN_00409990();
  *(undefined4 *)(puVar15 + -4) = param_4;
  *(HINSTANCE *)(puVar15 + -8) = param_1;
  DAT_004b1ce8 = (uintptr_t)(param_1);
  *(undefined4 *)(puVar15 + -0xc) = 0x405d45;
  pHVar5 = (HWND)ogl_init();
  if (pHVar5 == (HWND)0x0) {
    if (DAT_009ca838 != (HWND)0x0) {
      *(undefined4 *)(puVar15 + -4) = 5;
      *(HINSTANCE *)(puVar15 + -8) = (HINSTANCE)(uintptr_t)(DAT_004b1ce8);
      *(undefined4 *)(puVar15 + -0xc) = 0x405d69;
      DAT_009ca838 = (uintptr_t)(pHVar5);
  pHVar5 = (HWND)ogl_init();
    }
    if (pHVar5 == (HWND)0x0) {
      *(undefined4 *)(puVar15 + -4) = 5;
      DAT_009ca824 = (uint)(299 < DAT_004b1de4);
      *(HINSTANCE *)(puVar15 + -8) = (HINSTANCE)(uintptr_t)(DAT_004b1ce8);
      *(undefined4 *)(puVar15 + -0xc) = 0x405d92;
  pHVar5 = (HWND)ogl_init();
      if (pHVar5 == (HWND)0x0) goto LAB_004060fc;
    }
  }
  *(undefined4 *)(puVar15 + -4) = 0x405da7;
  DAT_004b1cec = (uintptr_t)(pHVar5);
  FUN_00406190();
LAB_00405db9:
  *(undefined4 *)(puVar15 + -4) = 1;
  *(undefined4 *)(puVar15 + -8) = 0;
  *(undefined4 *)(puVar15 + -0xc) = 0;
  *(undefined4 *)(puVar15 + -0x10) = 0;
  puVar16 = puVar15 + -0x14;
  *(undefined1 **)(puVar15 + -0x14) = local_2c;
  *(undefined4 *)(puVar15 + -0x18) = 0x405dc7;
  iVar10 = PeekMessageA(*(LPMSG *)(puVar15 + -0x14),*(HWND *)(puVar15 + -0x10), *(UINT *)(puVar15 + -0xc),*(UINT *)(puVar15 + -8),*(UINT *)(puVar15 + -4));
  puVar15 = puVar15 + -0x14;
  while (iVar10 != 0) {
    if (local_28 == 0x12) goto LAB_004060fc;
    *(undefined1 **)(puVar15 + -4) = local_2c;
    *(undefined4 *)(puVar15 + -8) = 0x405ddb;
    TranslateMessage(*(MSG **)(puVar15 + -4));
    *(undefined1 **)(puVar15 + -8) = local_2c;
    *(undefined4 *)(puVar15 + -0xc) = 0x405de1;
    DispatchMessageA(*(MSG **)(puVar15 + -8));
    *(undefined4 *)(puVar15 + -0xc) = 1;
    *(undefined4 *)(puVar15 + -0x10) = 0;
    *(undefined4 *)(puVar15 + -0x14) = 0;
    *(undefined4 *)(puVar15 + -0x18) = 0;
    puVar16 = puVar15 + -0x1c;
    *(undefined1 **)(puVar15 + -0x1c) = local_2c;
    *(undefined4 *)(puVar15 + -0x20) = 0x405def;
    iVar10 = PeekMessageA(*(LPMSG *)(puVar15 + -0x1c),*(HWND *)(puVar15 + -0x18), *(UINT *)(puVar15 + -0x14),*(UINT *)(puVar15 + -0x10), *(UINT *)(puVar15 + -0xc));
    puVar15 = puVar15 + -0x1c;
  }
  *(undefined4 **)(puVar16 + -4) = &DAT_007c9600;
  _DAT_004b18f0 = 0;
  DAT_004b18f8 = 0;
  DAT_004b1d00 = 0;
  DAT_004b1924 = 0;
  *(undefined4 *)(puVar16 + -8) = 0x405e14;
  QueryPerformanceCounter(*(LARGE_INTEGER **)(puVar16 + -4));
  DAT_007c95e8 = (data_u32 *)(uintptr_t)(DAT_007c9600);
  DAT_007c95ec = (data_u32 *)(uintptr_t)(DAT_007c9604);
  puVar15 = puVar16 + -4;
  while (puVar16 = puVar15, DAT_0045f2b0 == 1) {
    *(undefined4 *)(puVar15 + -4) = 0x405e3e;
    pHVar5 = GetForegroundWindow();
    if ((pHVar5 == DAT_004b1cec) || (DAT_004b2277 != '\0')) {
      *(undefined4 *)(puVar15 + -4) = 0x405e58;
      iVar10 = FUN_004065e0();
      if (iVar10 == 0) {
LAB_00405fea:
        DAT_0045f2b0 = 0;
        break;
      }
      if (DAT_004b1d00 == 0) {
        DAT_00623654 = 0;
        DAT_00623650 = 0;
        _DAT_004b2368 = 0;
        *(undefined4 **)(puVar15 + -4) = (undefined4 *)(&DAT_004b38c0);
        *(undefined4 *)(puVar15 + -8) = 0x405ea9;
        ((long (*)())FUN_0040ceb0)();
        *(undefined4 *)(puVar15 + -4) = (uintptr_t)(DAT_004b39ac);
        *(undefined4 *)(puVar15 + -8) = 0x405eb7;
        ((long (*)())FUN_0040a1f0)();
        DAT_004efb8c = (data_u32 *)(uintptr_t)(DAT_004e5e44);
        *(undefined4 *)(puVar15 + -4) = (uintptr_t)(DAT_004b2164);
        *(undefined4 *)(puVar15 + -8) = 0x405ed2;
        ((long (*)())FUN_0044b370)();
        *(undefined4 *)(puVar15 + -4) = (uintptr_t)(DAT_004b39ac);
        *(undefined4 *)(puVar15 + -8) = 0x405ee0;
        ((long (*)())FUN_0044c4b0)();
        *(undefined4 **)(puVar15 + -4) = &DAT_004b21a0;
        *(undefined4 *)(puVar15 + -8) = 0x405eed;
        ((long (*)())FUN_0044bb50)();
        *(undefined4 *)(puVar15 + -4) = (uintptr_t)(DAT_004b373c);
        *(undefined4 *)(puVar15 + -8) = 0x405efc;
        ((long (*)())FUN_0044cfa0)();
        DAT_004b18fc = DAT_004b18fc + 1;
      }
      else {
        _DAT_004b2368 = _DAT_004b2368 & 0xffff0000;
        *(undefined4 *)(puVar15 + -4) = (uintptr_t)(DAT_004b39ac);
        *(undefined4 *)(puVar15 + -8) = 0x405e7e;
        ((long (*)())FUN_0040a530)();
        DAT_004b18f8 = DAT_004b18f8 + 1;
      }
      _DAT_004b2368 = _DAT_004b2368 & 0xffff0000;
      DAT_004b39c0 = 0;
      if (DAT_004b39b0 < 1) {
        *(undefined4 *)(puVar15 + -4) = 0x405fa0;
        (*DAT_004b1a90)();
      }
      else {
        if (DAT_004b1d00 == 0) {
          if (DAT_004e5e40 < 1) {
            *(undefined4 *)(puVar15 + -4) = 0x405f5a;
            (*DAT_004b1a90)();
          }
          else {
            *(undefined4 *)(puVar15 + -4) = 0;
            *(undefined4 *)(puVar15 + -8) = 0;
            *(HWND *)(puVar15 + -0xc) = (HWND)(uintptr_t)(DAT_004b1cec);
            *(undefined4 *)(puVar15 + -0x10) = 0x405f40;
            InvalidateRect(*(HWND *)(puVar15 + -0xc),*(RECT **)(puVar15 + -8), *(BOOL *)(puVar15 + -4));
            *(HWND *)(puVar15 + -0x10) = (HWND)(uintptr_t)(DAT_004b1cec);
            *(undefined4 *)(puVar15 + -0x14) = 0x405f4c;
            UpdateWindow(*(HWND *)(puVar15 + -0x10));
            _DAT_004b18f4 = _DAT_004b18f4 + 1;
            puVar15 = puVar15 + -0x10;
          }
          *(undefined4 *)(puVar15 + -4) = 1;
          *(undefined4 *)(puVar15 + -8) = 0;
          *(undefined4 *)(puVar15 + -0xc) = 0;
          *(undefined4 *)(puVar15 + -0x10) = 0;
          *(undefined1 **)(puVar15 + -0x14) = local_48;
          *(undefined4 *)(puVar15 + -0x18) = 0x405f68;
          iVar10 = PeekMessageA(*(LPMSG *)(puVar15 + -0x14),*(HWND *)(puVar15 + -0x10), *(UINT *)(puVar15 + -0xc),*(UINT *)(puVar15 + -8), *(UINT *)(puVar15 + -4));
          puVar16 = puVar15 + -0x14;
          puVar15 = puVar15 + -0x14;
          while (iVar10 != 0) {
            if (local_44 == 0x12) goto LAB_00405fea;
            *(undefined1 **)(puVar16 + -4) = local_48;
            *(undefined4 *)(puVar16 + -8) = 0x405f78;
            TranslateMessage(*(MSG **)(puVar16 + -4));
            *(undefined1 **)(puVar16 + -8) = local_48;
            *(undefined4 *)(puVar16 + -0xc) = 0x405f7e;
            DispatchMessageA(*(MSG **)(puVar16 + -8));
            *(undefined4 *)(puVar16 + -0xc) = 1;
            *(undefined4 *)(puVar16 + -0x10) = 0;
            *(undefined4 *)(puVar16 + -0x14) = 0;
            *(undefined4 *)(puVar16 + -0x18) = 0;
            puVar15 = puVar16 + -0x1c;
            *(undefined1 **)(puVar16 + -0x1c) = local_48;
            *(undefined4 *)(puVar16 + -0x20) = 0x405f8c;
            iVar10 = PeekMessageA(*(LPMSG *)(puVar16 + -0x1c),*(HWND *)(puVar16 + -0x18), *(UINT *)(puVar16 + -0x14),*(UINT *)(puVar16 + -0x10), *(UINT *)(puVar16 + -0xc));
            puVar16 = puVar16 + -0x1c;
          }
        }
        DAT_004b39b0 = 0;
      }
      *(undefined4 *)(puVar15 + -4) = 0x405fa5;
      FUN_00406ee0();
      _DAT_004b18f0 = _DAT_004b18f0 + 1;
    }
    *(undefined4 *)(puVar15 + -4) = 1;
    *(undefined4 *)(puVar15 + -8) = 0;
    *(undefined4 *)(puVar15 + -0xc) = 0;
    *(undefined4 *)(puVar15 + -0x10) = 0;
    puVar14 = puVar15 + -0x14;
    *(undefined1 **)(puVar15 + -0x14) = local_2c;
    *(undefined4 *)(puVar15 + -0x18) = 0x405fb9;
    iVar10 = PeekMessageA(*(LPMSG *)(puVar15 + -0x14),*(HWND *)(puVar15 + -0x10), *(UINT *)(puVar15 + -0xc),*(UINT *)(puVar15 + -8),*(UINT *)(puVar15 + -4)) ;
    puVar16 = puVar15 + -0x14;
    while (puVar15 = puVar14, iVar10 != 0) {
      *(undefined1 **)(puVar16 + -4) = local_2c;
      *(undefined4 *)(puVar16 + -8) = 0x405fc3;
      TranslateMessage(*(MSG **)(puVar16 + -4));
      *(undefined1 **)(puVar16 + -8) = local_2c;
      *(undefined4 *)(puVar16 + -0xc) = 0x405fc9;
      DispatchMessageA(*(MSG **)(puVar16 + -8));
      *(undefined4 *)(puVar16 + -0xc) = 1;
      *(undefined4 *)(puVar16 + -0x10) = 0;
      *(undefined4 *)(puVar16 + -0x14) = 0;
      *(undefined4 *)(puVar16 + -0x18) = 0;
      puVar14 = puVar16 + -0x1c;
      *(undefined1 **)(puVar16 + -0x1c) = local_2c;
      *(undefined4 *)(puVar16 + -0x20) = 0x405fd7;
      iVar10 = PeekMessageA(*(LPMSG *)(puVar16 + -0x1c),*(HWND *)(puVar16 + -0x18), *(UINT *)(puVar16 + -0x14),*(UINT *)(puVar16 + -0x10), *(UINT *)(puVar16 + -0xc));
      puVar16 = puVar16 + -0x1c;
    }
  }
  if (DAT_0045f2b0 != 0) {
    *(undefined4 *)(puVar16 + -4) = 0x104;
    *(CHAR **)(puVar16 + -8) = local_250;
    *(undefined4 *)(puVar16 + -0xc) = *(undefined4 *)(PTR_DAT_0045f344 + 0x10);
    puVar15 = puVar16 + -0x10;
    *(HINSTANCE *)(puVar16 + -0x10) = param_1;
    *(undefined4 *)(puVar16 + -0x14) = 0x406019;
    LoadStringA(*(HINSTANCE *)(puVar16 + -0x10),*(UINT *)(puVar16 + -0xc),*(LPSTR *)(puVar16 + -8), *(int *)(puVar16 + -4));
    if (DAT_0045f2b0 == 0x13) {
      *(undefined4 *)(puVar16 + -0x14) = 0x4060b5;
      FUN_0040dd40();
      goto LAB_004060b5;
    }
    if (DAT_0045f2b0 == 0x14) goto LAB_00406039;
    DAT_0045f2b0 = 1;
    puVar15 = puVar16 + -0x10;
    goto LAB_00405db9;
  }
  *(undefined4 *)(puVar16 + -4) = 0;
  *(undefined4 *)(puVar16 + -8) = 0x4060cc;
  PostQuitMessage(*(int *)(puVar16 + -4));
  *(undefined4 *)(puVar16 + -8) = 1;
  *(undefined4 *)(puVar16 + -0xc) = 0;
  *(undefined4 *)(puVar16 + -0x10) = 0;
  *(undefined4 *)(puVar16 + -0x14) = 0;
  puVar15 = puVar16 + -0x18;
  *(undefined1 **)(puVar16 + -0x18) = local_2c;
  *(undefined4 *)(puVar16 + -0x1c) = 0x4060da;
  iVar10 = PeekMessageA(*(LPMSG *)(puVar16 + -0x18),*(HWND *)(puVar16 + -0x14), *(UINT *)(puVar16 + -0x10),*(UINT *)(puVar16 + -0xc),*(UINT *)(puVar16 + -8) );
  puVar16 = puVar16 + -0x18;
  while (iVar10 != 0) {
    *(undefined1 **)(puVar16 + -4) = local_2c;
    *(undefined4 *)(puVar16 + -8) = 0x4060e4;
    TranslateMessage(*(MSG **)(puVar16 + -4));
    *(undefined1 **)(puVar16 + -8) = local_2c;
    *(undefined4 *)(puVar16 + -0xc) = 0x4060ea;
    DispatchMessageA(*(MSG **)(puVar16 + -8));
    *(undefined4 *)(puVar16 + -0xc) = 1;
    *(undefined4 *)(puVar16 + -0x10) = 0;
    *(undefined4 *)(puVar16 + -0x14) = 0;
    *(undefined4 *)(puVar16 + -0x18) = 0;
    puVar15 = puVar16 + -0x1c;
    *(undefined1 **)(puVar16 + -0x1c) = local_2c;
    *(undefined4 *)(puVar16 + -0x20) = 0x4060f8;
    iVar10 = PeekMessageA(*(LPMSG *)(puVar16 + -0x1c),*(HWND *)(puVar16 + -0x18), *(UINT *)(puVar16 + -0x14),*(UINT *)(puVar16 + -0x10), *(UINT *)(puVar16 + -0xc));
    puVar16 = puVar16 + -0x1c;
  }
LAB_004060fc:
  DAT_0045f2b0 = 0;
  if (DAT_004b1d04 == 1) {
    *(undefined **)(puVar15 + -4) = (undefined *)(&DAT_007c9360);
    *(undefined4 *)(puVar15 + -8) = 0x40611a;
    ((long (*)())FUN_004095a0)();
  }
  *(undefined4 *)(puVar15 + -4) = 0x406122;
  FUN_00402790();
  *(undefined4 *)(puVar15 + -4) = 0x406127;
  FUN_00447730();
  *(undefined4 *)(puVar15 + -4) = 0x40612c;
  FUN_0041e280();
  *(undefined4 *)(puVar15 + -4) = 0x406131;
  FUN_0041ddc0();
  *(undefined4 *)(puVar15 + -4) = 0x406136;
  FUN_00423b30();
  *(undefined4 *)(puVar15 + -4) = 0x40613b;
  FUN_004474a0();
  *(HWND **)(puVar15 + -4) = (void **)(&DAT_004b1cec);
  *(undefined4 *)(puVar15 + -8) = 0x406145;
  /* LP64: assembly 0x40613b pushes 0x4b1cec (the HWND cell); the -4 slot
   * staging is callee-invisible on LP64. */
  FUN_0040dc80((HWND *)&DAT_004b1cec);
  *(undefined4 *)(puVar15 + -4) = 0x40614d;
  FUN_0040e1b0();
  return local_24;
LAB_00406039:
  if (DAT_009ca838 != (HWND)0x0) {
    *(undefined4 *)(puVar16 + -0x14) = 0;
    *(undefined4 *)(puVar16 + -0x18) = 0x406049;
    ((long (*)())FUN_0040e1d0)();
    *(undefined4 *)(puVar16 + -0x14) = 0x406051;
    FUN_0040dd40();
  }
  *(undefined4 *)(puVar16 + -0x14) = 4;
  *(char **)(puVar16 + -0x18) = STR_MSGBOX_TITLE;
  *(CHAR **)(puVar16 + -0x1c) = local_250;
  *(HWND *)(puVar16 + -0x20) = (HWND)(uintptr_t)(DAT_004b1cec);
  DAT_0045f2b0 = 0;
  *(undefined4 *)(puVar16 + -0x24) = 0x406076;
  iVar10 = MessageBoxA(*(HWND *)(puVar16 + -0x20),*(LPCSTR *)(puVar16 + -0x1c), *(LPCSTR *)(puVar16 + -0x18),*(UINT *)(puVar16 + -0x14));
  puVar15 = puVar16 + -0x20;
  if (iVar10 == 7) {
    puVar15 = puVar16 + -0x20;
    if (DAT_009ca7b8 == 0) {
LAB_004060b5:
      DAT_0045f2b0 = 1;
    }
    else {
      *(undefined4 *)(puVar16 + -0x24) = 1;
      DAT_0045f2b0 = 0x13;
      *(undefined4 *)(puVar16 + -0x28) = 0x406099;
      ((long (*)())FUN_0040e1d0)();
      *(undefined4 *)(puVar16 + -0x24) = 0x4060a1;
      FUN_0040dd40();
      DAT_0045f2b0 = 1;
      puVar15 = puVar16 + -0x20;
    }
  }
  goto LAB_00405db9;
}
