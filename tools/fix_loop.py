#!/usr/bin/env python3
"""fix_loop.py -- drive gcc over linux/bblit_game.c and apply mechanical fixes
until the error count stops falling.

Pipeline per run:
  1. reconcile linux/bblit_game.h against the definitions in bblit_game.c
     (the old header used unprototyped `long f();` guesses; regenerated
     prototypes come verbatim from the definitions)
  2. one-shot text rewrites: `thunk_FUN_x(...)` call -> `FUN_x(`,
     invalid `X.DATAPART(a,b,c)` member-macro form, `(*)(...)` casts
  3. parse gcc diagnostics fully (primary + note chain), resolve the true
     bblit_game.c site for errors reported inside data_syms.h macros
  4. apply fix classes:
       A. void fn whose value is used          -> retype to long (+ bare returns)
       B. int<-ptr / ptr<-int on assign & args -> (uintptr_t)/(void*)(uintptr_t)
       C. incompatible pointer assign/arg      -> (void *)(uintptr_t)(rhs)
       D. bad float/double/longdouble casts of pointer slots -> u32_as_f/d/ld
       E. float assigned to a data_u32* slot   -> (void *)(uintptr_t)flt_bitsf((float)(rhs))
       F. arity mismatch on a call             -> ((long (*)())FUN)(args)
  5. loop until no fix applies; print per-class census of what remains

Generated files only: bblit_game.c / bblit_game.h.  Hand-fixes belong in
triage.txt follow-up, not here.
"""
import re, subprocess, sys, collections

GAME = 'linux/bblit_game.c'
HDR  = 'linux/bblit_game.h'

DIAG = re.compile(r'^(\S+?):(\d+):(\d+):\s+(error|note|warning):\s*(.*)$')

# ---------------------------------------------------------------- gcc driver
def run_gcc():
    r = subprocess.run(['gcc','-c','-std=gnu17','-Ilinux','-fno-diagnostics-show-caret',
                        GAME,'-o','/tmp/bblit_game.o'],
                       capture_output=True, text=True)
    return parse(r.stderr)

def count_errors(diags):
    """errors only.  Warnings are counted separately: -Wint-to-pointer-cast
    alone is thousands and is by design of the address-identity port."""
    return [d for d in diags if d['kind'] == 'error']

def parse(text):
    diags, cur = [], None
    for line in text.split('\n'):
        m = DIAG.match(line)
        if not m:
            continue
        f, ln, col, kind, msg = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)
        if kind == 'note':
            if cur is not None:
                cur['notes'].append((f, ln, col, msg))
        else:
            if cur is not None:
                diags.append(cur)
            cur = {'file': f, 'ln': ln, 'col': col, 'msg': msg, 'kind': kind, 'notes': []}
    if cur is not None:
        diags.append(cur)
    return diags

def site_of(d):
    """True (line, col) in bblit_game.c for a diagnostic."""
    if d['file'].endswith('bblit_game.c'):
        return (d['ln'], d['col'])
    for f, ln, col, msg in d['notes']:
        if f.endswith('bblit_game.c') and msg.startswith('in expansion of macro'):
            return (ln, col)
    return None

# ------------------------------------------------------------ line utilities
def ident_span(l, col):
    """(start,end) of identifier containing 0-based col, else None."""
    for m in re.finditer(r'[A-Za-z_][A-Za-z0-9_]*', l):
        if m.start() <= col < m.end():
            return m.start(), m.end()
    return None

def depth_scan(l):
    d = 0
    out = []
    for ch in l:
        if ch in '([': d += 1
        elif ch in ')]': d -= 1
        out.append(d)
    return out

def find_top_assign(l, after):
    """span (rhs_start, rhs_end) of the top-level `=` RHS after index `after`."""
    depth = depth_scan(l)
    i = after
    while i < len(l):
        if depth[i] == 0 and l[i] == '=' and (i+1 >= len(l) or l[i+1] != '=') \
           and (i == 0 or l[i-1] not in '=!<>=+-*/%&|^'):
            j = i + 1
            while j < len(l) and l[j] in ' \t': j += 1
            k = len(l)
            while k > j and (l[k-1] in ' \t;'): k -= 1
            if k > j:
                return j, k
        i += 1
    return None

def call_args_span(l, name, near):
    """per-argument (start,end) spans of the `name(...)` call best matching
    index `near`, or None."""
    best = None
    for m in re.finditer(r'\b' + re.escape(name) + r'\s*\(', l):
        i = m.end() - 1
        d = 0
        while i < len(l):
            if l[i] == '(': d += 1
            elif l[i] == ')':
                d -= 1
                if d == 0:
                    break
            i += 1
        if i >= len(l):
            continue
        if best is None or (m.end() <= near <= i):
            best = (m.start(), m.end(), i)
            if m.end() <= near <= i:
                break
    if best is None:
        return None
    name_start, a, b = best
    depth = depth_scan(l)
    # depth_scan records the count AFTER each char, so an argument separator
    # sits at the same value recorded at the call's own '('; hardcoding 1
    # collapsed nested calls into one argument and wrapped whole arglists
    lvl = depth[a - 1]  # depth at the call's own '('
    args, cur = [], a
    i = a
    while i < b:
        if depth[i] == lvl and l[i] == ',':
            args.append((cur, i))
            cur = i + 1
        i += 1
    args.append((cur, b))
    return args, name_start, b

def wrap_span(l, s, e, prefix, suffix):
    return l[:s] + prefix + l[s:e] + suffix + l[e:]

# --------------------------------------------------------------- fix classes
def fix_void_value_used(diags, lines, protolines):
    names = set()
    for d in diags:
        if 'void value not ignored' in d['msg'] or 'invalid use of void expression' in d['msg']:
            st = site_of(d)
            if not st: continue
            ln, col = st
            for j in range(ln-1, max(-1, ln-7), -1):
                m = re.search(r'\b(FUN_[0-9A-Fa-f]{8}|\w+)\s*\(', lines[j])
                if m:
                    names.add(m.group(1))
                    break
    changed = 0
    for name in names:
        done = False
        for i, l in enumerate(lines):
            m = re.match(r'(\s*)void(\s+' + re.escape(name) + r'\s*\()', l)
            if m:
                lines[i] = m.group(1) + 'long' + m.group(2) + l[m.end():]
                changed += 1
                done = True
                break
        for i, l in enumerate(protolines):
            m = re.match(r'(\s*)void(\s+' + re.escape(name) + r'\s*\()', l)
            if m:
                protolines[i] = m.group(1) + 'long' + m.group(2) + l[m.end():]
                changed += 1
                break
        if done:
            start = next((i for i, l in enumerate(lines)
                          if re.match(r'\s*long\s+' + re.escape(name) + r'\s*\(', l)), None)
            if start is not None:
                depth, seen = 0, False
                for j in range(start, len(lines)):
                    depth += lines[j].count('{') - lines[j].count('}')
                    if '{' in lines[j]: seen = True
                    if seen and depth <= 0: break
                    lines[j] = re.sub(r'^(\s*)return;\s*$', r'\1return 0;', lines[j])
    return changed

# ---------------------------------------------------------- type-aware engine
PROMOTABLE = {'char','short','int8','int16','sbyte','byte','ubyte','uchar','ushort',
              'uint16','undefined1','undefined2','bool','float'}

def type_kind(t):
    """kind of a type string: ptr, int, float, other."""
    t = t.strip()
    if '*' in t or t.startswith(('LP', 'PCTSTR', 'LPC', 'HANDLE', 'HWND', 'HDC', 'HGLRC',
                                 'HMODULE', 'HINSTANCE', 'HGLOBAL', 'HRSRC', 'HGDIOBJ',
                                 'HBITMAP', 'HCURSOR', 'HBRUSH', 'HPALETTE', 'HFONT',
                                 'HACCEL', 'HHOOK', 'HKEY', 'HDWP', 'HIMC', 'HKL', 'code')):
        return 'ptr'
    if t in ('float','double','longdouble','float2','double2') or 'float' in t or 'double' in t:
        return 'float'
    return 'int'

def cast_for(dst_t, src_t):
    """C wrapper pair converting src-shaped expr into dst type.
    Bit-preserving for the dword/pointer overlap the x86 code relied on."""
    dk, sk = type_kind(dst_t), type_kind(src_t)
    dst_t = dst_t.strip()
    if dk == 'ptr' and sk == 'ptr':
        return '(' + dst_t + ')(', ')'
    if dk == 'ptr' and sk == 'int':
        return '(' + dst_t + ')(uintptr_t)(', ')'
    if dk == 'int' and sk == 'ptr':
        return '(uintptr_t)(', ')'
    if dk == 'float' and sk == 'ptr':
        base = 'u32_as_ld' if 'longdouble' in dst_t else ('u32_as_d' if 'double' in dst_t else 'u32_as_f')
        return base + '((uintptr_t)(', '))'
    if dk == 'ptr' and sk == 'float':
        if 'double' in src_t:
            return '(' + dst_t + ')(uintptr_t)flt_bitsd((double)(', '))'
        return '(' + dst_t + ')(uintptr_t)flt_bitsf((float)(', '))'
    if dk == 'int' and sk == 'float':
        # original stored float bits into a dword slot view
        return '(int)(uintptr_t)flt_bitsf((float)(', '))'
    if dk == 'float' and sk == 'int':
        base = 'u32_as_ld' if 'longdouble' in dst_t else ('u32_as_d' if 'double' in dst_t else 'u32_as_f')
        return base + '((uintptr_t)(', '))'
    return None

def note_expected(d):
    """(expected_type, actual_type) from the gcc expected/actual note."""
    for f, ln, col, msg in d['notes']:
        m = re.search(r"expected '([^']+)' but argument is of type '([^']+)'", msg)
        if m:
            return m.group(1), m.group(2)
    return None, None

def assign_types(d):
    """(dst_type, src_type) from an assignment-to message."""
    m = re.search(r"assign(?:ment|ing) to (?:type )?'([^']+)'", d['msg'])
    if not m:
        return None, None
    dst = m.group(1)
    m2 = re.search(r"from (?:type |incompatible pointer type )?'([^']+)'", d['msg'])
    if not m2:
        m2 = re.search(r"from type '([^']+)'", d['msg'])
    src = m2.group(1) if m2 else None
    return dst, src

def fix_conversions(diags, lines):
    changed = 0
    for d in diags:
        msg = d['msg']
        st = site_of(d)
        if not st: continue
        ln, col = st
        if not (1 <= ln <= len(lines)): continue
        l = lines[ln-1]
        m = re.search(r"passing argument (\d+) to '([^']+)'", msg) or \
            re.search(r"passing argument (\d+) of '([^']+)'", msg)
        if m and ('makes integer from pointer' in msg or 'makes pointer from integer' in msg
                  or 'incompatible pointer type' in msg or 'from type' in msg
                  or 'discards qualifiers' in msg):
            name = m.group(2)
            got = call_args_span(l, name, col-1)
            if not got:
                continue
            args, a_open, a_close = got
            if not (a_open <= col-1 <= a_close):
                continue
            def is_wrapped(t):
                t = t.strip()
                return t.startswith('(uintptr_t)') or t.startswith('(void *)')
            # prefer the message's argument index; fall back to the argument
            # containing the reported column; never re-wrap an already-wrapped
            # argument (gcc reports these errors once per call, and repeated
            # passes would stack casts on argument 1)
            n = int(m.group(1))
            idx = (n - 1) if (n - 1) < len(args) else None
            if idx is not None and is_wrapped(l[args[idx][0]:args[idx][1]]):
                idx = None
            if idx is None:
                for i, (s_, e_) in enumerate(args):
                    if s_ <= col-1 < e_ and not is_wrapped(l[s_:e_]):
                        idx = i
                        break
            if idx is None:
                for i, (s_, e_) in enumerate(args):
                    if not is_wrapped(l[s_:e_]):
                        idx = i
                        break
            if idx is None:
                continue
            s, e = args[idx]
            s2 = s
            while s2 < e and l[s2] in ' \t': s2 += 1
            e2 = e
            while e2 > s2 and l[e2-1] in ' \t': e2 -= 1
            if e2 <= s2: continue
            et, at = note_expected(d)
            if et:
                pair = cast_for(et, at or "int")
            else:
                pair = ('(uintptr_t)(', ')') if 'makes integer from pointer' in msg \
                    else ('(void *)(uintptr_t)(', ')')
            if not pair: continue
            if ',' in l[s2:e2]:
                # comma expression: bind it before casting
                prefix, suffix = pair[0] + '(', ')' + pair[1]
                new = l[:s2] + prefix + '(' + l[s2:e2] + ')' + suffix + l[e2:]
            else:
                prefix, suffix = pair
                new = wrap_span(l, s2, e2, prefix, suffix)
            if new != l:
                lines[ln-1] = new
                changed += 1
            continue
        if 'assignment to' in msg or 'when assigning to type' in msg:
            dst, src = assign_types(d)
            # the error column sits at/near the offending '='; prefer the '='
            # nearest the column (for/while headers hold several)
            depth = depth_scan(l)
            cands = [i for i, c in enumerate(l)
                     if c == '=' and (i+1 >= len(l) or l[i+1] != '=')
                     and (i == 0 or l[i-1] not in '=!<>=+-*/%&|^')]
            if not cands:
                continue
            best = min(cands, key=lambda i: abs(i - (col-1)))
            j = best + 1
            while j < len(l) and l[j] in ' \t': j += 1
            # The RHS ends at the first ',' / ';' at the '=' 's own paren
            # depth, or at a ')' that closes a paren opened BEFORE the RHS
            # (the enclosing if-header).  A ')' that closes something opened
            # inside the RHS -- a cast, a call -- is part of the expression:
            # `x = (T)(y);` must not stop after `(T)`.
            q = j
            c = 0
            while q < len(l):
                ch = l[q]
                if ch == '(':
                    c += 1
                elif ch == ')':
                    if c == 0:
                        break
                    c -= 1
                elif c == 0 and ch in ',;':
                    break
                q += 1
            if q >= len(l):
                k = len(l)
                while k > j and l[k-1] in ' \t;': k -= 1
            else:
                k = q
            while k > j and l[k-1] in ' \t': k -= 1
            s, e = j, k
            if k <= j: continue
            if dst and src:
                pair = cast_for(dst, src)
            else:
                pair = ('(uintptr_t)(', ')') if 'makes integer from pointer' in msg \
                    else ('(void *)(uintptr_t)(', ')')
            if not pair: continue
            new = wrap_span(l, s, e, pair[0], pair[1])
            if new != l:
                lines[ln-1] = new
                changed += 1
    return changed

CAST_HELPERS = {'float':'u32_as_f', 'double':'u32_as_d', 'longdouble':'u32_as_ld'}
BADCAST = re.compile(r'\(\s*(float|double|longdouble)\s*\)\s*(\*?\s*[A-Za-z_][A-Za-z0-9_]*(\[[^\]]*\])?)')

def wrap_designator(l, name, col, rettype='long'):
    """name( -> ((rettype (*)())name)( at the occurrence nearest col.
    Works across multi-line arglists since only the designator changes."""
    best = None
    for m in re.finditer(r'(?<![\w.(])' + re.escape(name) + r'\s*\(', l):
        if best is None or abs(m.start() - (col-1)) < abs(best.start() - (col-1)):
            best = m
    if best is None:
        return None
    s = best.start()
    e = s + len(name)
    return l[:s] + '((%s (*)())%s)' % (rettype, name) + l[e:]

def fix_calls(diags, lines):
    changed = 0
    for d in diags:
        msg = d['msg']
        if not ('too few arguments' in msg or 'too many arguments' in msg
                or 'non-compatible type' in msg):
            continue
        m = re.search(r"to function '([^']+)'", msg) or re.search(r"of '([^']+)'", msg)
        if not m: continue
        name = m.group(1)
        if name.startswith(('expected', 'argument')): continue
        st = site_of(d)
        if not st: continue
        ln, col = st
        if not (1 <= ln <= len(lines)): continue
        l = lines[ln-1]
        new = wrap_designator(l, name, col)
        if new and new != l:
            lines[ln-1] = new
            changed += 1
    return changed

def operand_span(l, op_idx, side):
    """span of the operand left or right of the operator at op_idx."""
    n = len(l)
    if side > 0:
        i = op_idx + 1
        while i < n and l[i] in ' \t': i += 1
        if i >= n: return None
        start = i
        depth = 0
        end = i
        while i < n:
            c = l[i]
            if c in '([': depth += 1
            elif c in ')]':
                if depth == 0: break
                depth -= 1
            elif depth == 0 and c in '+-*/%<>=&|^,;?!':
                if i > start:
                    break
                # possible unary sign: consume it and continue
                if c in '+-' and i+1 < n and l[i+1] not in ' \t':
                    i += 1
                    end = i
                    continue
                break
            i += 1
            end = i
        while end > start and l[end-1] in ' \t': end -= 1
        return (start, end) if end > start else None
    else:
        i = op_idx - 1
        while i >= 0 and l[i] in ' \t': i -= 1
        if i < 0: return None
        end = i + 1
        depth = 0
        while i >= 0:
            c = l[i]
            if c in ')]': depth += 1
            elif c in '([':
                if depth == 0: break
                depth -= 1
            elif depth == 0 and c in '+-*/%<>=&|^,;?:!':
                break
            i -= 1
        start = i + 1
        # also stop at a top-level comma: a comma expression is not one operand
        j = end - 1
        d3 = depth_scan(l)
        while j >= start:
            if l[j] == ',' and d3[j] == d3[op_idx]:
                start = j + 1
                break
            j -= 1
        while start < end and l[start] in ' \t': start += 1
        return (start, end) if end > start else None

MACRO_DEF = re.compile(r'#define\s+(\w+)\s+\(\s*\*\s*\(\s*([A-Za-z_][A-Za-z0-9_ \t]*\**)\s*\*\s*\)\s*(0x[0-9A-Fa-f]+)')

def load_macro_types(hdrs=('linux/data_syms.h',)):
    mt = {}
    for h in hdrs:
        try:
            txt = open(h).read()
        except FileNotFoundError:
            continue
        for m in MACRO_DEF.finditer(txt):
            mt[m.group(1)] = m.group(2).replace(' ', '')
    return mt

def fix_binary_ops(diags, lines):
    changed = 0
    for d in diags:
        msg = d['msg']
        if 'invalid operands to binary' not in msg and 'wrong type argument to unary minus' not in msg:
            continue
        st = site_of(d)
        if not st: continue
        ln, col = st
        if not (1 <= ln <= len(lines)): continue
        l = lines[ln-1]
        p = min(len(l)-1, max(0, col-1))
        if 'unary minus' in msg:
            while p < len(l) and l[p] != '-':
                p += 1
            if p >= len(l): continue
            sp = operand_span(l, p, +1)
            if not sp: continue
            new = wrap_span(l, sp[0], sp[1], '(int)(uintptr_t)(', ')')
            if new != l:
                lines[ln-1] = new
                changed += 1
            continue
        if l[p] not in '+-*/%<>&|^':
            # scan nearby for the operator
            for q in range(max(0,p-3), min(len(l), p+4)):
                if l[q] in '+-*/%<>&|^':
                    p = q
                    break
            else:
                continue
        # types appear as (have 'A' {aka ...} and 'B' {aka ...}); the aka
        # text between them defeats a single regex, so take the two pieces
        # separately
        ma = re.search(r"\(\s*have\s+'([^']+)'", msg)
        mb = re.search(r"\s+and\s+'([^']+)'", msg)
        types = [ma.group(1), mb.group(1)] if (ma and mb) else []
        if len(types) < 2:
            continue
        if '*' in types[0]:
            side = -1
        elif '*' in types[1]:
            side = +1
        else:
            continue
        other = types[1] if side < 0 else types[0]
        sp = operand_span(l, p, side)
        if not sp: continue
        operand = l[sp[0]:sp[1]]
        mt = load_macro_types()
        is_float_slot = False
        for name in re.findall(r'[A-Za-z_][A-Za-z0-9_]*', operand):
            if name in mt and 'float' in mt[name]:
                is_float_slot = True
        if is_float_slot:
            pre, post = 'u32_as_f((uintptr_t)(', '))'
        elif type_kind(other) == 'float':
            base = 'u32_as_ld' if 'longdouble' in other else ('u32_as_d' if 'double' in other else 'u32_as_f')
            pre, post = base + '((uintptr_t)(', '))'
        else:
            pre, post = '(uintptr_t)(', ')'
        new = wrap_span(l, sp[0], sp[1], pre, post)
        if new != l:
            lines[ln-1] = new
            changed += 1
    return changed

def fix_misc(diags, lines):
    """switch on non-integer; bad float casts of pointer slots."""
    changed = 0
    for d in diags:
        msg = d['msg']
        st = site_of(d)
        if not st: continue
        ln, col = st
        if not (1 <= ln <= len(lines)): continue
        l = lines[ln-1]
        if 'switch quantity not an integer' in msg:
            i = l.find('switch(')
            if i < 0: i = l.find('switch (')
            if i >= 0:
                j = l.find('(', i)
                k = l.rfind(')')
                if j >= 0 and k > j:
                    inner = l[j+1:k].strip()
                    new = l[:j+1] + '(uintptr_t)(' + inner + ')' + l[k:]
                    if new != l:
                        lines[ln-1] = new
                        changed += 1
            continue
        if 'pointer value used where a floating-point was expected' in msg:
            for m2 in BADCAST.finditer(l):
                if m2.start() <= col-1 <= m2.end() or abs(m2.start()-(col-1)) < 40:
                    helper = CAST_HELPERS[m2.group(1)]
                    operand = m2.group(2).replace(' *', '*').replace('* ', '*')
                    new = l[:m2.start()] + helper + '((uintptr_t)(' + operand + '))' + l[m2.end():]
                    if new != l:
                        lines[ln-1] = new
                        changed += 1
                    break
            continue
    return changed

def join_statements(lines):
    """Merge continuation lines so every statement is one physical line.
    A line is continued when it does not end a statement: decompiled C
    statements always end in one of ; { } or a case label's colon.  Any other
    ending (an operator, comma, open paren) means the statement continues on
    the next line.  Skips preprocessor lines.  Returns (lines, removed)."""
    out = []
    removed = 0
    def ends_statement(t):
        t = t.rstrip()
        if not t:
            return True
        if (t.endswith((';', '{', '}', ':', '*/')) or t.startswith('#')
                or t.startswith('//')):
            return True
        return False
    for l in lines:
        if out and not ends_statement(out[-1]):
            out[-1] = out[-1].rstrip() + ' ' + l.strip()
            removed += 1
        else:
            out.append(l)
    return out, removed

# ------------------------------------------------------- one-shot rewrites
# hand fixes for sites the mechanical passes cannot type-resolve.  Each entry
# is applied verbatim to the freshly generated text on every run.
HAND_FIXES = [
    # DAT_009ca724 is a 64-bit module handle (dlopen result); float sites
    # read it as fild/qword - cast through uintptr_t
    ("(float)DAT_009ca724", "(float)(uintptr_t)DAT_009ca724"),
    # DAT_004b0ca8 typed raw byte array: use sites are scalar byte reads
    ("*puVar2 = DAT_004b0ca8;", "*puVar2 = (uintptr_t)*DAT_004b0ca8;"),
    ("*puVar4 = DAT_004b0ca8;", "*puVar4 = (uintptr_t)*DAT_004b0ca8;"),
    ("(DAT_004b0ca8 == (char)local_18c)", "(*DAT_004b0ca8 == (char)local_18c)"),
    ("*pcVar12 = DAT_004b0ca8;", "*pcVar12 = *DAT_004b0ca8;"),
    ("(bVar10 == DAT_004b0ca8)", "(bVar10 == *DAT_004b0ca8)"),
    ("(bVar10 != DAT_004b0ca8)", "(bVar10 != *DAT_004b0ca8)"),
    # FUN_00408f70: generator emitted in_ECX twice (void* artifact + size_t)
    ("  char *pcVar2;\n  size_t in_ECX;\n  uint uVar3;",
     "  char *pcVar2;\n  uint uVar3;"),
    ("  void *in_ECX;", "  size_t in_ECX;"),
    # FUN_0040e050/FUN_0040e790: the GL import table at 0x468750 is 8-byte
    # entries (dest u32, name u32); the decompile read it as 16-byte
    # (ptr,ptr) pairs.  Rewrite the loop body to stride 8 via u32 reads.
    ("""    ppuVar7 = (undefined **)(&PTR_DAT_00468750);
    puVar6 = puVar4 + -4;
    do {
      *(undefined **)(puVar6 + -4) = ppuVar7[1];
      puVar5 = puVar6 + -8;
      *(HMODULE *)(puVar6 + -8) = (HMODULE)(uintptr_t)(DAT_009ca724);
      *(undefined4 *)(puVar6 + -0xc) = 0x40e0ae;
      pFVar2 = GetProcAddress(*(HMODULE *)(puVar6 + -8),*(LPCSTR *)(puVar6 + -4));
      puVar8 = (undefined4 *)*ppuVar7;
      ppuVar7 = ppuVar7 + 2;
      *puVar8 = (uintptr_t)pFVar2;
      puVar6 = puVar6 + -8;
    } while (ppuVar7 <= (undefined **)((int)&PTR_s_wglUseFontOutlinesW_0046928c + 3));""",
     """    {
      const data_u32 *glent = (const data_u32 *)&PTR_DAT_00468750;
      do {
        data_u32 dest_slot = glent[0];
        const char *gname = (const char *)(uintptr_t)glent[1];
        FARPROC pfn = GetProcAddress((HMODULE)(uintptr_t)DAT_009ca724, gname);
        *(data_u32 *)(uintptr_t)dest_slot = (data_u32)(uintptr_t)pfn;
        glent += 2;
      } while (glent <= (const data_u32 *)((uintptr_t)&PTR_s_wglUseFontOutlinesW_0046928c + 3));
    }"""),
    ("""            ppuVar17 = (undefined **)(&PTR_DAT_00468750);
            puVar16 = puVar16 + -4;
            do {
              *(undefined **)(puVar16 + -4) = ppuVar17[1];
              puVar13 = puVar16 + -8;
              *(HMODULE *)(puVar16 + -8) = (HMODULE)(uintptr_t)(DAT_009ca724);
              *(undefined4 *)(puVar16 + -0xc) = 0x40eb46;
              pFVar9 = GetProcAddress(*(HMODULE *)(puVar16 + -8),*(LPCSTR *)(puVar16 + -4));
              puVar3 = (undefined4 *)*ppuVar17;
              ppuVar17 = ppuVar17 + 2;
              *puVar3 = (uintptr_t)pFVar9;
              puVar16 = puVar16 + -8;
            } while (ppuVar17 <= (undefined **)((int)&PTR_s_wglUseFontOutlinesW_0046928c + 3));""",
     """            {
              const data_u32 *glent = (const data_u32 *)&PTR_DAT_00468750;
              do {
                data_u32 dest_slot = glent[0];
                const char *gname = (const char *)(uintptr_t)glent[1];
                FARPROC pfn = GetProcAddress((HMODULE)(uintptr_t)DAT_009ca724, gname);
                *(data_u32 *)(uintptr_t)dest_slot = (data_u32)(uintptr_t)pfn;
                glent += 2;
              } while (glent <= (const data_u32 *)((uintptr_t)&PTR_s_wglUseFontOutlinesW_0046928c + 3));
            }"""),
    # code address stored into a pointer slot: DAT_0044fc00 is a code macro
    ("*(undefined **)(puVar6 + -0x38) = &DAT_0044fc00;",
     "*(undefined **)(puVar6 + -0x38) = (undefined *)DAT_0044fc00;"),
    # local_c holds float bits (stored via flt_bitsf above these sites)
    ("(float)local_c - -0.5", "u32_as_f((uintptr_t)local_c) - -0.5"),
    # viewport scales: Ghidra value-cast of the slot dword (fild)
    ("(float)DAT_00621610", "(float)(uintptr_t)DAT_00621610"),
    ("(float)DAT_00621618", "(float)(uintptr_t)DAT_00621618"),
    # slot holds a table pointer; byte-scaled decrement before fild
    ("(float)(DAT_00565fe8 + -1)", "(float)((uintptr_t)DAT_00565fe8 + -1)"),
    # FARPROC stored into dword slot
    ("*puVar8 = pFVar2;", "*puVar8 = (uintptr_t)pFVar2;"),
    ("*puVar3 = pFVar9;", "*puVar3 = (uintptr_t)pFVar9;"),
    # Ghidra `*local_28._4_4_`: deref of the pointer stored at local_28+4
    ("*DATAPART(local_28,4,4)", "*(uint *)(uintptr_t)DATAPART(local_28,4,4)"),
    # FUN_004259e0 takes a float arg; caller pushes the address dword
    ("FUN_004259e0(&DAT_004b4000);", "FUN_004259e0(u32_as_f((uintptr_t)&DAT_004b4000));"),
    # dword viewport math on pointer-typed slot lvalues (FUN_004318f0)
    ("DAT_004b38c4 = DAT_004b38c4 + ((DAT_004b38d0 - *(int *)(psVar3 + 0x2a)) - DAT_004b38c4 >> 1);",
     "DAT_004b38c4 = (data_u32 *)(uintptr_t)(DAT_004b38c4 + ((uintptr_t)DAT_004b38d0 - (uintptr_t)*(int *)(psVar3 + 0x2a) - DAT_004b38c4 >> 1));"),
    ("DAT_004b38c0 = DAT_004b38c0 - (DAT_004b38c0 - DAT_004b3984 >> 2);",
     "DAT_004b38c0 = (data_u32 *)(uintptr_t)((uintptr_t)DAT_004b38c0 - ((uintptr_t)DAT_004b38c0 - (uintptr_t)DAT_004b3984 >> 2));"),
    ("DAT_004b38c8 = DAT_004b38c8 - (DAT_004b38c8 - DAT_004b398c >> 2);",
     "DAT_004b38c8 = (data_u32 *)(uintptr_t)((uintptr_t)DAT_004b38c8 - ((uintptr_t)DAT_004b38c8 - (uintptr_t)DAT_004b398c >> 2));"),
    # FUN_00434b00-ish table walk: base pointer + byte-scaled index
    ("return DAT_004b3b70 + iVar2 * 0x58;",
     "return (int)((uintptr_t)DAT_004b3b70 + iVar2 * 0x58);"),
    # FUN_004565a0: 2^round(ST0) as an 8-byte int (fistp quadword); the
    # caller's uVar2 is dword but only the low dword is ever read
    ("Var2 = (int)(uintptr_t)flt_bitsf((float)(fscale((longdouble)1 + lVar1,ROUND(in_ST0))));",
     "Var2 = (longlong)fscale((longdouble)1 + lVar1,ROUND(in_ST0));"),
    ("        uVar2 = FUN_004565a0();",
     "        uVar2 = (undefined4)(uintptr_t)FUN_004565a0();"),
    # FUN_004565a0 returns a fistp quadword, not the 10-byte ST slot view
    ("unkbyte10 FUN_004565a0(void) {", "longlong FUN_004565a0(void) {"),
    ("  unkbyte10 Var2;\n  \n  lVar1 = (longdouble)f2xm1",
     "  longlong Var2;\n  \n  lVar1 = (longdouble)f2xm1"),
    # VC6 64-bit RT helpers: Ghidra models the 64/32 mixed calls with 4
    # args; the port helpers take (longlong, longlong)
    ("__alldiv(uVar8,0xac44,0)", "__alldiv((longlong)uVar8,(longlong)0xac44)"),
    # Ghidra used these hidden locals without declaring them
    ("void FUN_00424f40(double param_1,double param_2,double param_3) {",
     "void FUN_00424f40(double param_1,double param_2,double param_3) {\n  double _local_8;"),
    ("undefined4 FUN_0044a9a0(int param_1,ushort param_2,undefined4 param_3) {",
     "undefined4 FUN_0044a9a0(int param_1,ushort param_2,undefined4 param_3) {\n  uint _param_2;"),
    ("undefined4 FUN_00451332(int param_1,uint param_2,int param_3,uint param_4) {",
     "undefined4 FUN_00451332(int param_1,uint param_2,int param_3,uint param_4) {\n  undefined4 _local_4;"),
    # fstpt of ST(7) into a 10-byte frame slot (FUN_00451332)
    ("auStack_e = (undefined1  [10])in_ST7;",
     "__builtin_memcpy(auStack_e,&in_ST7,10);"),
    # data_u32** slot returned through an undefined* API
    ("return &DAT_004b20f8;", "return (undefined *)&DAT_004b20f8;"),
    # FUN_0044fca0 (VC6 sub-malloc): the decompile dropped every return value
    # (extraout artifacts).  Real behaviour: try the sub-allocator, else
    # HeapAlloc, and hand the block back to the caller.
    ("""long FUN_0044fca0(int param_1) {
  int iVar1;
  uint dwBytes;
  
  dwBytes = param_1 + 0xfU & 0xfffffff0;
  if ((dwBytes <= DAT_004b0904) && (iVar1 = (uintptr_t)(FUN_00452440(param_1 + 0xfU >> 4)), iVar1 != 0)) {
    return 0;
  }
  HeapAlloc((void *)(uintptr_t)(DAT_009caae4),0,dwBytes);
  return 0;
}""",
     """long FUN_0044fca0(int param_1) {
  long p;
  uint dwBytes;

  dwBytes = param_1 + 0xfU & 0xfffffff0;
  if (dwBytes <= DAT_004b0904) {
    p = (long)FUN_00452440(param_1 + 0xfU >> 4);
    if (p != 0) return p;
  }
  return (long)HeapAlloc((void *)(uintptr_t)(DAT_009caae4),0,dwBytes);
}"""),
    # FUN_0044fc30 wraps FUN_0044fc50 and must forward its block, not return 0
    ("""long FUN_0044fc30(undefined4 param_1) {
  FUN_0044fc50(param_1,DAT_004b1e70);
  return 0;
}""",
     """long FUN_0044fc30(undefined4 param_1) {
  return (long)FUN_0044fc50(param_1,DAT_004b1e70);
}"""),
    # FUN_0044fc50: same dropped-return decompile; when the sub-allocator is
    # disabled (param_2 == 0) the original falls through to HeapAlloc
    ("""      if (iVar1 != 0) {
        return iVar1;
      }
      if (param_2 == 0) {
        return 0;
      }
      iVar1 = FUN_00452080(param_1);""",
     """      if (iVar1 != 0) {
        return iVar1;
      }
      if (param_2 == 0) {
        return (long)HeapAlloc((void *)(uintptr_t)(DAT_009caae4),0,param_1);
      }
      iVar1 = FUN_00452080(param_1);"""),
]

def ptr_cast_pass(lines):
    """(int)<pointer> truncates a 64-bit pointer.  Ghidra decompiled a 32-bit
    PE where (int)ptr is lossless; on x86-64 it destroys the address (observed:
    FUN_00405950 crashed storing through a 32-bit-truncated stack pointer).
    Rewrite casts whose operand is a pointer-typed identifier, &sym, or
    &stackXXXXXX to (long)(uintptr_t).  (int)x[...] element casts and casts of
    known-integer variables are left alone."""
    # per-function pointer-typed locals: declarations whose declarator has '*'
    PTR_DECL = re.compile(r'^\s{2}[\w ]+?\*\s*\**\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*;')
    func = None
    ptrvars = {}
    for l in lines:
        m = re.match(r'// ===== (FUN_[0-9A-Fa-f]+)', l)
        if m:
            func = m.group(1)
            ptrvars[func] = set()
            continue
        if func is None:
            continue
        dm = PTR_DECL.match(l)
        if dm:
            ptrvars[func].add(dm.group(1))
    # one global pass over the joined text, tracking the enclosing function by
    # marker lines so the per-function var sets apply
    n = 0
    out = []
    func = None
    CAST = re.compile(r'\(int\)\s*(&?)\s*\b([A-Za-z_][A-Za-z0-9_]*)\b(?!\s*\[)')
    for l in lines:
        m = re.match(r'// ===== (FUN_[0-9A-Fa-f]+)', l)
        if m:
            func = m.group(1)
            out.append(l)
            continue
        pv = ptrvars.get(func, set())

        def rep(mm):
            nonlocal n
            amp, name = mm.group(1), mm.group(2)
            is_ptr = amp == '&' or name in pv or name.startswith('stack0x')
            if not is_ptr:
                return mm.group(0)
            n += 1
            return '(long)(uintptr_t)' + amp + name
        out.append(CAST.sub(rep, l))
    return out, n


def lp64_ptr_arith_pass(lines):
    """Ghidra decompiled a 32-bit PE: unsigned dword pointer offsets wrap
    modulo 2^32, so the idiom `ptr + -uVar` means `ptr - uVar` and
    `(uint)bVar * -2 + 1` is a +1/-1 direction stride.  Compiled at LP64 the
    same text adds 0xffffffff as a positive 64-bit offset and the pointer
    lands 4 GiB up (observed: winmain SIGSEGV at bblit_game.c:3581 with
    RAX=0x10045f370, the '\' string address plus 1<<32).  Fix by computing
    the offset in 32-bit and sign-extending: (long)(int)(...).
    Applies to the stride idiom, ptr + -uVar, ~uVar and N - uVar subscripts."""
    n = 0
    STRIDE = re.compile(r'\+\s*\(uint\)(bVar\d+)\s*\*\s*-(\d+)(\s*\+\s*(\d+))?')
    NEGOFF = re.compile(r'\b(p[A-Za-z]*Var\d+|param_\d+)\s*\+\s*-uVar(\d+)\b')
    out = []
    for l in lines:
        new = STRIDE.sub(
            lambda m: '+ (long)(int)((uint)%s * -%s%s)' % (
                m.group(1), m.group(2), ' + ' + m.group(4) if m.group(4) else ''),
            l)
        new = NEGOFF.sub(lambda m: '%s + (long)(int)(-uVar%s)' % (m.group(1), m.group(2)), new)
        new = re.sub(r'\[~uVar(\d+)\]', r'[(long)(int)(~uVar\1)]', new)
        new = re.sub(r'\[(\d+) - uVar(\d+)\]', r'[(long)(int)(\1 - uVar\2)]', new)
        if new != l:
            n += 1
        out.append(new)
    return out, n


def oneshot(lines):
    lines, n_join = join_statements(lines)
    n = n_join
    lines, n_ptr = ptr_cast_pass(lines)
    n += n_ptr
    lines, n_lp64 = lp64_ptr_arith_pass(lines)
    n += n_lp64
    text = '\n'.join(lines)
    nfix = 0
    for old, new in HAND_FIXES:
        if new in text:
            continue  # already applied on a previous in-place run
        if old in text:
            nfix += text.count(old)
            text = text.replace(old, new)
    lines = text.split('\n')
    n += nfix
    # &stackXXXXXX in assignments/views: port needs an integer-roundtrip so the
    # void* slot lvalue accepts it (gcc14 rejects implicit conversions)
    for i, l in enumerate(lines):
        new = re.sub(r'(?<![\w.)])&stack0x[0-9a-fA-F]+\b',
                     lambda m: '((void *)(uintptr_t)' + m.group(0) + ')', l)
        if new != l:
            lines[i] = new
            n += 1
    for i, l in enumerate(lines):
        new = re.sub(r'\bthunk_(FUN_[0-9A-Fa-f]{8})\s*\(', r'\1(', l)
        new = re.sub(r'(\w+)\.DATAPART\((\w+)\s*,', r'DATAPART(\1.\2,', new)
        new = new.replace('(*)...', '(*)()')
        if new != l:
            lines[i] = new
            n += 1
    return lines, n

# ------------------------------------------------------ header reconciliation
# Generic: any column-0 line shaped `<type stuff> <name>(` opens a definition.
DEF_START = re.compile(r'^[A-Za-z_][A-Za-z0-9_ \t]*\**[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]*\(')

def reconcile_header(lines):
    defs = []
    i = 0
    while i < len(lines):
        l = lines[i]
        m = DEF_START.match(l)
        if m and not l.startswith((' ', '\t', '#', '/')):
            buf = [l]
            j = i
            joined = buf[0]
            while (('{' not in joined) or (joined.count('(') > joined.count(')'))) and j + 1 < len(lines):
                j += 1
                buf.append(lines[j])
                joined = ' '.join(x.strip() for x in buf)
            jm = re.match(r'^(.*?)\b([A-Za-z_]\w*)[ \t]*\((.*?)\)[ \t]*\{', joined)
            if jm and jm.group(2) not in ('if', 'for', 'while', 'switch', 'return'):
                defs.append((jm.group(1).strip(), jm.group(2), jm.group(3).strip()))
                i = j + 1
                continue
        i += 1
    out = ['/* bblit_game.h -- prototypes regenerated from bblit_game.c definitions. */',
           '#ifndef BBLIT_GAME_H', '#define BBLIT_GAME_H', '',
           '#include "ghidra_types.h"', '#include "data_syms.h"', '#include "winshim.h"', '']
    for ret, name, params in defs:
        out.append(f'{ret} {name}({params});')
    out.append('')
    out.append('#endif')
    open(HDR, 'w').write('\n'.join(out) + '\n')
    return len(defs)

# ----------------------------------------------------------------------- main
def main():
    lines = open(GAME).read().split('\n')
    lines, n = oneshot(lines)
    ndef = reconcile_header(lines)
    print(f'oneshot rewrites: {n}; header prototypes: {ndef}')
    open(GAME, 'w').write('\n'.join(lines))

    prev = None
    for it in range(15):
        diags = run_gcc()
        errs = count_errors(diags)
        total = len([d for d in errs
                     if d['file'].endswith(('bblit_game.c', 'data_syms.h', 'ghidra_types.h'))])
        print(f'pass {it}: {total} errors')
        if total == 0:
            break
        if prev is not None and total >= prev:
            print('  plateau')
            break
        prev = total
        lines = open(GAME).read().split('\n')
        protolines = open(HDR).read().split('\n')
        n1 = fix_void_value_used(diags, lines, protolines)
        n2 = fix_conversions(diags, lines)
        n3 = fix_calls(diags, lines)
        n4 = fix_binary_ops(diags, lines)
        n5 = fix_misc(diags, lines)
        print(f'  fixes: void={n1} conv={n2} calls={n3} binop={n4} misc={n5}')
        open(GAME, 'w').write('\n'.join(lines))
        open(HDR, 'w').write('\n'.join(protolines))
        if n1 + n2 + n3 + n4 + n5 == 0:
            print('  no mechanical fixes applied; remaining census below')
            break

    diags = run_gcc()
    census = collections.Counter()
    for d in count_errors(diags):
        m = d['msg']
        m = re.sub(r"'[^']*'", "'X'", m)
        m = re.sub(r'FUN_[0-9A-Fa-f]{8}', 'F', m)
        m = re.sub(r'DAT_[0-9A-Fa-f]{8}', 'D', m)
        m = re.sub(r'0x[0-9a-fA-F]+', 'N', m)
        census[(d['file'].split('/')[-1], m)] += 1
    print('\nremaining error census (file, class) -> count:')
    for (f, m), c in census.most_common(30):
        print(f'  {c:5d}  {f}: {m}')

if __name__ == '__main__':
    main()
