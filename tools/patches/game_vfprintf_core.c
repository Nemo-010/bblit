/* game_vfprintf_core @ 00453400 -- hand-port maintained in tools/patches, spliced into
 * linux/bblit_game.c by tools/fix_loop.py.  Reason for the hand
 * port is documented at the top of the body.
 */
/* Format-char class table: binary bytes at 0x45cd30+c for c in
 * 0x20..0x78 (extracted from bugs.exe; the decompile renders the base as
 * the "?IsProcessorFeaturePresent" string, whose tail overlaps the table).
 * A C string literal cannot reproduce the following bytes. */
static const unsigned char fmt_class[89] = {
  6, 0, 0, 6, 0, 1, 0, 0, 0, 0, 3, 6, 0, 6, 2, 0,
  4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0,
  0, 0, 8, 8, 0, 8, 7, 8, 0, 7, 0, 0, 7, 0, 7, 0,
  0, 0, 0, 8, 0, 0, 0, 0, 8, 0, 8, 0, 0, 0, 0, 0,
  0, 0, 0, 8, 8, 8, 8, 8, 7, 8, 0, 0, 7, 0, 8, 8,
  8, 0, 0, 8, 0, 8, 0, 7, 8,
};

undefined4 game_vfprintf_core(undefined4 param_1,char *param_2,data_u32 *param_3) {
  char cVar1;
  undefined1 uVar2;
  short sVar3;
  uint uVar4;
  undefined4 uVar5;
  short *psVar6;
  undefined4 *puVar7;
  uint uVar8;
  int iVar9;
  int iVar10;
  int iVar11;
  short *psVar12;
  char cVar13;
  undefined1 *puVar14;
  undefined1 *puVar15;
  undefined1 *puVar16;
  undefined2 *puVar17;
  char *pcVar18;
  byte bVar19;
  ulonglong uVar20;
  longlong lVar21;
  /* The decompile's outgoing-argument slots run from -0x24 to +0x264 around
   * the frame anchor.  The generated code anchored that on `aiStack_2b4 - 8`,
   * a 40-byte array, so every slot past +0x20 addressed memory outside the
   * object.  vf_frame is 0x400 bytes with the anchor 0x40 in, which covers
   * [-0x40, +0x3c0) and so every slot the function uses. */
  static unsigned char vf_frame[0x400];
  char *f00453400_frame = (char *)vf_frame + 0x40; /* frame anchor */
  uint local_24c [3];
  char local_240 [12];
  int local_234;
  int local_22c [6];
  undefined4 local_214 [4];
  int local_204 [114];
  int aiStack_3c [15];
  
  bVar19 = 0;
  puVar15 = (void *)(char *)f00453400_frame;
  iVar11 = 0;
  puVar16 = (undefined1 *)0x0;
  /* LP64 va/struct convention (replaces x86 slot staging).  On x86 the
   * +0x260 slot carried the first arg (the print struct: [esp+0x260] is the
   * arg slot, verified at the 0x453b70 call sequence) and +0x268 the va
   * cursor (third arg).  Neither arrives in the port, so both are seeded
   * from the C params; every slot traffic below then works unchanged in
   * the vf_frame arena.  Outgoing-arg slots (-4..-0x24) are callee-
   * invisible on LP64 and became direct calls.  Companion changes:
   * FUN_00453e60's cursor cell widened to uintptr_t, FUN_00453e20 void ->
   * long with silicon-faithful returns, __aullrem/__aulldiv are native
   * 64-bit (divisor-hi slot +0x48 is never stored).  Deferred (unreached,
   * same disease): the wide-char 53ea0/58400 sites, the float indirect
   * calls, 53e80, and sprintf va-forwarding at FUN_00450420/450490. */
  *(undefined4 *)(puVar15 + 0x260) = param_1;
  *(undefined1 **)(puVar15 + 0x268) = (undefined1 *)param_3;
  /* the decompile drops two more x86 arrivals the loop depends on: the
   * counter at +0x1c (mov [esp+0x1c],0 at 0x453415; stale arena state made
   * later calls loop wrong) and the format cursor at +0x264 (mov at
   * 0x453422; a zero slot faulted the second print at game_vfprintf:35182). */
  *(int *)(puVar15 + 0x1c) = 0;
  /* LP64: +0x264 is a 4-byte x86 slot; an 8-byte char* store's high half
   * zeroes the low 4 bytes of the +0x268 va cursor (observed: first %d
   * fetch faulted on cur=0x7ffd00000000).  The format cursor is always an
   * image address, so 4-byte traffic is exact. */
  *(undefined4 *)(puVar15 + 0x264) = (undefined4)(uintptr_t)param_2;
  cVar13 = *param_2;
  param_2 = param_2 + 1;
  puVar14 = (void *)(char *)f00453400_frame;
  if (cVar13 != '\0') {
    while (puVar15 = puVar14, -1 < *(int *)(puVar15 + 0x1c)) {
      if ((cVar13 < ' ') || ('x' < cVar13)) {
        uVar4 = 0;
      }
      else {
        /* LP64: the class bytes are the binary's own data at 0x45cd30+c
         * (verified: 'l'->7, 'x'->8); the C string literal is followed by
         * unrelated .rodata so indexing 57..145 past it read garbage. */
        uVar4 = fmt_class[cVar13 - ' '] & 0xf;
      }
      /* LP64: the dispatch reads BYTES (assembly movsx eax, BYTE PTR
       * [ecx+eax*8+0x45cd50]); data_u32* indexing lands 4x too far
       * (observed: literal 's' read state 6 instead of 0, literal 'n'
       * fell into case 'n' and stored through a NULL fetch). */
      cVar1 = ((signed char *)&DAT_0045cd50)[uVar4 * 8 + iVar11];
      *(int *)(puVar15 + 0x3c) = (int)cVar1 >> 4;
      /* assembly 0x453470: cmp eax,7 / ja 0x453c3a — states 8..15 skip the
       * 8-case dispatch and advance to the next format char. */
      if (7 < ((int)cVar1 >> 4)) {
        goto LAB_00453c3a;
      }
      switch((int)cVar1 >> 4) {
      case 0:
switchD_0045347d_caseD_0:
        *(undefined4 *)(puVar15 + 0x2c) = 0;
        if ((PTR_DAT_004b0a98[(*(uint *)(puVar15 + 0x40) & 0xff) * 2 + 1] & 0x80) != 0) {
          FUN_00453d90((int)cVar13,(int *)(uintptr_t)(*(undefined4 *)(puVar15 + 0x260)),(int *)(puVar15 + 0x1c));
          cVar13 = *param_2;
          *(undefined4 *)(puVar15 + 0x264) = (undefined4)(uintptr_t)(param_2 + 1);
        }
        FUN_00453d90((int)cVar13,(int *)(uintptr_t)(*(undefined4 *)(puVar15 + 0x260)),(int *)(puVar15 + 0x1c));
        break;
      case 1:
        *(undefined4 *)(puVar15 + 0x44) = 0;
        *(undefined4 *)(puVar15 + 0x34) = 0;
        *(undefined4 *)(puVar15 + 0x28) = 0;
        *(undefined4 *)(puVar15 + 0x24) = 0;
        *(undefined4 *)(puVar15 + 0x10) = 0;
        *(undefined4 *)(puVar15 + 0x18) = 0xffffffff;
        *(undefined4 *)(puVar15 + 0x2c) = 0;
        break;
      case 2:
        switch(cVar13) {
        case ' ':
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 2;
          break;
        case '#':
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x80;
          break;
        case '+':
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 1;
          break;
        case '-':
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 4;
          break;
        case '0':
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 8;
        }
        break;
      case 3:
        if (cVar13 == '*') {
                    iVar11 = (int)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
          *(int *)(puVar15 + 0x28) = iVar11;
          if (iVar11 < 0) {
            *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 4;
            *(int *)(puVar15 + 0x28) = -iVar11;
          }
        }
        else {
          *(int *)(puVar15 + 0x28) = cVar13 + -0x30 + local_234 * 10;
        }
        break;
      case 4:
        *(undefined4 *)(puVar15 + 0x18) = 0;
        break;
      case 5:
        if (cVar13 == '*') {
                    iVar11 = (int)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
          *(int *)(puVar15 + 0x18) = iVar11;
          if (iVar11 < 0) {
            *(undefined4 *)(puVar15 + 0x18) = 0xffffffff;
          }
        }
        else {
          *(int *)(puVar15 + 0x18) = cVar13 + -0x30 + *(int *)(puVar15 + 0x18) * 10;
        }
        break;
      case 6:
        switch(cVar13) {
        case 'I':
          if ((*param_2 != '6') || (param_2[1] != '4')) {
            *(undefined4 *)(puVar15 + 0x3c) = 0;
            goto switchD_0045347d_caseD_0;
          }
          *(undefined4 *)(puVar15 + 0x264) = (undefined4)(uintptr_t)(param_2 + 2);
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x8000;
          break;
        case 'h':
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x20;
          break;
        case 'l':
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x10;
          break;
        case 'w':
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x800;
        }
        break;
      case 7:
        switch(cVar13) {
        case 'C':
          if ((*(uint *)(puVar15 + 0x10) & 0x830) == 0) {
            *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x800;
          }
        case 'c':
          if ((*(uint *)(puVar15 + 0x10) & 0x810) == 0) {
                        uVar2 = (undefined1)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
            puVar15[0x5c] = uVar2;
            puVar16 = (undefined1 *)0x1;
          }
          else {
            *(undefined1 **)(puVar15 + -4) = puVar15 + 0x268;
            *(undefined4 *)(puVar15 + -8) = 0x4536c1;
            uVar5 = ((long (*)())FUN_00453ea0)();
            *(undefined4 *)(puVar15 + -4) = uVar5;
            *(undefined1 **)(puVar15 + -8) = puVar15 + 0x5c;
            *(undefined4 *)(puVar15 + -0xc) = 0x4536cf;
            puVar16 = (undefined1 *)((long (*)())FUN_00458400)();
            if ((long)(uintptr_t)puVar16 < 0) {
              *(undefined4 *)(puVar15 + 0x34) = 1;
              *(undefined1 **)(puVar15 + 0x14) = puVar15 + 0x5c;
              break;
            }
          }
          *(undefined1 **)(puVar15 + 0x14) = puVar15 + 0x5c;
          break;
        case 'E':
        case 'G':
          *(undefined4 *)(puVar15 + 0x44) = 1;
          cVar13 = cVar13 + ' ';
        case 'e':
        case 'f':
        case 'g':
          *(undefined1 **)(puVar15 + 0x14) = puVar15 + 0x5c;
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x40;
          if (*(int *)(puVar15 + 0x18) < 0) {
            *(undefined4 *)(puVar15 + 0x18) = 6;
          }
          else if ((*(int *)(puVar15 + 0x18) == 0) && (cVar13 == 'g')) {
            *(undefined4 *)(puVar15 + 0x18) = 1;
          }
          puVar7 = *(undefined4 **)(puVar15 + 0x268);
          iVar11 = *(int *)(puVar15 + 0x18);
          *(undefined4 **)(puVar15 + 0x268) = puVar7 + 2;
          *(undefined4 *)(puVar15 + 0x4c) = *puVar7;
          *(undefined4 *)(puVar15 + 0x50) = puVar7[1];
          *(undefined4 *)(puVar15 + -4) = *(undefined4 *)(puVar15 + 0x44);
          *(int *)(puVar15 + -8) = iVar11;
          *(int *)(puVar15 + -0xc) = (int)cVar13;
          *(undefined1 **)(puVar15 + -0x10) = puVar15 + 0x5c;
          *(undefined1 **)(puVar15 + -0x14) = puVar15 + 0x4c;
          *(undefined4 *)(puVar15 + -0x18) = 0x4538e3;
          (*(code *)PTR_FUN_004b0908)();
          uVar4 = *(uint *)(puVar15 + 0x10);
          if (((uVar4 & 0x80) != 0) && (iVar11 == 0)) {
            *(undefined1 **)(puVar15 + -4) = puVar15 + 0x5c;
            *(undefined4 *)(puVar15 + -8) = 0x453901;
            (*(code *)PTR_FUN_004b0914)();
          }
          if ((cVar13 == 'g') && ((uVar4 & 0x80) == 0)) {
            *(undefined1 **)(puVar15 + -4) = puVar15 + 0x5c;
            *(undefined4 *)(puVar15 + -8) = 0x453918;
            (*(code *)PTR_FUN_004b090c)();
          }
          if (puVar15[0x5c] == '-') {
            *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x100;
            *(undefined1 **)(puVar15 + 0x14) = puVar15 + 0x5d;
          }
          uVar4 = 0xffffffff;
          pcVar18 = *(char **)(puVar15 + 0x14);
          do {
            if (uVar4 == 0) break;
            uVar4 = uVar4 - 1;
            cVar13 = *pcVar18;
            pcVar18 = pcVar18 + (long)(int)((uint)bVar19 * -2 + 1);
          } while (cVar13 != '\0');
          puVar16 = (undefined1 *)(~uVar4 - 1);
          break;
        case 'S':
          if ((*(uint *)(puVar15 + 0x10) & 0x830) == 0) {
            *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x800;
          }
        case 's':
          iVar11 = 0x7fffffff;
          if (*(int *)(puVar15 + 0x18) != -1) {
            iVar11 = *(int *)(puVar15 + 0x18);
          }
                    psVar6 = (short *)(uintptr_t)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
          *(short **)(puVar15 + 0x14) = psVar6;
          if ((*(uint *)(puVar15 + 0x10) & 0x810) == 0) {
            psVar12 = psVar6;
            if (psVar6 == (short *)0x0) {
              *(undefined **)(puVar15 + 0x14) = (undefined *)(PTR_DAT_004b0a90);
              psVar6 = (short *)PTR_DAT_004b0a90;
              psVar12 = (short *)PTR_DAT_004b0a90;
            }
            for (; (iVar11 != 0 && (iVar11 = iVar11 + -1, (char)*psVar6 != '\0'));
                psVar6 = (short *)((long)(uintptr_t)psVar6 + 1)) {
            }
            puVar16 = (undefined1 *)((long)(uintptr_t)psVar6 - (long)(uintptr_t)psVar12);
          }
          else {
            if (psVar6 == (short *)0x0) {
              *(undefined **)(puVar15 + 0x14) = (undefined *)(PTR_DAT_004b0a94);
              psVar6 = (short *)PTR_DAT_004b0a94;
            }
            *(undefined4 *)(puVar15 + 0x2c) = 1;
            for (psVar12 = psVar6; (iVar11 != 0 && (iVar11 = iVar11 + -1, *psVar12 != 0));
                psVar12 = psVar12 + 1) {
            }
            puVar16 = (undefined1 *)((long)(uintptr_t)psVar12 - (long)(uintptr_t)psVar6 >> 1);
          }
          break;
        case 'X':
          goto switchD_00453691_caseD_58;
        case 'Z':
                    psVar6 = (short *)(uintptr_t)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
          if ((psVar6 == (short *)0x0) || (iVar11 = *(int *)(psVar6 + 2), iVar11 == 0)) {
            uVar4 = 0xffffffff;
            *(undefined **)(puVar15 + 0x14) = (undefined *)(PTR_DAT_004b0a90);
            pcVar18 = (char *)(PTR_DAT_004b0a90);
            do {
              if (uVar4 == 0) break;
              uVar4 = uVar4 - 1;
              cVar13 = *pcVar18;
              pcVar18 = pcVar18 + (long)(int)((uint)bVar19 * -2 + 1);
            } while (cVar13 != '\0');
            puVar16 = (undefined1 *)(~uVar4 - 1);
          }
          else if ((*(uint *)(puVar15 + 0x10) & 0x800) == 0) {
            puVar16 = (undefined1 *)(int)*psVar6;
            *(undefined4 *)(puVar15 + 0x2c) = 0;
            *(int *)(puVar15 + 0x14) = iVar11;
          }
          else {
            sVar3 = *psVar6;
            *(int *)(puVar15 + 0x14) = iVar11;
            *(undefined4 *)(puVar15 + 0x2c) = 1;
            puVar16 = (undefined1 *)((uint)(int)sVar3 >> 1);
          }
          break;
        case 'd':
        case 'i':
          *(undefined4 *)(puVar15 + 0x30) = 10;
          *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x40;
          goto LAB_004539c7;
        case 'n':
                    puVar7 = (undefined4 *)(uintptr_t)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
          if ((puVar15[0x10] & 0x20) == 0) {
            *(undefined4 *)(puVar15 + 0x34) = 1;
            *puVar7 = *(undefined4 *)(puVar15 + 0x1c);
          }
          else {
            *(undefined4 *)(puVar15 + 0x34) = 1;
            *(undefined2 *)puVar7 = *(undefined2 *)(puVar15 + 0x1c);
          }
          break;
        case 'o':
          *(undefined4 *)(puVar15 + 0x30) = 8;
          if ((puVar15[0x10] & 0x80) != 0) {
            *(uint *)(puVar15 + 0x10) = *(uint *)(puVar15 + 0x10) | 0x200;
          }
          goto LAB_004539c7;
        case 'p':
          *(undefined4 *)(puVar15 + 0x18) = 8;
switchD_00453691_caseD_58:
          *(undefined4 *)(puVar15 + 0x38) = 7;
LAB_00453982:
          *(undefined4 *)(puVar15 + 0x30) = 0x10;
          if ((puVar15[0x10] & 0x80) != 0) {
            puVar15[0x22] = 0x30;
            *(undefined4 *)(puVar15 + 0x24) = 2;
            puVar15[0x23] = puVar15[0x38] + 'Q';
          }
          goto LAB_004539c7;
        case 'u':
          *(undefined4 *)(puVar15 + 0x30) = 10;
LAB_004539c7:
          uVar4 = *(uint *)(puVar15 + 0x10);
          if ((uVar4 & 0x8000) == 0) {
            if ((uVar4 & 0x20) == 0) {
              if ((uVar4 & 0x40) == 0) {
                                uVar8 = (undefined4)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
                uVar20 = (ulonglong)uVar8;
              }
              else {
                                iVar11 = (int)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
                uVar20 = (ulonglong)iVar11;
              }
            }
            else if ((uVar4 & 0x40) == 0) {
                            uVar8 = (undefined4)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
              uVar20 = (ulonglong)(uVar8 & 0xffff);
            }
            else {
                            sVar3 = (short)FUN_00453e60((uintptr_t *)(puVar15 + 0x268));
              uVar20 = (ulonglong)(int)sVar3;
            }
          }
          else {
            *(undefined1 **)(puVar15 + -4) = puVar15 + 0x268;
            *(undefined4 *)(puVar15 + -8) = 0x4539dd;
            uVar20 = ((long (*)())FUN_00453e80)();
          }
          if ((((uVar4 & 0x40) != 0) && ((longlong)uVar20 < 0x100000000)) && ((longlong)uVar20 < 0)) {
            uVar20 = CONCAT44(-((int)(uVar20 >> 0x20) + (uint)((int)uVar20 != 0)),-(int)uVar20);
            uVar4 = uVar4 | 0x100;
            *(uint *)(puVar15 + 0x10) = uVar4;
          }
          iVar11 = (int)(uVar20 >> 0x20);
          if ((uVar4 & 0x8000) == 0) {
            iVar11 = 0;
          }
          lVar21 = CONCAT44(iVar11,(int)uVar20);
          iVar10 = *(int *)(puVar15 + 0x18);
          if (iVar10 < 0) {
            iVar10 = 1;
          }
          else {
            *(uint *)(puVar15 + 0x10) = uVar4 & 0xfffffff7;
          }
          if ((int)uVar20 == 0 && iVar11 == 0) {
            *(undefined4 *)(puVar15 + 0x24) = 0;
          }
          puVar14 = puVar15 + 0x25b;
          *(undefined1 **)(puVar15 + 0x14) = puVar14;
          while ((*(int *)(puVar15 + 0x18) = iVar10 + -1, 0 < iVar10 || (lVar21 != 0))) {
            iVar11 = *(int *)(puVar15 + 0x30);
            /* LP64: native 64-bit divide; x86 passed (lo,hi,blo,bhi). */
            iVar9 = (int)__aullrem((ulonglong)lVar21,(ulonglong)(long long)iVar11);
            iVar9 = iVar9 + 0x30;
            /* LP64: native 64-bit divide; divisor-hi slot (+0x48) is never
             * stored anywhere in this function, so it reads as 0 (entry note). */
            lVar21 = (long long)__aulldiv((ulonglong)lVar21,(ulonglong)(long long)iVar11);
            /* assembly 0x453ae2: add ebx,[esp+0x38] — the hex-letter bias
             * (+0x38 = 7 for %X, 0x27 for %x), not the +0x18 counter. */
            if (0x39 < iVar9) {
              iVar9 = iVar9 + *(int *)(puVar15 + 0x38);
            }
            /* LP64: the decompile maps the [esp+0x14] digit pointer and
             * [esp+0x18] counter onto outgoing-arg slots -0xc/-8
             * (callee-invisible on LP64; -0xc read faulted) and invents a
             * -0x20 frame walk the assembly has no trace of (the loop is
             * 0x453aa2-0x453af5: reload, store, dec, spill, jmp). */
            /* LP64: +0x14 (digit pointer) and +0x18 (counter) are adjacent
             * 4-byte x86 slots; the 8-byte pointer spill's high half IS
             * +0x18, so the while-condition's counter store zeroes... then
             * destroys the pointer (observed: 2nd digit read
             * slot14=0xffffffff4863a2da).  The C locals already carry both
             * values across the pure div calls, so the loop keeps them in
             * locals and never respills.  (Single-digit values survived
             * only because post-loop code uses the local.) */
            iVar10 = *(int *)(puVar15 + 0x18);
            *puVar14 = (char)iVar9;
            puVar14 = puVar14 + -1;
          }
          puVar16 = puVar15 + (0x25b - (long)(uintptr_t)puVar14);
          *(undefined1 **)(puVar15 + 0x14) = puVar14 + 1;
          if (((*(uint *)(puVar15 + 0x10) & 0x200) != 0) && ((puVar14[1] != '0' || (puVar16 == (undefined1 *)0x0)))) {
            puVar16 = puVar16 + 1;
            *(undefined1 **)(puVar15 + 0x14) = puVar14;
            *puVar14 = 0x30;
          }
          break;
        case 'x':
          *(undefined4 *)(puVar15 + 0x38) = 0x27;
          goto LAB_00453982;
        }
        if (*(int *)(puVar15 + 0x34) == 0) {
          uVar4 = *(uint *)(puVar15 + 0x10);
          if ((uVar4 & 0x40) != 0) {
            if ((uVar4 & 0x100) == 0) {
              if ((uVar4 & 1) == 0) {
                if ((uVar4 & 2) == 0) goto LAB_00453b5f;
                puVar15[0x22] = 0x20;
              }
              else {
                puVar15[0x22] = 0x2b;
              }
            }
            else {
              puVar15[0x22] = 0x2d;
            }
            *(undefined4 *)(puVar15 + 0x24) = 1;
          }
LAB_00453b5f:
          iVar11 = (*(int *)(puVar15 + 0x28) - *(int *)(puVar15 + 0x24)) - (long)(uintptr_t)puVar16;
          if ((uVar4 & 0xc) == 0) {
            uVar5 = param_1;
            FUN_00453de0(0x20,iVar11,uVar5,(int *)(puVar15 + 0x1c));
          }
          else {
            uVar5 = param_1;
          }
          FUN_00453e20((char *)(puVar15 + 0x22),*(int *)(puVar15 + 0x24),uVar5,(int *)(puVar15 + 0x1c));
          if (((uVar4 & 8) != 0) && ((uVar4 & 4) == 0)) {
            FUN_00453de0(0x30,iVar11,uVar5,(int *)(puVar15 + 0x1c));
          }
          iVar10 = *(int *)(puVar15 + 0x2c);
          if ((iVar10 == 0) || ((long)(uintptr_t)puVar16 < 1)) {
            FUN_00453e20(*(char **)(puVar15 + 0x14),(int)(uintptr_t)puVar16,uVar5,(int *)(puVar15 + 0x1c));
          }
          else {
            puVar17 = *(undefined2 **)(puVar15 + 0x14);
            puVar14 = puVar16;
            do {
              puVar14 = puVar14 + -1;
              iVar10 = FUN_00458400((LPSTR)(puVar15 + 0x48),(ushort)*puVar17);
              puVar17 = puVar17 + 1;
              if (iVar10 < 1) break;
              iVar10 = (int)FUN_00453e20((char *)(puVar15 + 0x48),iVar10,*(undefined4 *)(puVar15 + 0x260),(int *)(puVar15 + 0x1c));
            } while (puVar14 != (undefined1 *)0x0);
            uVar4 = *(uint *)(puVar15 + 0x10);
          }
          if ((uVar4 & 4) != 0) {
            FUN_00453de0(0x20,iVar11,uVar5,(int *)(puVar15 + 0x1c));
          }
        }
      }
LAB_00453c3a:
      cVar13 = *(char *)(uintptr_t)(*(undefined4 *)(puVar15 + 0x264));
      param_2 = (char *)(uintptr_t)(*(undefined4 *)(puVar15 + 0x264)) + 1;
      puVar15[0x40] = cVar13;
      *(undefined4 *)(puVar15 + 0x264) = (undefined4)(uintptr_t)param_2;
      if (cVar13 == '\0') break;
      local_234 = *(int *)(puVar15 + 0x28);
      iVar11 = *(int *)(puVar15 + 0x3c);
      puVar14 = puVar15;
    }
  }
  return *(undefined4 *)(puVar15 + 0x1c);
}
