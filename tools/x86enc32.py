"""80386 instruction encoder for the USE32 segments of tasm.py.

A USE32 segment has 32-bit operands and addresses by default: a word
operation gets the operand-size prefix 66h, an address made of 16-bit
registers, or one written [SMALL n] (TASM's operator for a 16-bit
address with no register), the address-size prefix 67h.  Prefixes are written in the
order REP/LOCK, 66h, segment, 67h.

Jumps are sized as a multi-pass assembler (MASM 6, TASM /m) sizes them
in the end: every jump in its shortest form, no NOP padding.  A forward
jump takes the size its distance had in the pass before; a near one
whose target turns out in reach makes the assembler run another pass
(Assembler.near_checks).  A USE32 segment needs the multi-pass mode.

Choices with two encodings follow the same switches as the 16-bit
encoder (Assembler.regreg_form, test_form, alu_ax_short, lea_smart).
Not here: x87, the system instructions (LGDT, MOV CRn, ...), 32-bit
addresses in USE16 segments.
"""
from tasm import AsmError, REG8, REG16, REG32, SREG, INSTR_NAMES
import x86enc as X
from x86enc import (Enc, fits8, parse_operands, target_info, fwd_fits, rel_to,
                    jump_name, ALU, SHIFT, GRP3, JCC, LOOPS, PREFIX)

SEGPFX = {'ES': 0x26, 'CS': 0x2E, 'SS': 0x36, 'DS': 0x3E, 'FS': 0x64, 'GS': 0x65}
SETCC = {'SET' + k[1:]: v - 0x70 for k, v in JCC.items()}
BT = {'BT': 4, 'BTS': 5, 'BTR': 6, 'BTC': 7}
# (bytes, operand size): the size says whether 66h goes before them
SIMPLE = {
    'CLI': ([0xFA], 0), 'STI': ([0xFB], 0), 'CLC': ([0xF8], 0), 'STC': ([0xF9], 0),
    'CMC': ([0xF5], 0), 'CLD': ([0xFC], 0), 'STD': ([0xFD], 0), 'LAHF': ([0x9F], 0),
    'SAHF': ([0x9E], 0), 'NOP': ([0x90], 0), 'HLT': ([0xF4], 0), 'XLAT': ([0xD7], 0),
    'XLATB': ([0xD7], 0), 'AAA': ([0x37], 0), 'AAS': ([0x3F], 0), 'DAA': ([0x27], 0),
    'DAS': ([0x2F], 0), 'WAIT': ([0x9B], 0), 'FWAIT': ([0x9B], 0), 'INTO': ([0xCE], 0),
    'LEAVE': ([0xC9], 0), 'CLTS': ([0x0F, 0x06], 0),
    'CBW': ([0x98], 2), 'CWDE': ([0x98], 4), 'CWD': ([0x99], 2), 'CDQ': ([0x99], 4),
    'PUSHA': ([0x60], 2), 'PUSHAW': ([0x60], 2), 'PUSHAD': ([0x60], 4),
    'POPA': ([0x61], 2), 'POPAW': ([0x61], 2), 'POPAD': ([0x61], 4),
    'PUSHF': ([0x9C], 2), 'PUSHFW': ([0x9C], 2), 'PUSHFD': ([0x9C], 4),
    'POPF': ([0x9D], 2), 'POPFW': ([0x9D], 2), 'POPFD': ([0x9D], 4),
    # IRET as the 32-bit form in a USE32 segment (not checked against MASM or TASM)
    'IRET': ([0xCF], 4), 'IRETD': ([0xCF], 4), 'IRETW': ([0xCF], 2),
    'MOVSB': ([0xA4], 1), 'MOVSW': ([0xA5], 2), 'MOVSD': ([0xA5], 4),
    'CMPSB': ([0xA6], 1), 'CMPSW': ([0xA7], 2), 'CMPSD': ([0xA7], 4),
    'STOSB': ([0xAA], 1), 'STOSW': ([0xAB], 2), 'STOSD': ([0xAB], 4),
    'LODSB': ([0xAC], 1), 'LODSW': ([0xAD], 2), 'LODSD': ([0xAD], 4),
    'SCASB': ([0xAE], 1), 'SCASW': ([0xAF], 2), 'SCASD': ([0xAF], 4),
    'INSB': ([0x6C], 1), 'INSW': ([0x6D], 2), 'INSD': ([0x6D], 4),
    'OUTSB': ([0x6E], 1), 'OUTSW': ([0x6F], 2), 'OUTSD': ([0x6F], 4),
}
LOOPS32 = dict(LOOPS, JECXZ=0xE3, LOOPD=0xE2, LOOPDE=0xE1, LOOPDZ=0xE1,
               LOOPDNE=0xE0, LOOPDNZ=0xE0)

INSTR_NAMES.update(SETCC, BT, SIMPLE, LOOPS32)
INSTR_NAMES.update(['MOVZX', 'MOVSX', 'BSF', 'BSR', 'SHLD', 'SHRD', 'LFS', 'LGS', 'LSS',
                    'BSWAP', 'IRETD'])


# ---------------------------------------------------------------- operands

def classify(v):
    if v.reg:
        if v.reg in REG8:
            return 'r8'
        if v.reg in REG16:
            return 'r16'
        if v.reg in REG32:
            return 'r32'
        return 'sreg'
    if v.mem or v.hasbr or v.base or v.index:
        return 'mem'
    if v.label and not v.isoffset and v.rel is not None:
        return 'mem'
    if X.LENIENT[0] and v.unknown and v.label and not v.isoffset and v.rel is None:
        return 'mem'
    return 'imm'


GPR = ('r8', 'r16', 'r32')
RSIZE = {'r8': 1, 'r16': 2, 'r32': 4}


def regno(v):
    for t in (REG32, REG16, REG8, SREG):
        if v.reg in t:
            return t.index(v.reg)
    raise AsmError(f'not a register: {v.reg}')


def opsize(*vs):
    """The operation's size: a register's, else a sized memory operand's,
    else 4 for an OFFSET (None if nothing says)."""
    for v in vs:
        c = classify(v)
        if c in GPR:
            return RSIZE[c]
    for v in vs:
        if classify(v) == 'mem' and v.size in (1, 2, 4):
            return v.size
    for v in vs:
        if classify(v) == 'imm' and v.rel is not None:
            return 4
    if X.LENIENT[0]:
        return 4
    return None


def is_small_imm(v, size):
    """Does the immediate fit the sign-extended byte form of a size-byte
    operation?  (0FFFFFF80h in a dword operation is -128.)"""
    if v.rel is not None or v.segrel is not None or v.unknown:
        return False
    n = v.num
    top = 1 << (8 * size)
    if top - 128 <= n < top:
        n -= top
    return fits8(n)


def addr16(v):
    return v.base in REG16 or v.index in REG16 or getattr(v, 'small', False)


def default_seg(v):
    return 'SS' if v.base in ('BP', 'EBP', 'ESP') else 'DS'


def segment_prefix(e, v):
    """The override a memory operand needs (None for none): a written one
    unless it is the default, else from ASSUME for the variable's segment."""
    a = e.a
    default = default_seg(v)
    if v.ovr:
        if isinstance(v.ovr, tuple):
            name = v.ovr[1]
            for r in ('DS', 'SS', 'ES', 'CS', 'FS', 'GS'):
                if a.assume.get(r) == name:
                    return None if r == default else r
            raise AsmError(f'no segment register assumed to {name}')
        return None if v.ovr == default else v.ovr
    if v.vseg is None or a.assume.get(default) == v.vseg:
        return None
    for r in ('DS', 'SS', 'ES', 'CS', 'FS', 'GS'):
        if a.assume.get(r) == v.vseg:
            return r
    return None


def prefixes(e, size=None, m=None):
    """66h for a word operation, then the segment override and 67h of the
    memory operand m."""
    if size == 2:
        e.b(0x66)
    if m is not None and classify(m) == 'mem':
        r = e.decide('seg', segment_prefix(e, m))
        if r:
            e.b(SEGPFX[r])
        if addr16(m):
            e.b(0x67)


def d32(e, n):
    e.out += (n & 0xFFFFFFFF).to_bytes(4, 'little')


def disp32(e, v):
    if v.rel is not None:
        e.fix.append((len(e.out), 'OFF32', v.rel, v.num))
    d32(e, v.num)


def modrm(e, v, regfield):
    """ModRM (SIB, displacement) for a register or memory operand."""
    if v.reg:
        e.b(0xC0 | (regfield << 3) | regno(v))
        return
    if addr16(v):
        X.emit_modrm(e, v, regfield)
        return
    base, index = v.base, v.index
    reloc = v.rel is not None
    if base is None and index is None:
        e.b(0x05 | (regfield << 3))
        disp32(e, v)
        return
    if base is None:
        dsz = 32                  # [index*scale+disp32]: no form without a displacement
    elif reloc or v.unknown:
        dsz = 32
    elif v.num == 0 and base != 'EBP':
        dsz = 0
    elif fits8(v.num):
        dsz = 8
    else:
        dsz = 32
    if base is not None:
        dsz = e.decide('disp', dsz)
    if dsz == 0 and v.num != 0:
        raise AsmError('phase error in displacement')
    if dsz == 8 and not fits8(v.num):
        raise AsmError('phase error: displacement grew')
    mod = 0 if base is None else {0: 0, 8: 1, 32: 2}[dsz]
    if index is None and base != 'ESP':
        e.b((mod << 6) | (regfield << 3) | REG32.index(base))
    else:
        ss = {1: 0, 2: 1, 4: 2, 8: 3}[v.scale if index else 1]
        e.b((mod << 6) | (regfield << 3) | 4)
        e.b((ss << 6) | ((REG32.index(index) if index else 4) << 3) |
            (REG32.index(base) if base else 5))
    if dsz == 8:
        e.b(v.num)
    elif dsz == 32:
        disp32(e, v)


def imm(e, v, size):
    if size == 1:
        if v.rel is not None or v.segrel is not None:
            raise AsmError('relocatable byte immediate')
        e.b(v.num)
    elif size == 2:
        if v.rel is not None:
            raise AsmError('a 32-bit offset in a word')
        if v.segrel is not None:
            e.fix.append((len(e.out), 'SEG', v.segrel, 0))
            e.w(0)
        else:
            e.w(v.num)
    else:
        if v.segrel is not None:
            raise AsmError('SEG in a dword')
        disp32(e, v)


def need(size):
    if size is None:
        raise AsmError('operand size unknown')
    return size


# ---------------------------------------------------------------- main

def encode(asm, seg, seq, mn, toks):
    if not asm.multipass:
        raise AsmError('USE32 segments need the multi-pass mode')
    X.LENIENT[0] = asm.passno == 0
    e = Enc(asm, seg, seq)
    while mn in PREFIX or mn in SEGPFX and toks and toks[0].k == 'id':
        e.b(PREFIX[mn] if mn in PREFIX else SEGPFX[mn])
        if not toks:
            e.finish()
            return
        mn, toks = toks[0].v, toks[1:]
    f = HANDLERS.get(mn)
    if f is None:
        for tbl, h in ((ALU, enc_alu), (SHIFT, enc_shift), (GRP3, enc_grp3),
                       (JCC, enc_jcc), (LOOPS32, enc_loop), (SIMPLE, enc_simple),
                       (SETCC, enc_setcc), (BT, enc_bt)):
            if mn in tbl:
                f = h
                break
    if f is None:
        raise AsmError(f'unknown instruction or directive {mn}')
    f(e, mn, toks)
    e.finish()


def enc_simple(e, mn, toks):
    code, size = SIMPLE[mn]
    if size == 2:
        e.b(0x66)
    if toks:
        # XLAT table, MOVSB with operands: only an override counts
        for v in parse_operands(e.a, toks):
            if v.ovr and not isinstance(v.ovr, tuple) and v.ovr != 'DS':
                e.b(SEGPFX[v.ovr])
    e.out += bytes(code)


def enc_strop(e, mn, toks):
    vs = parse_operands(e.a, toks)
    size = next((v.size for v in vs if v.size), None)
    base = {'MOVS': 0xA4, 'CMPS': 0xA6, 'STOS': 0xAA, 'LODS': 0xAC, 'SCAS': 0xAE,
            'INS': 0x6C, 'OUTS': 0x6E}[mn]
    if size == 2:
        e.b(0x66)
    src = vs[-1] if mn in ('MOVS', 'LODS', 'CMPS', 'OUTS') else None
    if src is not None and src.ovr:
        r = segment_prefix(e, src)
        if r:
            e.b(SEGPFX[r])
    if vs and addr16(vs[0]):
        e.b(0x67)
    e.b(base + (0 if need(size) == 1 else 1))


def alu_like(e, op_rm_r, d, s, size):
    """Two-operand register/memory forms: op_rm_r is the 'r/m, reg' opcode,
    +2 the 'reg, r/m' one."""
    w = 0 if size == 1 else 1
    cd, cs = classify(d), classify(s)
    if cd in GPR and cs in GPR:
        prefixes(e, size)
        if e.a.regreg_form == 'rm_reg':
            e.b(op_rm_r + w)
            modrm(e, d, regno(s))
        else:
            e.b(op_rm_r + 2 + w)
            modrm(e, s, regno(d))
    elif cd in GPR:
        prefixes(e, size, s)
        e.b(op_rm_r + 2 + w)
        modrm(e, s, regno(d))
    elif cs in GPR:
        prefixes(e, size, d)
        e.b(op_rm_r + w)
        modrm(e, d, regno(s))
    else:
        raise AsmError('bad operands')


def enc_alu(e, mn, toks):
    d, s = parse_operands(e.a, toks)
    n = ALU[mn]
    size = opsize(d, s)
    if classify(s) != 'imm':
        return alu_like(e, n * 8, d, s, size)
    size = need(size)
    small = is_small_imm(s, size)
    if d.reg in ('AL', 'AX', 'EAX'):
        # the accumulator form where it is not longer (AL; AX with 66h,
        # 4 bytes either way), or (MASM, `asm alu_ax_short=0`) the
        # sign-extended byte form where it fits; EAX with a byte-sized
        # immediate always takes the shorter 3-byte form
        if size == 1 or not small or (size == 2 and getattr(e.a, 'alu_ax_short', True)):
            prefixes(e, size)
            e.b(n * 8 + (4 if size == 1 else 5))
            imm(e, s, size)
            return
    prefixes(e, size, d)
    if size == 1:
        e.b(0x80)
        modrm(e, d, n)
        imm(e, s, 1)
        return
    small = e.decide('imm8', small)
    e.b(0x83 if small else 0x81)
    modrm(e, d, n)
    if small:
        e.b(s.num)
    else:
        imm(e, s, size)


def enc_mov(e, mn, toks):
    d, s = parse_operands(e.a, toks)
    cd, cs = classify(d), classify(s)
    if cd == 'sreg' or cs == 'sreg':
        if cd == cs:
            raise AsmError('mov sreg,sreg')
        if cd == 'sreg':
            # MOV DS,AX has no 66h: the move is a word whatever the register
            prefixes(e, None, s)
            e.b(0x8E)
            modrm(e, s, regno(d))
        else:
            size = RSIZE.get(cd) if cd in GPR else d.size
            prefixes(e, 2 if size == 2 else None, d)
            e.b(0x8C)
            modrm(e, d, regno(s))
        return
    size = opsize(d, s)
    if cs == 'imm':
        if cd in GPR:
            prefixes(e, RSIZE[cd])
            e.b((0xB0 if cd == 'r8' else 0xB8) + regno(d))
            imm(e, s, RSIZE[cd])
            return
        size = need(size)
        prefixes(e, size, d)
        e.b(0xC6 if size == 1 else 0xC7)
        modrm(e, d, 0)
        imm(e, s, size)
        return
    w = 0 if size == 1 else 1
    # the accumulator and a direct address: A0..A3
    for acc, m, op in ((d, s, 0xA0), (s, d, 0xA2)):
        if acc.reg in ('AL', 'AX', 'EAX') and classify(m) == 'mem' \
                and m.base is None and m.index is None:
            prefixes(e, size, m)
            e.b(op + w)
            if addr16(m):
                if m.rel is not None:
                    e.fix.append((len(e.out), 'OFF', m.rel, m.num))
                e.w(m.num)
            else:
                disp32(e, m)
            return
    alu_like(e, 0x88, d, s, size)


def enc_movx(e, mn, toks):
    d, s = parse_operands(e.a, toks)
    if classify(d) not in ('r16', 'r32'):
        raise AsmError(f'{mn} needs a word or dword register')
    ssize = RSIZE.get(classify(s)) or s.size
    if ssize not in (1, 2):
        raise AsmError(f'{mn}: source size unknown')
    prefixes(e, RSIZE[classify(d)], s)
    e.b(0x0F, (0xB6 if mn == 'MOVZX' else 0xBE) + (ssize - 1))
    modrm(e, s, regno(d))


GLUE = {'PTR', 'OFFSET', 'SEG', 'BYTE', 'WORD', 'DWORD', 'SHORT', 'SMALL', 'NEAR', 'FAR', 'MOD',
        'SHL', 'SHR', 'AND', 'OR', 'XOR', 'NOT', 'HIGH', 'LOW', 'SIZE', 'TYPE', 'LENGTH',
        'EQ', 'NE', 'LT', 'LE', 'GT', 'GE'}


def push_operands(asm, toks):
    """TASM takes several operands separated by blanks: PUSH EAX EBX."""
    if any(t.v == ',' for t in toks):
        raise AsmError('comma in push/pop')
    groups, prev = [[]], None
    for t in toks:
        if prev is not None and prev.k != 'op' and t.k != 'op' and \
                prev.v not in GLUE and t.v not in GLUE:
            groups.append([])
        groups[-1].append(t)
        prev = t
    return [asm.eval_toks(g) for g in groups]


def enc_pushpop(e, mn, toks):
    push = mn == 'PUSH'
    for v in push_operands(e.a, toks):
        c = classify(v)
        if c in ('r16', 'r32'):
            prefixes(e, RSIZE[c])
            e.b((0x50 if push else 0x58) + regno(v))
        elif c == 'sreg':
            n = regno(v)
            if n >= 4:
                e.b(0x0F, (0xA0 if push else 0xA1) + (n - 4) * 8)
            elif push:
                e.b([0x06, 0x0E, 0x16, 0x1E][n])
            elif n != 1:
                e.b([0x07, None, 0x17, 0x1F][n])
            else:
                raise AsmError('POP CS')
        elif c == 'mem':
            size = v.size or 4
            prefixes(e, size, v)
            e.b(0xFF if push else 0x8F)
            modrm(e, v, 6 if push else 0)
        elif c == 'imm' and push:
            small = e.decide('imm8', is_small_imm(v, 4))
            if small:
                e.b(0x6A, v.num)
            else:
                e.b(0x68)
                imm(e, v, 4)
        else:
            raise AsmError(f'bad {mn} operand')


def enc_incdec(e, mn, toks):
    (v,) = parse_operands(e.a, toks)
    c = classify(v)
    n = 0 if mn == 'INC' else 1
    if c in ('r16', 'r32'):
        prefixes(e, RSIZE[c])
        e.b(0x40 + n * 8 + regno(v))
        return
    size = need(opsize(v))
    prefixes(e, size, v)
    e.b(0xFE if size == 1 else 0xFF)
    modrm(e, v, n)


def enc_grp3(e, mn, toks):
    vs = parse_operands(e.a, toks)
    if mn == 'IMUL' and len(vs) >= 2:
        d = vs[0]
        size = RSIZE[classify(d)]
        if len(vs) == 2 and classify(vs[1]) != 'imm':
            prefixes(e, size, vs[1])
            e.b(0x0F, 0xAF)
            modrm(e, vs[1], regno(d))
            return
        src, im = (d, vs[1]) if len(vs) == 2 else (vs[1], vs[2])
        prefixes(e, size, src)
        small = e.decide('imm8', is_small_imm(im, size))
        e.b(0x6B if small else 0x69)
        modrm(e, src, regno(d))
        if small:
            e.b(im.num)
        else:
            imm(e, im, size)
        return
    (v,) = vs
    size = need(opsize(v))
    prefixes(e, size, v)
    e.b(0xF6 if size == 1 else 0xF7)
    modrm(e, v, GRP3[mn])


def enc_test(e, mn, toks):
    d, s = parse_operands(e.a, toks)
    cd, cs = classify(d), classify(s)
    size = opsize(d, s)
    w = 0 if size == 1 else 1
    if cs == 'imm':
        size = need(size)
        if d.reg in ('AL', 'AX', 'EAX'):
            prefixes(e, size)
            e.b(0xA8 + w)
        else:
            prefixes(e, size, d)
            e.b(0xF6 + w)
            modrm(e, d, 0)
        imm(e, s, size)
        return
    if cd == 'mem' or cs == 'mem':
        m, r = (d, s) if cd == 'mem' else (s, d)
        prefixes(e, size, m)
        e.b(0x84 + w)
        modrm(e, m, regno(r))
        return
    prefixes(e, size)
    e.b(0x84 + w)
    # MASM (test_form = 'rm_reg') puts the first register in r/m
    if getattr(e.a, 'test_form', e.a.regreg_form) == 'rm_reg':
        modrm(e, d, regno(s))
    else:
        modrm(e, s, regno(d))


def enc_xchg(e, mn, toks):
    d, s = parse_operands(e.a, toks)
    cd, cs = classify(d), classify(s)
    size = opsize(d, s)
    if cd == cs and cd in ('r16', 'r32') and (d.reg in ('AX', 'EAX') or s.reg in ('AX', 'EAX')):
        other = s if d.reg in ('AX', 'EAX') else d
        prefixes(e, size)
        e.b(0x90 + regno(other))
        return
    w = 0 if size == 1 else 1
    m, r = (d, s) if cd == 'mem' else (s, d)
    prefixes(e, size, m)
    e.b(0x86 + w)
    modrm(e, m, regno(r))


def enc_lea(e, mn, toks):
    d, s = parse_operands(e.a, toks)
    s = s.copy()
    s.mem = True
    size = RSIZE[classify(d)]
    if mn == 'LEA' and s.base is None and s.index is None and getattr(e.a, 'lea_smart', True):
        # TASM turns LEA of a direct address into MOV reg,OFFSET
        prefixes(e, size)
        e.b(0xB8 + regno(d))
        imm(e, s, size)
        return
    if mn == 'LEA':
        # LEA ignores the segment: only 66h and 67h
        prefixes(e, size)
        if addr16(s):
            e.b(0x67)
    else:
        prefixes(e, size, s)
    e.out += bytes({'LEA': [0x8D], 'LDS': [0xC5], 'LES': [0xC4], 'LSS': [0x0F, 0xB2],
                    'LFS': [0x0F, 0xB4], 'LGS': [0x0F, 0xB5]}[mn])
    modrm(e, s, regno(d))


def enc_shift(e, mn, toks):
    d, c = parse_operands(e.a, toks)
    size = need(opsize(d))
    w = 0 if size == 1 else 1
    n = SHIFT[mn]
    prefixes(e, size, d)
    if c.reg == 'CL':
        e.b(0xD2 + w)
        modrm(e, d, n)
    elif c.num == 1 and not c.unknown:
        e.b(0xD0 + w)
        modrm(e, d, n)
    else:
        e.b(0xC0 + w)
        modrm(e, d, n)
        e.b(c.num)


def enc_shxd(e, mn, toks):
    d, s, c = parse_operands(e.a, toks)
    prefixes(e, RSIZE[classify(s)], d)
    op = 0xA4 if mn == 'SHLD' else 0xAC
    if c.reg == 'CL':
        e.b(0x0F, op + 1)
        modrm(e, d, regno(s))
    else:
        e.b(0x0F, op)
        modrm(e, d, regno(s))
        e.b(c.num)


def enc_bt(e, mn, toks):
    d, s = parse_operands(e.a, toks)
    if classify(s) == 'imm':
        prefixes(e, need(opsize(d)), d)
        e.b(0x0F, 0xBA)
        modrm(e, d, BT[mn])
        e.b(s.num)
    else:
        prefixes(e, RSIZE[classify(s)], d)
        e.b(0x0F, 0xA3 + (BT[mn] - 4) * 8)
        modrm(e, d, regno(s))


def enc_bsx(e, mn, toks):
    d, s = parse_operands(e.a, toks)
    prefixes(e, RSIZE[classify(d)], s)
    e.b(0x0F, 0xBC if mn == 'BSF' else 0xBD)
    modrm(e, s, regno(d))


def enc_setcc(e, mn, toks):
    (v,) = parse_operands(e.a, toks)
    prefixes(e, None, v)
    e.b(0x0F, 0x90 + SETCC[mn])
    modrm(e, v, 0)


def enc_bswap(e, mn, toks):
    (v,) = parse_operands(e.a, toks)
    e.b(0x0F, 0xC8 + regno(v))


def enc_inout(e, mn, toks):
    a, b = parse_operands(e.a, toks)
    acc, port = (a, b) if mn == 'IN' else (b, a)
    size = RSIZE[classify(acc)]
    w = 0 if size == 1 else 1
    prefixes(e, size)
    if port.reg == 'DX':
        e.b((0xEC if mn == 'IN' else 0xEE) + w)
    else:
        e.b((0xE4 if mn == 'IN' else 0xE6) + w, port.num)


def enc_int(e, mn, toks):
    (v,) = parse_operands(e.a, toks)
    if v.num == 3:
        e.b(0xCC)
    else:
        e.b(0xCD, v.num)


def enc_ret(e, mn, toks):
    far = mn == 'RETF' or mn == 'RET' and e.a.proc_stack and e.a.proc_stack[-1][1] == 'FAR'
    if toks:
        (v,) = parse_operands(e.a, toks)
        e.b(0xCA if far else 0xC2)
        e.w(v.num)
    else:
        e.b(0xCB if far else 0xC3)


def enc_aam(e, mn, toks):
    n = parse_operands(e.a, toks)[0].num if toks else 10
    e.b(0xD4 if mn == 'AAM' else 0xD5, n)


def enc_enter(e, mn, toks):
    a, b = parse_operands(e.a, toks)
    e.b(0xC8)
    e.w(a.num)
    e.b(b.num)


# ---------------------------------------------------------------- control flow

def jump_shape(e, v, near_len):
    """'short' or 'near' for a jump to v (the short form is 2 bytes)."""
    kind, same = target_info(e, v)
    if kind == 'extern' or not same:
        return kind, same, 'near'
    if v.short:
        shape = 'short'
    elif kind == 'fwd':
        shape = 'short' if fwd_fits(e, v) else 'near'
        if shape == 'near' and not e.a.emit and e.fwdname and e.a.passno > 0:
            # in reach after all? then another pass (MASM's shortest jumps)
            e.a.near_checks.append((e.seq, e.fwdname, e.seg.name))
    else:
        shape = 'short' if fits8(rel_to(e, v, 2)) else 'near'
    return kind, same, e.decide('j', shape)


def short(e, op, v):
    d = rel_to(e, v, len(e.out) + 2)
    if e.a.emit and not fits8(d):
        raise AsmError(f'jump out of range by {abs(d) - 128} bytes')
    e.b(op)
    e.relbytes(1)
    e.b(d)


def rel32(e, v, kind):
    e.relbytes(4)
    if kind == 'extern':
        e.fix.append((len(e.out), 'REL32', v.rel, v.num))
        d32(e, 0)
    else:
        d32(e, rel_to(e, v, len(e.out) + 4))


def enc_jcc(e, mn, toks):
    jump_name(e, toks)
    (v,) = parse_operands(e.a, toks)
    kind, same, shape = jump_shape(e, v, 6)
    if kind == 'extern' or not same:
        raise AsmError('conditional jump to another segment')
    if shape == 'short':
        short(e, JCC[mn], v)
    else:
        e.b(0x0F, JCC[mn] + 0x10)
        rel32(e, v, kind)


def enc_loop(e, mn, toks):
    jump_name(e, toks)
    (v,) = parse_operands(e.a, toks)
    if mn == 'JCXZ':
        e.b(0x67)             # CX: a 16-bit address size
    short(e, LOOPS32[mn], v)


def far_ptr(e, v, op):
    e.b(op)
    e.fix.append((len(e.out), 'OFF32', v.rel, v.num))
    d32(e, v.num)
    e.fix.append((len(e.out), 'SEG', v.rel, 0))
    e.w(0)


def enc_jmp(e, mn, toks):
    jump_name(e, toks)
    (v,) = parse_operands(e.a, toks)
    c = classify(v) if not (v.label and not v.hasbr and not v.mem) else 'label'
    if c == 'label' and v.rel is None and not v.unknown:
        raise AsmError('jump to a constant')
    if c in ('mem', 'r32', 'r16'):
        far = v.dist == 'FAR' or v.size == 6
        prefixes(e, 2 if c == 'r16' or v.size == 2 else None, v)
        e.b(0xFF)
        if mn == 'JMP':
            modrm(e, v, 5 if far else 4)
        else:
            modrm(e, v, 3 if far else 2)
        return
    kind, same = target_info(e, v)
    if kind == 'known' and (v.dist == 'FAR' or not same):
        return far_ptr(e, v, 0xEA if mn == 'JMP' else 0x9A)
    if mn == 'CALL':
        e.b(0xE8)
        rel32(e, v, kind)
        return
    kind, same, shape = jump_shape(e, v, 5)
    if shape == 'short':
        short(e, 0xEB, v)
    else:
        e.b(0xE9)
        rel32(e, v, kind)


HANDLERS = {
    'MOV': enc_mov, 'MOVZX': enc_movx, 'MOVSX': enc_movx, 'PUSH': enc_pushpop,
    'POP': enc_pushpop, 'INC': enc_incdec, 'DEC': enc_incdec, 'TEST': enc_test,
    'XCHG': enc_xchg, 'LEA': enc_lea, 'LDS': enc_lea, 'LES': enc_lea, 'LSS': enc_lea,
    'LFS': enc_lea, 'LGS': enc_lea, 'IN': enc_inout, 'OUT': enc_inout, 'INT': enc_int,
    'RET': enc_ret, 'RETN': enc_ret, 'RETF': enc_ret, 'JMP': enc_jmp, 'CALL': enc_jmp,
    'AAM': enc_aam, 'AAD': enc_aam, 'ENTER': enc_enter, 'SHLD': enc_shxd,
    'SHRD': enc_shxd, 'BSF': enc_bsx, 'BSR': enc_bsx, 'BSWAP': enc_bswap,
    'MOVS': enc_strop, 'LODS': enc_strop, 'STOS': enc_strop, 'CMPS': enc_strop,
    'SCAS': enc_strop, 'INS': enc_strop, 'OUTS': enc_strop,
}
