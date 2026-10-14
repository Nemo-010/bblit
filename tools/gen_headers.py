#!/usr/bin/env python3
"""gen_headers.py -- generate linux/data_syms.h/.c for the bblit Linux port.

Architecture: the whole PE image outside .text is reconstructed at its ORIGINAL
addresses at runtime.  bblit_port_init() mmaps [0x400000, 0x9da000), copies the
original bytes out of bblit_image_blob[], plants 12-byte `movabs;jmp rax`
trampolines at every original function address so data-held code pointers work,
then widens referenced pointer-typed slots to full 8-byte pointers.

Because every data symbol keeps its original address, all Ghidra-isms compile
natively and behave like the 32-bit original:

    DAT_x          typed lvalue view at its real address
    (&DAT_x)[i]    correct stride for the symbol's width
    &DAT_x + k     byte/word/dword scaling exactly as the original
    s_xxx          const char[size] array lvalue (sizeof and DATAPART work)
    PTR_DAT_x      8-byte pointer lvalue (original held a 4-byte slot)

Inputs: tools/symbols.csv (labels, types, sizes), tools/databytes.csv (raw
bytes from the Ghidra project), linux/bblit_game.c (referenced symbols),
linux/bblit_game.h (kept-function prototypes for trampoline targets).
"""
import re, sys, os, json, argparse, collections

BLOB_BASE = 0x400000
BLOB_END  = 0x9da000          # .reloc end 0x9d99ff rounded up
CODE_END  = 0x45c000          # first byte after .text; trampoline range

def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument('--symbols', default='tools/symbols.csv')
    ap.add_argument('--bytes', default='tools/databytes.csv')
    ap.add_argument('--game', default='linux/bblit_game.c')
    ap.add_argument('--gamehdr', default='linux/bblit_game.h')
    ap.add_argument('--overrides', default='linux/data_overrides.json')
    ap.add_argument('--renames', default='tools/renames.json')
    ap.add_argument('--out', default='linux/data_syms.h')
    ap.add_argument('--body-out', default='linux/data_syms.c')
    return ap.parse_args()

WIDTH_BY_TYPE = [
    (re.compile(r'^(undefined1|byte|uchar|undefined)$'), 1),
    (re.compile(r'^(undefined2|word|ushort)$'), 2),
    (re.compile(r'^(undefined4|dword|uint|int|long)$'), 4),
    (re.compile(r'^(undefined8|qword|unkbyte8|longlong|ulonglong)$'), 8),
]

def sym_width(typ, size):
    if '*' in typ:
        return 4, 'ptr'
    if typ == 'float':
        return 4, 'float'
    if typ == 'double':
        return 8, 'float'
    for rx, w in WIDTH_BY_TYPE:
        if rx.match(typ):
            return w, 'uint'
    if typ in ('string', 'TerminatedCString', 'PascalUnicode', 'TerminatedUnicode',
               'StringInfo', 'Unicode'):
        return 1, 'uint'
    # structs and anything else: array of raw bytes
    return max(size, 1), 'raw'

def main():
    args = parse_args()
    text = open(args.game, encoding='utf-8', errors='replace').read()

    # identifier rename map (functions and data); keys are 8-hex addresses or
    # full DAT_ identifiers, values are the new C names.  refs read from the
    # game file carry the NEW names, so every lookup of a Ghidra-side symbol
    # goes through rev_renames first.
    renames = {}
    rev_renames = {}
    if os.path.exists(args.renames):
        renames = json.load(open(args.renames))
        for k, v in renames.items():
            rev_renames[v] = k

    # ---- referenced data symbols ----------------------------------------
    refs = sorted(set(re.findall(r'\b(?:_DAT_[0-9A-Fa-f]{8}|DAT_[0-9A-Fa-f]{8}|PTR_[A-Za-z0-9_]+)\b', text)))
    # renamed data symbols appear under their new names; add them so their
    # defines are emitted (the loop below maps them back via rev_renames)
    for k, new in renames.items():
        if k.startswith(('DAT_', '_DAT_', 'PTR_')) and re.search(r'\b' + re.escape(new) + r'\b', text):
            refs.append(new)

    by_addr = collections.defaultdict(list)
    label_addr = {}
    for line in open(args.symbols):
        p = line.rstrip('\n').split('|')
        if p[0] != 'DATA':
            continue
        addr, size, typ, lbl = p[1], int(p[2]), p[3], p[4]
        by_addr[addr].append((lbl, size, typ))
        if lbl and lbl not in label_addr:
            label_addr[lbl] = addr

    def addr_of(sym):
        if sym.startswith('PTR'):
            if sym in label_addr:
                return label_addr[sym]
            m = re.search(r'([0-9A-Fa-f]{8})$', sym)
            return m.group(1) if m else None
        m = re.match(r'^_?DAT_([0-9A-Fa-f]{8})$', sym)
        return m.group(1) if m else None

    # ---- bytes -----------------------------------------------------------
    bytemap = {}
    for line in open(args.bytes):
        p = line.rstrip('\n').split('|')
        if p[0] != 'BYTES':
            continue
        a = int(p[2], 16)
        h = p[3]
        for i in range(len(h) // 2):
            bytemap[a + i] = int(h[2 * i:2 * i + 2], 16)

    # ---- classify each referenced address --------------------------------
    info = {}
    for sym in refs:
        a = addr_of(rev_renames.get(sym, sym))
        if a is None:
            print(f'gen_headers: WARN no address for {sym}', file=sys.stderr)
            continue
        d = info.setdefault(a, {'syms': set(), 'rows': by_addr.get(a, []),
                                'ptr': False, 'called': False})
        d['syms'].add(sym)

    for a, d in info.items():
        for sym in d['syms']:
            e = re.escape(sym)
            # code view: called through the name, or cast to code*
            if (re.search('\\(\\s*\\*?\\s*(?:code\\s*\\*\\s*)?' + e + '\\)[ \\t]*\\(', text)
                    or re.search('\\(\\s*code\\s*\\*\\s*\\)[ \\t]*&?' + e + '\\b', text)):
                d['called'] = True
        d['ptr'] = any(s.startswith('_DAT_') or s.startswith('PTR') for s in d['syms'])
        if not d['rows']:
            print(f'gen_headers: WARN no symbol row for {a}', file=sys.stderr)
        best = None
        for (lbl, sz, typ) in d['rows']:
            if best is None or sz > best[1]:
                best = (lbl, sz, typ)
        d['label'] = best[0] if best else None
        d['size'], d['kind'] = sym_width(best[2], best[1]) if best else (4, 'uint')
        # use heuristics refine the view; Ghidra leaves many pointer variables
        # typed undefined4, and decompiled call sites beat declared types.
        deref_kind = None
        for sym in sorted(d['syms']):
            e = re.escape(sym)
            is_d = sym.startswith('_DAT_') or sym.startswith('PTR')
            if is_d:
                if (re.search(r'(?:[0-9]+\.[0-9]+|fVar\w*|dVar\w*)[ \t]*\*[ \t]*' + e, text)
                        or re.search(e + r'[ \t]*\*[ \t]*[0-9]+\.[0-9]+', text)
                        or re.search(r'[0-9]+\.[0-9]+[ \t]*/[ \t]*\(?' + e, text)
                        or re.search(r'\((?:float|double)\)[ \t]*' + e + r'\b', text)
                        or re.search(r'longdouble[ \t]*\w+[ \t]*=[ \t]*' + e + r'\b', text)
                        or re.search(r'\b' + e + r'[ \t]*=[ \t]*[^\n;]{0,60}[0-9]+\.[0-9]+', text)):
                    deref_kind = 'float'
                if (re.search(e + r'[ \t]*=[ \t]*&', text)
                        or re.search(r'\(\s*(?:LPSTR|LPVOID|LPDWORD|int\s*\*|char\s*\*|undefined\d?\s*\*|HANDLE|HWND|code\s*\*)[ \t]*\)[ \t]*' + e + r'\b', text)):
                    deref_kind = deref_kind or 'ptr'
            else:
                # plain DAT_ dereferenced, indexed, or assigned an & -> the
                # variable holds pointers even if typed undefined4
                if (re.search(r'\*' + e + r'\b', text)
                        or re.search(e + r'[ \t]*\[[^\]]', text)
                        or re.search(e + r'[ \t]*=[ \t]*&', text)
                        or re.search(r'=[ \t]*\([\w ]*\*[ \t]*\)[ \t]*' + e + r'\b', text)
                        or re.search(r'\b(?:pi|ppi|pu|ppu|pc|ppc|ps|pf|pb|pv|pd|ppd|pl|ppb)\w*[ \t]*=[ \t]*' + e + r'[ \t]*;', text)):
                    d['kind'] = 'ptr'
        if deref_kind and d['kind'] == 'uint':
            d['kind'] = deref_kind

    # LP64 widen/conflict sets, shared by the header and body emitters.
    # slots_pre: pointer-typed slots outside .text (widen candidates).
    # A slot whose high half overlaps live non-slot data must not be
    # widened in place (measured: 0x4b0900's write zeroed DAT_004b0904;
    # 0x45f344's ate "BBLIT Game").  Those conflict slots are skipped in
    # bblit_ptr_slots; the ones whose macro is an 8-byte pointer load
    # (`*(data_u32 **)` / `*(code **)`) read side-table cells instead,
    # because the load would over-read the same neighbour (measured:
    # 0x45f344's load picked up "BBLI" and faulted winmain's LoadStringA).
    # Cell exclusions: the arena sentinel pair (own side-table), 0x4b0900
    # and 0x4ae8e4 (kept on explicit 4-byte views; values fit 32 bits),
    # and 0x468750 (only ever address-taken, never read).
    # NOTE: info keys are 8-hex with leading zeros and mixed case
    # (e.g. '004ae8e0'), so these sets hold ints, not strings.
    CELL_EXCLUDE = {0x4ae8e0, 0x4ae8e4, 0x4b0900, 0x4ae8e4, 0x468750}
    slots_pre = sorted((a) for a, d in info.items()
                       if (d['ptr'] or d['called'] or d['kind'] == 'ptr')
                       and int(a, 16) >= CODE_END)
    slots_pre_int = set(int(x, 16) for x in slots_pre)
    def high_half_live(a):
        s = int(a, 16)
        if (s + 4) in slots_pre_int:
            return False
        return any(bytemap.get(s + i, 0) for i in range(4, 8))
    conflict_addrs = set(a for a in slots_pre if high_half_live(a))
    slots = [a for a in slots_pre
             if not high_half_live(a) and int(a, 16) not in (0x4ae8e0, 0x4ae8e4)]
    nskip = len(slots_pre) - len(slots)
    # conflict slots needing cells (8-byte-load macros, minus exclusions).
    # Cell i mirrors the image dword, zero-extended; reads and writes go
    # through the cell at full 64-bit width.  data_u32* keeps the original
    # dword scaling for `NAME + k` uses and preserves high function
    # addresses (e.g. 0x4b091c) that a 4-byte slot could not hold.
    cell_addrs = sorted((a for a in conflict_addrs if int(a, 16) not in CELL_EXCLUDE),
                        key=lambda x: int(x, 16))

    # ---- emit data_syms.h -------------------------------------------------
    out = []
    words = (BLOB_END - BLOB_BASE) // 8
    out.append('/* ---- image mapping ---- */')
    out.append(f'#define BBLIT_BLOB_BASE   0x{BLOB_BASE:08x}u')
    out.append(f'#define BBLIT_BLOB_END    0x{BLOB_END:08x}u')
    out.append(f'#define BBLIT_CODE_END    0x{CODE_END:08x}u')
    out.append(f'#define BBLIT_BLOB_WORDS  {words}u')
    out.append('extern data_u64 bblit_image_blob[BBLIT_BLOB_WORDS];')
    out.append('/* VC6 arena-list sentinel: side-table for the 0x4ae8e0/0x4ae8e4')
    out.append(' * image slots (see the note at their sites below). */')
    out.append('extern void *bblit_arena_sentinel[6];')
    out.append('/* LP64 side-table cells for conflict slots (see above). */')
    out.append(f'extern data_u32 *bblit_slot_cells[{len(cell_addrs)}];')
    out.append('void bblit_port_init(void);')
    out.append('')
    out.append('/* ---- data symbol views (all at original addresses) ---- */')

    # addresses whose decompiled use proves a float/longdouble view; the
    # generic inference below misreads them as pointer slots
    TYPE_OVERRIDES = {
        '005f8530': ('float', 4),       # x87 compare operand, stored via fld
        '004b0dfe': ('longdouble', 10), # FPU control/operand shadow (NAN/==)
        # float slots the generic pass re-classified as pointer vars; every
        # use site is a float store/compare (see bblit_game.c)
        '004efb68': ('float', 4),
        '004efb7c': ('float', 4),
        '00621604': ('float', 4),
        '006235c4': ('float', 4),
        '006235c8': ('float', 4),
        '006235cc': ('float', 4),
        '004b0ca8': ('raw', 1),         # byte compared with (char) returns
    }

    def view_macro(a, size, kind):
        addr = f'0x{a}ul'
        if kind == 'ptr':
            return f'(*(void **){addr})'
        if kind == 'longdouble':
            return f'(*(longdouble *){addr})'
        if kind == 'float':
            return f'(*(double *){addr})' if size == 8 else f'(*(float *){addr})'
        if kind == 'raw':
            return f'(*(unsigned char (*)[{size}]){addr})'
        if size == 1:
            return f'(*(unsigned char *){addr})'
        if size == 2:
            return f'(*(unsigned short *){addr})'
        if size == 8:
            return f'(*(unsigned long long *){addr})'
        return f'(*(data_u32 *){addr})'

    n = 0
    overrides = {}
    if os.path.exists(args.overrides):
        with open(args.overrides) as f:
            overrides = json.load(f)
    for a in sorted(info, key=lambda x: int(x, 16)):
        d = info[a]
        in_text = int(a, 16) < CODE_END
        ov = overrides.get(a)
        if ov:
            d['kind'] = ov
            d['size'] = {'u8': 1, 'u16': 2, 'u32': 4, 'u64': 8,
                         'f32': 4, 'f64': 8, 'ptr': 4}.get(ov, 4)
        bare = amp = bare_assign = False
        for s in d['syms']:
            e = re.escape(s)
            if re.search(r'(?<![\w&])' + e + r'\s*\[', text):
                bare = True
            if re.search(r'\(\s*&\s*' + e + r'\s*\)\s*\[', text):
                amp = True
            if re.search(r'\b' + e + r'\s*=(?!=)', text):
                bare_assign = True
        d['idx_use'] = bare or amp
        # pointer-variable evidence: a real dereference (element-0 rewrites
        # already removed the array-shaped ones), a null compare, a cast of the
        # symbol to a pointer, assignment into pointer-shaped variables, or an
        # assignment FROM an address / pointer cast / other data symbol.
        deref = any(re.search(r'\*' + re.escape(s) + r'\b(?!\s*\)\s*\()', text)
                    for s in d['syms'])
        nullcmp = any(re.search(re.escape(s) + r'\s*==\s*\([^()]*\*\s*\)\s*0x?0\b', text)
                      for s in d['syms'])
        cast_ptr = any(re.search(r'=\s*\([^()]*\*\s*\)\s*' + re.escape(s) + r'\b', text)
                       for s in d['syms'])
        to_ptrvar = any(re.search(r'\b(?:pi|ppi|pu|ppu|pc|ppc|ps|pf|pb|pv|pd|ppd|pl|ppb)\w*\s*=\s*'
                                  + re.escape(s) + r'\s*;', text) for s in d['syms'])
        rhs_ptr = False
        for s in d['syms']:
            e = re.escape(s)
            for m in re.finditer(r'\b' + e + r'\s*=\s*(?!=)\s*([^;]{1,60})', text):
                r = m.group(1).lstrip()
                if re.search(r'\(f(?:loat|double\)|[0-9]+\.[0-9])', r):
                    continue  # RHS is a float expression, not a pointer
                if (r.startswith('&')
                        or re.match(r'\([^)]*\*[^)]*\)', r)
                        or re.search(r'\b(?!' + e + r'\b)(?:DAT_[0-9A-Fa-f]{8}|PTR_[A-Za-z0-9_]+)', r)):
                    rhs_ptr = True
                    break
        # float evidence: stored from or used with float-literal arithmetic
        float_hint = False
        if not (deref or bare or amp):
            for s in d['syms']:
                e = re.escape(s)
                if (re.search(r'\b' + e + r'\s*=\s*[^\n;]{0,60}(?:\(f(?:loat|double)\)|[0-9]+\.[0-9])', text)
                        or re.search(r'\((?:float|double)\)\s*' + e + r'\b', text)
                        or re.search(r'[0-9]+\.[0-9]+\s*/\s*\(?\s*' + e + r'\b', text)
                        or re.search(e + r'\s*[/ *]\s*(?:\(\s*f\w+\s*\)\s*)?[0-9]+\.[0-9]', text)):
                    float_hint = True
        # scalar evidence: arithmetic on the symbol (multiply/shift/mask) or its
        # use as an array index.  Ghidra's pointer-var names (puVar etc.) lie on
        # type-confused sites; arithmetic does not.
        scalar = False
        for s in d['syms']:
            e = re.escape(s)
            if (re.search(r'\b' + e + r'\s*(?:\*|/{1,2}|%|>>|<<|&|\|)\s*(?:0x[0-9a-fA-F]+|[0-9]+)(?![.\w])', text)
                    or re.search(r'(?:0x[0-9a-fA-F]+|[0-9]+)(?![.\w])\s*\*\s*\b' + e + r'\b', text)
                    or re.search(r'\[\s*' + e + r'\s*\]', text)
                    or re.search(r'\b' + e + r'\s*[/%]\s*[A-Za-z_]', text)):
                scalar = True
        ptrvar = (deref or nullcmp or cast_ptr or to_ptrvar or rhs_ptr) and not (
            scalar and not (deref or bare))

        # VC6 sub-allocator arena-list sentinel (0x4ae8e0/0x4ae8e4): the two
        # image slots sit 4 bytes apart, so neither can be widened in place,
        # and the metadata layout needs 8-byte entries.  Head and tail live
        # in bblit_arena_sentinel[] instead; every site using &SLOT, bare
        # SLOT reads or SLOT writes keeps compiling because the macro stays
        # an lvalue of pointer shape (see port_init, which self-links it).
        if int(a, 16) in (0x4ae8e0, 0x4ae8e4):
            sidx = 0 if int(a, 16) == 0x4ae8e0 else 1
            for sym in sorted(d['syms']):
                out.append(f'#define {sym} (bblit_arena_sentinel[{sidx}]) /* {d["label"] or a} */')
            n += 1
            continue
        if a in TYPE_OVERRIDES:
            k2, z2 = TYPE_OVERRIDES[a]
            v = view_macro(a, z2, k2)
        elif d['called']:
            # (*SYM)(...) call sites: the symbol holds function pointers
            v = f'(*(code **)0x{a}ul)'
        elif float_hint and not (nullcmp or cast_ptr or to_ptrvar or rhs_ptr):
            v = view_macro(a, d['size'] if d['size'] in (4, 8) else 4, 'float')
        elif scalar and not (deref or bare):
            # arithmetic on the symbol: it is a counter/flag regardless of any
            # pointer evidence or declared pointer type
            v = view_macro(a, d['size'], d['kind'] if d['kind'] != 'ptr' else 'uint')
        elif ptrvar:
            # pointer variable: *SYM must yield the 4-byte stored value, and
            # SYM itself must be usable as a pointer
            v = f'(*(data_u32 **)0x{a}ul)'
        elif bare and bare_assign:
            # indexed and assigned: compile-first, element stride is reviewed
            # at runtime
            v = f'(*(data_u32 **)0x{a}ul)'
        elif bare:
            # SYM[i] on a pointer-typed symbol: u32 elements, 4-byte stride
            v = f'(*(data_u32 (*)[])0x{a}ul)'
        elif amp:
            # Ghidra (&SYM)[i] on a scalar-typed symbol: u32 element at +4i
            v = view_macro(a, 4, 'uint')
        elif in_text and not d['ptr']:
            # label at a code address used as data: treat as code entry
            v = f'((code)0x{a}ul)'
        elif d['ptr'] and d['kind'] == 'float':
            v = view_macro(a, d['size'], 'float')
        elif d['ptr'] or d['kind'] == 'ptr':
            # pointer variable: *SYM must yield the 4-byte stored value, and
            # SYM itself must be usable as a pointer
            v = f'(*(data_u32 **)0x{a}ul)'
        else:
            v = view_macro(a, d['size'], d['kind'])
        # LP64 side-table cell (see above): same lvalue shape, exact width.
        if a in conflict_addrs and int(a, 16) not in CELL_EXCLUDE and (
                v == f'(*(data_u32 **)0x{a}ul)' or v == f'(*(code **)0x{a}ul)'):
            v = f'(bblit_slot_cells[{cell_addrs.index(a)}])'
        for sym in sorted(d['syms']):
            out.append(f'#define {sym} {v} /* {d["label"] or a} */')
        n += 1

    # string buffers the game rewrites at runtime (seeded defaults, not literals)
    MUTABLE_STRINGS = {'004673f0'}

    # ---- string labels ----------------------------------------------------
    nstr = 0
    for line in open(args.symbols):
        p = line.rstrip('\n').split('|')
        if p[0] != 'DATA':
            continue
        lbl = p[4]
        if not lbl.startswith('s_'):
            continue
        lbl_id = re.sub(r'[^A-Za-z0-9_]', '_', lbl)
        # One address can be referenced under two spellings: the rename map
        # may give it a semantic name (0045f278 -> STR_MSGBOX_TITLE) while the
        # decompile still spells some sites with the raw Ghidra label
        # (s_Bugs_Bunny_0045f278).  Both names must be defined or the sites
        # that kept the raw spelling fail to compile, so emit each spelling
        # that actually appears in the source.
        names = []
        renamed = renames.get(p[1])
        if renamed and renamed in text:
            names.append(renamed)
        if lbl_id in text:
            names.append(lbl_id)
        if not names:
            continue
        addr, size = p[1], max(int(p[2]), 1)
        const = '' if addr in MUTABLE_STRINGS else 'const '
        for nm in names:
            out.append(f'#define {nm} (*({const}char (*)[{size}])0x{addr}ul)')
            nstr += 1

    with open(args.out, 'w') as f:
        f.write('/* data_syms.h -- generated by tools/gen_headers.py. Do not edit. */\n'
                '#ifndef BBLIT_DATA_SYMS_H\n#define BBLIT_DATA_SYMS_H\n\n'
                '#include "ghidra_types.h"\n\n#ifdef __cplusplus\nextern "C" {\n#endif\n\n'
                + '\n'.join(out)
                + '\n\n#ifdef __cplusplus\n}\n#endif\n\n#endif\n')

    # ---- emit data_syms.c -------------------------------------------------
    body = []
    body.append('/* data_syms.c -- generated by tools/gen_headers.py. Do not edit.')
    body.append(' * Original PE image bytes outside .text, mapped back at their')
    body.append(' * original addresses by bblit_port_init(). */')
    body.append('#include "data_syms.h"')
    body.append('#include "bblit_game.h"')
    body.append('')
    body.append('#include <stdint.h>')
    body.append('#include <string.h>')
    body.append('#include <stdlib.h>')
    body.append('#include <sys/mman.h>')
    body.append('#include <unistd.h>')
    body.append('')
    body.append(f'data_u64 bblit_image_blob[{words}] __attribute__((aligned(4096))) = {{')
    nword = 0
    for i in range(words):
        vals = [bytemap.get(BLOB_BASE + i * 8 + j, 0) for j in range(8)]
        if not any(vals):
            continue
        w = 0
        for j in range(8):
            w |= vals[j] << (8 * j)
        body.append(f'[{i}] = 0x{w:016x}ull,')
        nword += 1
    body.append('};')
    body.append('')

    # trampolines: original function address -> compiled function
    hdr = open(args.gamehdr).read()
    body.append('static const struct { unsigned from; void *to; } bblit_thunks[] = {')
    nfn = 0
    for line in open(args.symbols):
        p = line.rstrip('\n').split('|')
        if p[0] != 'FUNC':
            continue
        addr, name = p[1], p[2]
        if not re.match(r'^FUN_[0-9A-Fa-f]{8}$', name):
            continue  # CRT/thunk chunks: no compiled body exists
        newname = renames.get(addr, name)
        if f'{newname}(' not in hdr:
            print(f'gen_headers: WARN trampoline target {newname} missing from {args.gamehdr}',
                  file=sys.stderr)
            continue
        body.append(f'{{ 0x{addr}, (void *)&{newname} }},')
        nfn += 1
    body.append('};')
    body.append('')

    # referenced pointer-typed slots to widen
    # never widen inside .text: those dwords are data-shaped labels on code
    # (jump-table heads etc), read-only and already executable.
    # slots/conflict sets are computed once above (shared with the header
    # emitter); conflict slots read side-table cells (see bblit_slot_cells).
    # (nskip counts every slot left unwidened: live-high-half conflicts
    # plus the arena sentinel pair.)
    body.append('static const unsigned bblit_ptr_slots[] = {')
    for a in slots:
        body.append(f'  0x{a},')
    body.append('};')
    body.append('')
    body.append('/* VC6 sub-allocator arena-list sentinel (replaces the image slots')
    body.append(' * 0x4ae8e0/0x4ae8e4, which cannot be widened in place).  next+0 and')
    body.append(' * prev+8 self-link; the rest stays zero so the arena walks in')
    body.append(' * FUN_004522b0/452380/452440 skip the sentinel. */')
    body.append('void *bblit_arena_sentinel[6] = {0};')
    body.append('')
    body.append('/* LP64 side-table cells for conflict slots (see above).  Cell i')
    body.append(' * mirrors the image dword for its address, zero-extended; reads')
    body.append(' * and writes go through the cell at full 64-bit width so neither')
    body.append(' * the neighbour nor a high function address is ever clobbered. */')
    body.append(f'data_u32 *bblit_slot_cells[{len(cell_addrs)}] = {{0}};')
    body.append('')

    body.append('void bblit_port_init(void);')
    body.append('void bblit_port_init(void)')
    body.append('{')
    body.append('    void *p = mmap((void *)BBLIT_BLOB_BASE, BBLIT_BLOB_END - BBLIT_BLOB_BASE,')
    body.append('                   PROT_READ | PROT_WRITE,')
    body.append('                   MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);')
    body.append('    if (p != (void *)BBLIT_BLOB_BASE) {')
    body.append('        write(2, "bblit: cannot map image at 0x400000\\n", 34);')
    body.append('        abort();')
    body.append('    }')
    body.append('    memcpy((void *)BBLIT_BLOB_BASE, bblit_image_blob, sizeof bblit_image_blob);')
    body.append('    for (unsigned i = 0; i < sizeof bblit_thunks / sizeof bblit_thunks[0]; i++) {')
    body.append('        volatile unsigned char *t =')
    body.append('            (volatile unsigned char *)(uintptr_t)bblit_thunks[i].from;')
    body.append('        t[0] = 0x48; t[1] = 0xb8;                    /* movabs $to, %rax */')
    body.append('        *(void *volatile *)(uintptr_t)&t[2] = bblit_thunks[i].to;')
    body.append('        t[10] = 0xff; t[11] = 0xe0;                  /* jmp *%rax */')
    body.append('    }')
    body.append('    mprotect((void *)BBLIT_BLOB_BASE, BBLIT_CODE_END - BBLIT_BLOB_BASE,')
    body.append('             PROT_READ | PROT_EXEC);')
    body.append('    /* side-table cells snapshot the pristine image dwords FIRST:')
    body.append('     * a skipped slot neighboring a widened one (e.g. 0x4ac310')
    body.append('     * next to the widened 0x4ac30c) has its image bytes clobbered')
    body.append('     * by the widening write (measured: the Cyrix needle cell')
    body.append('     * snapshotted NULL). */')
    for i, a in enumerate(cell_addrs):
        body.append(f'    bblit_slot_cells[{i}] = (data_u32 *)(uintptr_t)*(volatile data_u32 *)0x{a}ul;')
    body.append('    /* Two phases: slots sit 4 bytes apart, so an in-place 8-byte')
    body.append('     * widening write clobbers the next slot\'s original value')
    body.append('     * (measured: 0x4ac30c\'s write zeroed 0x4ac310, whose needle')
    body.append('     * read back NULL and crashed strstr in sysinfo_cpu_detect).')
    body.append('     * Snapshot every original dword first, then write back. */')
    body.append('    static data_u32 widen_snap[sizeof bblit_ptr_slots / sizeof bblit_ptr_slots[0]];')
    body.append('    for (unsigned i = 0; i < sizeof bblit_ptr_slots / sizeof bblit_ptr_slots[0]; i++)')
    body.append('        widen_snap[i] = *(volatile data_u32 *)(uintptr_t)bblit_ptr_slots[i];')
    body.append('    for (unsigned i = 0; i < sizeof bblit_ptr_slots / sizeof bblit_ptr_slots[0]; i++) {')
    body.append('        uintptr_t a = bblit_ptr_slots[i];')
    body.append('        /* original 4-byte slot value zero-extended into an 8-byte pointer */')
    body.append('        *(void *volatile *)a =')
    body.append('            (void *)(uintptr_t)widen_snap[i];')
    body.append('    }')
    body.append('    /* arena-list sentinel starts self-linked, like the image did. */')
    body.append('    bblit_arena_sentinel[0] = bblit_arena_sentinel[1] = (void *)&bblit_arena_sentinel[0];')
    body.append('}')

    with open(args.body_out, 'w') as f:
        f.write('\n'.join(body) + '\n')

    print(f'{n} data addresses declared, {nstr} string labels, '
          f'{nfn} trampolines, {len(slots)} pointer slots ({nskip} skipped), {len(cell_addrs)} side-table cells, {nword} non-zero blob words')

if __name__ == '__main__':
    main()
