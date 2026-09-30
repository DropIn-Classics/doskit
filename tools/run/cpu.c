/* 386 CPU interpreter: real mode, protected mode, V86 mode (see dosrun.h
 * and "protected mode" below) */
#include "dosrun.h"
#include <setjmp.h>
/* setjmp saves the signal mask on BSD and macOS, a system call each time:
 * once an instruction under PE that made the runner five times slower */
#ifdef _WIN32
#define FAULT_JMP_BUF jmp_buf
#define FAULT_SETJMP(b) setjmp(b)
#define FAULT_LONGJMP(b) longjmp(b, 1)
#else
#define FAULT_JMP_BUF sigjmp_buf
#define FAULT_SETJMP(b) sigsetjmp(b, 0)
#define FAULT_LONGJMP(b) siglongjmp(b, 1)
#endif

CPU cpu;
uint32_t a20_mask = 0xFFFFFFFFu;

static uint8_t ptab[256];
static void init_ptab(void){
    int i,j,c;
    for(i=0;i<256;i++){ c=0; for(j=0;j<8;j++) if(i&(1<<j)) c++; ptab[i] = (c&1)?0:1; }
}

/* ------------------------------------------------------------------ flags */
uint32_t cpu_getflags(void){
    return 0x0002u | (cpu.cf) | (cpu.pf<<2) | (cpu.af<<4) | (cpu.zf<<6) |
           (cpu.sf<<7) | (cpu.tf<<8) | (cpu.iflag<<9) | (cpu.df<<10) |
           (cpu.of<<11) | (cpu.iopl<<12) | (cpu.nt<<14) | (cpu.rf<<16) |
           (cpu.vm<<17) | (cpu.ac<<18);
}
/* all but VM, which only an interrupt and IRET change */
void cpu_setflags(uint32_t f){
    cpu.cf=f&1; cpu.pf=(f>>2)&1; cpu.af=(f>>4)&1; cpu.zf=(f>>6)&1;
    cpu.sf=(f>>7)&1; cpu.tf=(f>>8)&1; cpu.iflag=(f>>9)&1; cpu.df=(f>>10)&1;
    cpu.of=(f>>11)&1; cpu.iopl=(f>>12)&3; cpu.nt=(f>>14)&1; cpu.rf=(f>>16)&1;
    cpu.ac=(f>>18)&1;
}

/* ------------------------------------------------------------- decode ctx */
static int opsz, adsz, segovr, rep;
static uint32_t cs_base;
static int def32;                  /* the code segment's D bit: 32-bit operands and addresses */
static uint32_t ipmask;            /* EIP wraps at 64 KB in a 16-bit code segment */
#define PE (cpu.cr0 & 1)
#define STK32 (cpu.sbig[S_SS])
static int mod_, reg_, rm_;
static uint32_t ea, ea_off;
static int ea_isreg;
static int no_iret;
static void cpu_undef(const char *what);   /* defined with the trace ring below */

#define MASK(sz) ((sz)==32?0xFFFFFFFFu:((sz)==16?0xFFFFu:0xFFu))

/* Local memory helpers: byte-for-byte the semantics of mem_r8/mem_w8 in
 * vga.c (A20 + 16 MB wrap, VGA dispatch, ROM write-ignore), but inlined in
 * this translation unit.  The interpreter executes millions of instructions
 * per second and almost every one fetches bytes and touches memory through
 * here, so removing the per-access cross-TU call (and the repeated
 * mask/branch inside mem_r16/mem_r32) matters.  Behaviour is unchanged. */
#define CPU_VGA_LO 0xA0000u
#define CPU_VGA_HI 0xC0000u
/* instruction fetches: not seen by -rwatch */
static inline uint8_t cpu_fetch8(uint32_t a){
    a &= a20_mask; a &= (RAM_SIZE-1);
    if(a >= CPU_VGA_LO && a < CPU_VGA_HI) return vga_mem_r(a);
    return ram[a];
}
/* -rwatch: rwatch_hi is 0 while it is off, so a load pays one compare */
#define RWATCH(a, n) do { if((a) < rwatch_hi && (a) + (n) > rwatch_lo) rwatch_hit(a, n); } while(0)
static inline uint8_t cpu_ld8(uint32_t a){
    a &= a20_mask; a &= (RAM_SIZE-1);
    RWATCH(a, 1u);
    if(a >= CPU_VGA_LO && a < CPU_VGA_HI) return vga_mem_r(a);
    return ram[a];
}
static inline uint16_t cpu_ld16(uint32_t a){
    a &= a20_mask; a &= (RAM_SIZE-1);
    RWATCH(a, 2u);
    if(a + 1u >= CPU_VGA_LO && a < CPU_VGA_HI) return (uint16_t)(cpu_ld8(a) | ((uint16_t)cpu_ld8(a+1) << 8));
    return (uint16_t)(ram[a] | ((uint16_t)ram[a+1] << 8));
}
static inline uint32_t cpu_ld32(uint32_t a){
    a &= a20_mask; a &= (RAM_SIZE-1);
    RWATCH(a, 4u);
    if(a + 3u >= CPU_VGA_LO && a < CPU_VGA_HI)
        return (uint32_t)cpu_ld8(a) | ((uint32_t)cpu_ld8(a+1) << 8) |
               ((uint32_t)cpu_ld8(a+2) << 16) | ((uint32_t)cpu_ld8(a+3) << 24);
    return (uint32_t)ram[a] | ((uint32_t)ram[a+1] << 8) |
           ((uint32_t)ram[a+2] << 16) | ((uint32_t)ram[a+3] << 24);
}
static inline void cpu_st8(uint32_t a, uint8_t v){
    a &= a20_mask; a &= (RAM_SIZE-1);
    if(a == memwatch_addr) memwatch_hit(a, v);
    if(a >= CPU_VGA_LO && a < CPU_VGA_HI){ vga_mem_w(a, v); return; }
    if(a >= 0xC0000 && a < 0x100000) return;   /* ROM */
    ram[a] = v;
}
static inline void cpu_st16(uint32_t a, uint16_t v){
    a &= a20_mask; a &= (RAM_SIZE-1);
    if(a + 1u >= CPU_VGA_LO && a < 0x100000){ cpu_st8(a, (uint8_t)v); cpu_st8(a+1, (uint8_t)(v >> 8)); return; }
    if(a == memwatch_addr) memwatch_hit(a, (uint8_t)v);
    else if(a + 1u == memwatch_addr) memwatch_hit(a + 1u, (uint8_t)(v >> 8));
    ram[a] = (uint8_t)v; ram[a+1] = (uint8_t)(v >> 8);
}
static inline void cpu_st32(uint32_t a, uint32_t v){
    a &= a20_mask; a &= (RAM_SIZE-1);
    if(a + 3u >= CPU_VGA_LO && a < 0x100000){
        cpu_st8(a, (uint8_t)v); cpu_st8(a+1, (uint8_t)(v >> 8));
        cpu_st8(a+2, (uint8_t)(v >> 16)); cpu_st8(a+3, (uint8_t)(v >> 24)); return; }
    if(a <= memwatch_addr && memwatch_addr < a + 4u)
        memwatch_hit(memwatch_addr, (uint8_t)(v >> ((memwatch_addr - a) * 8)));
    ram[a] = (uint8_t)v; ram[a+1] = (uint8_t)(v >> 8);
    ram[a+2] = (uint8_t)(v >> 16); ram[a+3] = (uint8_t)(v >> 24);
}

static uint8_t fetch8(void){ uint8_t v = cpu_fetch8(cs_base + cpu.eip); cpu.eip = (cpu.eip+1)&ipmask; return v; }
static uint16_t fetch16(void){
    uint16_t v;
    if(!rwatch_hi){ v = cpu_ld16(cs_base + cpu.eip); cpu.eip = (cpu.eip+2)&ipmask; return v; }
    v = fetch8(); return (uint16_t)(v | (fetch8() << 8));
}
static uint32_t fetch32(void){
    uint32_t v;
    if(!rwatch_hi){ v = cpu_ld32(cs_base + cpu.eip); cpu.eip = (cpu.eip+4)&ipmask; return v; }
    v = fetch16(); return v | ((uint32_t)fetch16() << 16);
}

static void load_seg(int s, uint16_t sel);       /* protected mode, below */
static void cs_changed(void){
    cs_base = cpu.sbase[S_CS]; def32 = cpu.sbig[S_CS]; ipmask = def32 ? 0xFFFFFFFFu : 0xFFFFu;
}
/* Real mode and V86 mode: the base is the selector times 16.  Real mode
 * keeps the limit and D bit the register had (as the 386 does); V86 mode
 * sets them to 64 KB, 16-bit. */
void set_sreg(int s, uint16_t v){
    if(PE && !cpu.vm){ load_seg(s, v); return; }
    cpu.sreg[s]=v; cpu.sbase[s]=(uint32_t)v<<4;
    if(cpu.vm){ cpu.slimit[s] = 0xFFFF; cpu.sbig[s] = 0; }
    if(s==S_CS) cs_changed();
}
static uint32_t sb(int s){ return cpu.sbase[segovr>=0 ? segovr : s]; }

/* the stack pointer is ESP in a 32-bit stack segment (B bit), SP otherwise */
static inline uint32_t sp_get(void){ return STK32 ? REG32(R_ESP) : REG16(R_ESP); }
static inline void sp_add(uint32_t d){ if(STK32) REG32(R_ESP) += d; else REG16(R_ESP) = (uint16_t)(REG16(R_ESP) + d); }
static void push16(uint16_t v){ sp_add((uint32_t)-2); cpu_st16(cpu.sbase[S_SS] + sp_get(), v); }
static uint16_t pop16(void){ uint16_t v = cpu_ld16(cpu.sbase[S_SS] + sp_get()); sp_add(2); return v; }
static void push32(uint32_t v){ sp_add((uint32_t)-4); cpu_st32(cpu.sbase[S_SS] + sp_get(), v); }
static uint32_t pop32(void){ uint32_t v = cpu_ld32(cpu.sbase[S_SS] + sp_get()); sp_add(4); return v; }
static void pushv(uint32_t v){ if(opsz==32) push32(v); else push16((uint16_t)v); }
static uint32_t popv(void){ return opsz==32?pop32():pop16(); }

void cpu_push16(uint16_t v){ push16(v); }
uint16_t cpu_pop16(void){ return pop16(); }

/* ------------------------------------------------------------- modrm     */
static void modrm(void){
    uint8_t m = fetch8();
    mod_ = m>>6; reg_ = (m>>3)&7; rm_ = m&7;
    ea_isreg = 0;
    if(mod_==3){ ea_isreg=1; return; }
    if(adsz==16){
        uint32_t a=0; int dseg=S_DS;
        switch(rm_){
        case 0: a=(uint16_t)(REG16(R_EBX)+REG16(R_ESI)); break;
        case 1: a=(uint16_t)(REG16(R_EBX)+REG16(R_EDI)); break;
        case 2: a=(uint16_t)(REG16(R_EBP)+REG16(R_ESI)); dseg=S_SS; break;
        case 3: a=(uint16_t)(REG16(R_EBP)+REG16(R_EDI)); dseg=S_SS; break;
        case 4: a=REG16(R_ESI); break;
        case 5: a=REG16(R_EDI); break;
        case 6: if(mod_==0){ a=fetch16(); } else { a=REG16(R_EBP); dseg=S_SS; } break;
        case 7: a=REG16(R_EBX); break;
        }
        if(mod_==1) a += (int8_t)fetch8();
        else if(mod_==2) a += (int16_t)fetch16();
        ea_off = a & 0xFFFF;
        ea = sb(dseg) + ea_off;
    } else {
        uint32_t a=0; int dseg=S_DS;
        if(rm_==4){
            uint8_t sib=fetch8();
            int base=sib&7, idx=(sib>>3)&7, sc=sib>>6;
            if(idx!=4) a += REG32(idx)<<sc;
            if(base==5 && mod_==0) a += fetch32();
            else { a += REG32(base); if(base==4||base==5) dseg=S_SS; }
        } else if(rm_==5 && mod_==0){ a = fetch32(); }
        else { a = REG32(rm_); if(rm_==5) dseg=S_SS; }
        if(mod_==1) a += (int32_t)(int8_t)fetch8();
        else if(mod_==2) a += fetch32();
        ea_off = a;
        ea = sb(dseg) + a;
    }
}

static uint32_t rdE(int sz){
    if(ea_isreg) return sz==8?REG8(rm_): sz==16?REG16(rm_):REG32(rm_);
    return sz==8?cpu_ld8(ea): sz==16?cpu_ld16(ea):cpu_ld32(ea);
}
static void wrE(int sz, uint32_t v){
    if(ea_isreg){ if(sz==8) REG8(rm_)=(uint8_t)v; else if(sz==16) REG16(rm_)=(uint16_t)v; else REG32(rm_)=v; return; }
    if(sz==8) cpu_st8(ea,(uint8_t)v); else if(sz==16) cpu_st16(ea,(uint16_t)v); else cpu_st32(ea,v);
}
static uint32_t rdG(int sz){ return sz==8?REG8(reg_): sz==16?REG16(reg_):REG32(reg_); }
static void wrG(int sz, uint32_t v){ if(sz==8) REG8(reg_)=(uint8_t)v; else if(sz==16) REG16(reg_)=(uint16_t)v; else REG32(reg_)=v; }

/* ------------------------------------------------------------- ALU       */
static void setlog(uint32_t r, int sz){
    cpu.cf=0; cpu.of=0; cpu.af=0;
    cpu.zf = (r & MASK(sz))==0;
    cpu.sf = (r >> (sz-1)) & 1;
    cpu.pf = ptab[r & 0xFF];
}
static uint32_t alu(int op, uint32_t a, uint32_t b, int sz){
    uint64_t r; uint32_t m = MASK(sz); uint32_t res; int c;
    a &= m; b &= m;
    switch(op){
    case 0: case 2:
        c = (op==2) ? (int)cpu.cf : 0;
        r = (uint64_t)a + b + c; res = (uint32_t)r & m;
        cpu.cf = (uint32_t)((r >> sz) & 1);
        cpu.af = ((a ^ b ^ res) >> 4) & 1;
        cpu.of = (((~(a^b)) & (a^res)) >> (sz-1)) & 1;
        break;
    case 5: case 3: case 7:
        c = (op==3) ? (int)cpu.cf : 0;
        r = (uint64_t)a - b - c; res = (uint32_t)r & m;
        cpu.cf = (uint32_t)((r >> sz) & 1);
        cpu.af = ((a ^ b ^ res) >> 4) & 1;
        cpu.of = (((a^b) & (a^res)) >> (sz-1)) & 1;
        break;
    case 1: res = (a|b)&m; setlog(res,sz); return res;
    case 4: res = (a&b)&m; setlog(res,sz); return res;
    case 6: res = (a^b)&m; setlog(res,sz); return res;
    default: res=0; break;
    }
    cpu.zf = res==0; cpu.sf = (res>>(sz-1))&1; cpu.pf = ptab[res&0xFF];
    return res;
}
static uint32_t do_inc(uint32_t a, int sz){
    uint32_t m=MASK(sz), r=(a+1)&m;
    cpu.af = ((a ^ 1 ^ r)>>4)&1;
    cpu.of = (r == ((m>>1)+1));
    cpu.zf = r==0; cpu.sf=(r>>(sz-1))&1; cpu.pf=ptab[r&0xFF];
    return r;
}
static uint32_t do_dec(uint32_t a, int sz){
    uint32_t m=MASK(sz), r=(a-1)&m;
    cpu.af = ((a ^ 1 ^ r)>>4)&1;
    cpu.of = (r == (m>>1));
    cpu.zf = r==0; cpu.sf=(r>>(sz-1))&1; cpu.pf=ptab[r&0xFF];
    return r;
}

/* ------------------------------------------------------------- shifts    */
static uint32_t do_shift(int op, uint32_t v, int cnt, int sz){
    uint32_t m = MASK(sz); uint32_t r = v & m; int i;
    cnt &= 31;
    if(!cnt) return r;
    switch(op){
    case 0: { int n = cnt % sz;
        if(n){ r = ((r<<n)|(r>>(sz-n))) & m; }
        cpu.cf = r & 1; cpu.of = (((r>>(sz-1))&1) ^ cpu.cf);
        return r; }
    case 1: { int n = cnt % sz;
        if(n){ r = ((r>>n)|(r<<(sz-n))) & m; }
        cpu.cf = (r>>(sz-1))&1; cpu.of = (((r>>(sz-1))&1) ^ ((r>>(sz-2))&1));
        return r; }
    case 2: { int n = cnt % (sz+1);
        for(i=0;i<n;i++){ uint32_t nc=(r>>(sz-1))&1; r=((r<<1)|cpu.cf)&m; cpu.cf=nc; }
        cpu.of = (((r>>(sz-1))&1) ^ cpu.cf);
        return r; }
    case 3: { int n = cnt % (sz+1);
        for(i=0;i<n;i++){ uint32_t nc=r&1; r=(r>>1)|(cpu.cf<<(sz-1)); cpu.cf=nc; }
        r &= m; cpu.of = (((r>>(sz-1))&1) ^ ((r>>(sz-2))&1));
        return r; }
    case 4: case 6:
        cpu.cf = (cnt<=sz) ? ((r >> (sz-cnt)) & 1) : 0;
        r = (cnt<32) ? ((r<<cnt)&m) : 0;
        cpu.of = (((r>>(sz-1))&1) ^ cpu.cf);
        break;
    case 5:
        cpu.cf = (cnt<=sz) ? ((r>>(cnt-1))&1) : 0;
        cpu.of = (v>>(sz-1))&1;
        r = (cnt<sz) ? (r>>cnt) : 0;
        break;
    case 7: {
        int32_t s = (int32_t)(r << (32-sz));
        s >>= (32-sz);
        cpu.cf = (cnt<sz) ? (uint32_t)((s>>(cnt-1))&1) : (uint32_t)(s<0?1:0);
        s = (cnt<sz) ? (s>>cnt) : (s<0?-1:0);
        r = ((uint32_t)s) & m; cpu.of = 0;
        break; }
    }
    cpu.zf = r==0; cpu.sf=(r>>(sz-1))&1; cpu.pf=ptab[r&0xFF];
    return r;
}

static int cond(int c){
    switch(c){
    case 0: return cpu.of;            case 1: return !cpu.of;
    case 2: return cpu.cf;            case 3: return !cpu.cf;
    case 4: return cpu.zf;            case 5: return !cpu.zf;
    case 6: return cpu.cf||cpu.zf;    case 7: return !(cpu.cf||cpu.zf);
    case 8: return cpu.sf;            case 9: return !cpu.sf;
    case 10: return cpu.pf;           case 11: return !cpu.pf;
    case 12: return cpu.sf!=cpu.of;   case 13: return cpu.sf==cpu.of;
    case 14: return cpu.zf||(cpu.sf!=cpu.of); case 15: return !cpu.zf&&(cpu.sf==cpu.of);
    }
    return 0;
}

/* ------------------------------------------------------ protected mode */
/* The 386's protected mode as a DOS extender uses it: segment registers
 * loaded from descriptors in the GDT and LDT (base, limit, 16/32-bit),
 * privilege levels with the stacks of the TSS, far jumps, calls and
 * returns through code descriptors and call gates, interrupts and
 * exceptions through the IDT's interrupt and trap gates, V86 mode (entered
 * by IRETD, left by an interrupt), the I/O permission bitmap and the
 * IOPL-sensitive instructions.
 *
 * Not here: paging, task switches (task gates, a far jump or call to a
 * TSS, IRET with NT), the checks of limits and types on memory accesses
 * (a descriptor's limit is kept, not enforced), the alignment check, debug
 * registers.  Any fault during the delivery of an exception is a double
 * fault; a fault during that stops the run (a triple fault).
 *
 * An exception is raised with fault(): it jumps out of the instruction
 * through fault_jb, puts back what the instruction changed of EIP, CS, SS,
 * ESP and CPL and is delivered through the IDT.  The jump is armed only
 * under PE (and around the BIOS's callbacks), so real mode costs nothing
 * more. */

typedef struct { uint32_t lo, hi; } Desc;
#define D_BASE(d)  (((d).lo >> 16) | (((d).hi & 0xFF) << 16) | ((d).hi & 0xFF000000u))
#define D_P(d)     (((d).hi >> 15) & 1)
#define D_DPL(d)   ((int)(((d).hi >> 13) & 3))
#define D_S(d)     (((d).hi >> 12) & 1)          /* code or data (1), system (0) */
#define D_TYPE(d)  ((int)(((d).hi >> 8) & 15))
#define D_BIG(d)   ((uint8_t)(((d).hi >> 22) & 1))
#define D_CODE(d)  (D_S(d) && (D_TYPE(d) & 8))
#define D_CONF(d)  (D_CODE(d) && (D_TYPE(d) & 4))
#define D_WDATA(d) (D_S(d) && (D_TYPE(d) & 10) == 2) /* writable data */
static uint32_t d_limit(Desc d){
    uint32_t l = (d.lo & 0xFFFF) | (d.hi & 0xF0000);
    return (d.hi & 0x800000) ? (l << 12) | 0xFFF : l;
}

enum { IK_HW, IK_SOFT, IK_EXC };

static FAULT_JMP_BUF fault_jb;
static int fault_armed;
static int fault_vec, fault_has_err;
static uint32_t fault_err;
static struct {                     /* at the instruction's start */
    uint32_t eip, esp, base[2], limit[2];
    uint16_t sel[2]; uint8_t big[2]; int cpl;
} saved;

static void save_state(void){
    int i, s;
    saved.eip = cpu.eip; saved.esp = REG32(R_ESP); saved.cpl = cpu.cpl;
    for(i = 0; i < 2; i++){
        s = i ? S_SS : S_CS;
        saved.sel[i] = cpu.sreg[s]; saved.base[i] = cpu.sbase[s];
        saved.limit[i] = cpu.slimit[s]; saved.big[i] = cpu.sbig[s];
    }
}
static void restore_state(void){
    int i, s;
    cpu.eip = saved.eip; REG32(R_ESP) = saved.esp; cpu.cpl = saved.cpl;
    for(i = 0; i < 2; i++){
        s = i ? S_SS : S_CS;
        cpu.sreg[s] = saved.sel[i]; cpu.sbase[s] = saved.base[i];
        cpu.slimit[s] = saved.limit[i]; cpu.sbig[s] = saved.big[i];
    }
    cs_changed();
}

static void fault(int vec, uint32_t err){
    fault_vec = vec; fault_err = err & 0xFFFF;
    fault_has_err = vec == 8 || (vec >= 10 && vec <= 14) || vec == 17;
    if(!fault_armed){
        /* cannot happen: every path that can fault arms fault_jb */
        fprintf(stderr, "dosrun: exception %02Xh with nothing to deliver it\n", vec);
        exit(2);
    }
    FAULT_LONGJMP(fault_jb);
}
#define GP(e) fault(13, (e))

/* something of protected mode the runner does not do: said once a site, and
 * the run stops (going on would only hide where it went wrong) */
static void pm_unsupported(const char *what){
    printf("[cpu] not emulated: %s at %04X:%08X\n", what, cpu.sreg[S_CS], (unsigned)insn_ip);
    cpu.shutdown = 1;
}

static uint32_t desc_addr;          /* where read_desc found it */
static int read_desc(uint16_t sel, Desc *d){
    uint32_t base = cpu.gdt_base, limit = cpu.gdt_limit;
    if(sel & 4){
        if(!(cpu.ldtr & ~3)) return 0;
        base = cpu.ldt_base; limit = cpu.ldt_limit;
    }
    if((uint32_t)(sel | 7) > limit) return 0;
    desc_addr = base + (sel & ~7u);
    d->lo = cpu_ld32(desc_addr); d->hi = cpu_ld32(desc_addr + 4);
    return 1;
}
static void set_accessed(Desc *d){
    if(D_S(*d) && !(d->hi & 0x100)){ d->hi |= 0x100; cpu_st32(desc_addr + 4, d->hi); }
}
static void seg_from_desc(int s, uint16_t sel, Desc d){
    cpu.sreg[s] = sel; cpu.sbase[s] = D_BASE(d);
    cpu.slimit[s] = d_limit(d); cpu.sbig[s] = D_BIG(d);
}
static void set_cs(uint16_t sel, Desc d, int cpl){
    seg_from_desc(S_CS, (uint16_t)((sel & ~3) | cpl), d);
    cpu.cpl = cpl;
    cs_changed();
}
static void set_null(int s, uint16_t sel){
    cpu.sreg[s] = sel; cpu.sbase[s] = 0; cpu.slimit[s] = 0; cpu.sbig[s] = 0;
}

/* SS for privilege level cpl: the checks of a stack switch (#TS or #SS) */
static void check_stack(uint16_t sel, int cpl, Desc *d, int vec){
    if(!(sel & ~3)) fault(vec, 0);
    if(!read_desc(sel, d) || (sel & 3) != cpl || D_DPL(*d) != cpl || !D_WDATA(*d))
        fault(vec, sel);
    if(!D_P(*d)) fault(12, sel);
}

/* MOV, POP, LDS...: a data segment register or SS */
static void load_seg(int s, uint16_t sel){
    Desc d;
    if(s == S_CS){ pm_unsupported("CS loaded as a data segment"); return; }
    if(s == S_SS){
        check_stack(sel, cpu.cpl, &d, 13);
        set_accessed(&d);
        seg_from_desc(S_SS, sel, d);
        return;
    }
    if(!(sel & ~3)){ set_null(s, sel); return; }
    if(!read_desc(sel, &d)) GP(sel);
    if(!D_S(d) || (D_CODE(d) && !(D_TYPE(d) & 2))) GP(sel);   /* system, execute-only */
    if(!D_CONF(d) && (D_DPL(d) < cpu.cpl || D_DPL(d) < (sel & 3))) GP(sel);
    if(!D_P(d)) fault(11, sel);
    set_accessed(&d);
    seg_from_desc(s, sel, d);
}

/* a return to an outer level: the data segments the new level may not use
 * become null */
static void null_inner_segs(void){
    static const int ss[4] = { S_ES, S_DS, S_FS, S_GS };
    int i;
    for(i = 0; i < 4; i++){
        Desc d; uint16_t sel = cpu.sreg[ss[i]];
        if(!(sel & ~3)) continue;
        if(!read_desc(sel, &d) || (!D_CONF(d) && D_DPL(d) < cpu.cpl)) set_null(ss[i], 0);
    }
}

/* the stack of level dpl from the TSS */
static void tss_stack(int dpl, uint16_t *ss, uint32_t *esp){
    if(cpu.tr_type & 8){
        if((uint32_t)dpl * 8 + 11 > cpu.tr_limit) fault(10, cpu.tr);
        *esp = cpu_ld32(cpu.tr_base + 4 + dpl*8); *ss = cpu_ld16(cpu.tr_base + 8 + dpl*8);
    } else {
        if((uint32_t)dpl * 4 + 5 > cpu.tr_limit) fault(10, cpu.tr);
        *esp = cpu_ld16(cpu.tr_base + 2 + dpl*4); *ss = cpu_ld16(cpu.tr_base + 4 + dpl*4);
    }
}

/* a push onto a stack that is not SS:ESP yet */
static void xpush(uint32_t base, int big, uint32_t *sp, int sz, uint32_t v){
    *sp -= (uint32_t)(sz / 8);
    if(!big) *sp &= 0xFFFF;
    if(sz == 32) cpu_st32(base + *sp, v); else cpu_st16(base + *sp, (uint16_t)v);
}
static void set_esp(uint32_t sp){ if(STK32) REG32(R_ESP) = sp; else REG16(R_ESP) = (uint16_t)sp; }

/* The flags a POPF or IRET may change: IOPL only at CPL 0, IF only at
 * CPL <= IOPL; VM never (IRET to V86 mode sets it itself). */
static void write_flags(uint32_t f, int sz){
    uint32_t mask = sz == 32 ? 0x57FD5u : 0x7FD5u;
    if(PE && (cpu.vm || cpu.cpl > 0)) mask &= ~0x3000u;
    if(PE && !cpu.vm && cpu.cpl > (int)cpu.iopl) mask &= ~0x200u;
    cpu_setflags((cpu_getflags() & ~mask) | (f & mask));
}

static void pm_interrupt(int n, int kind, int has_err, uint32_t err){
    Desc g, d, sd;
    uint16_t sel, nss = 0;
    uint32_t off, flags = cpu_getflags(), base, sp;
    int type, gsz, dpl, big, ext = kind != IK_SOFT, from_vm = (int)cpu.vm;
    if(kind == IK_SOFT && cpu.vm && cpu.iopl < 3) GP(0);
    if((uint32_t)n * 8 + 7 > cpu.idt_limit) GP(n*8 + 2 + ext);
    g.lo = cpu_ld32(cpu.idt_base + (uint32_t)n*8); g.hi = cpu_ld32(cpu.idt_base + (uint32_t)n*8 + 4);
    type = D_TYPE(g);
    if(!D_S(g) && type == 5){ pm_unsupported("a task gate in the IDT"); return; }
    if(D_S(g) || (type & 7) < 6) GP(n*8 + 2 + ext);
    if(kind == IK_SOFT && D_DPL(g) < cpu.cpl) GP(n*8 + 2);
    if(!D_P(g)) fault(11, n*8 + 2 + ext);
    gsz = (type & 8) ? 32 : 16;
    sel = (uint16_t)(g.lo >> 16);
    off = (g.lo & 0xFFFF) | (gsz == 32 ? (g.hi & 0xFFFF0000u) : 0);
    if(!(sel & ~3)) GP(ext);
    if(!read_desc(sel, &d) || !D_CODE(d) || D_DPL(d) > cpu.cpl) GP((sel & ~3) + ext);
    if(!D_P(d)) fault(11, (sel & ~3) + ext);
    dpl = D_CONF(d) ? cpu.cpl : D_DPL(d);
    if(from_vm && dpl != 0) GP((sel & ~3) + ext);
    if(dpl < cpu.cpl || from_vm){
        uint32_t nesp;
        tss_stack(dpl, &nss, &nesp);
        check_stack(nss, dpl, &sd, 10);
        base = D_BASE(sd); big = D_BIG(sd); sp = big ? nesp : nesp & 0xFFFF;
        if(from_vm){
            xpush(base, big, &sp, gsz, cpu.sreg[S_GS]); xpush(base, big, &sp, gsz, cpu.sreg[S_FS]);
            xpush(base, big, &sp, gsz, cpu.sreg[S_DS]); xpush(base, big, &sp, gsz, cpu.sreg[S_ES]);
        }
        xpush(base, big, &sp, gsz, cpu.sreg[S_SS]);
        xpush(base, big, &sp, gsz, REG32(R_ESP));
    } else {
        base = cpu.sbase[S_SS]; big = STK32; sp = sp_get();
    }
    xpush(base, big, &sp, gsz, flags);
    xpush(base, big, &sp, gsz, cpu.sreg[S_CS]);
    xpush(base, big, &sp, gsz, cpu.eip);
    if(has_err) xpush(base, big, &sp, gsz, err);
    /* nothing faults from here on */
    if(nss){
        seg_from_desc(S_SS, nss, sd);
        if(from_vm){ set_null(S_GS, 0); set_null(S_FS, 0); set_null(S_DS, 0); set_null(S_ES, 0); }
    }
    set_esp(sp);
    set_accessed(&d);
    set_cs(sel, d, dpl);
    cpu.eip = off;
    cpu.tf = 0; cpu.nt = 0; cpu.rf = 0; cpu.vm = 0;
    if(!(type & 1)) cpu.iflag = 0;              /* an interrupt gate, not a trap gate */
    cpu.halted = 0;
}

/* Deliver the exception fault() jumped out with; a fault while doing so is
 * a double fault, one while delivering that stops the run. */
static void deliver_fault(void){
    volatile int vec = fault_vec, has = fault_has_err, tries = 0;
    volatile uint32_t err = fault_err;
    restore_state();
    trc("[cpu] exception %02Xh error %04X at %04X:%08X\n", vec, (unsigned)err,
        cpu.sreg[S_CS], (unsigned)cpu.eip);
    if(FAULT_SETJMP(fault_jb)){
        restore_state();
        if(++tries > 1){
            fault_armed = 0;
            printf("[cpu] triple fault (exception %02Xh, then %02Xh) at %04X:%08X; stopped\n",
                   vec, fault_vec, cpu.sreg[S_CS], (unsigned)cpu.eip);
            cpu.shutdown = 1;
            return;
        }
        vec = 8; has = 1; err = 0;
    }
    fault_armed = 1;
    pm_interrupt(vec, IK_EXC, has, err);
    fault_armed = 0;
}

void cpu_interrupt(int n, int soft){
    uint32_t v;
    if(PE){
        if(fault_armed){ pm_interrupt(n, soft ? IK_SOFT : IK_HW, 0, 0); return; }
        save_state();
        if(FAULT_SETJMP(fault_jb)){ deliver_fault(); return; }
        fault_armed = 1;
        pm_interrupt(n, soft ? IK_SOFT : IK_HW, 0, 0);
        fault_armed = 0;
        return;
    }
    v = cpu_ld32(cpu.idt_base + (uint32_t)n*4);
    push16((uint16_t)cpu_getflags());
    push16(cpu.sreg[S_CS]);
    push16((uint16_t)cpu.eip);
    cpu.iflag = 0; cpu.tf = 0;
    set_sreg(S_CS, (uint16_t)(v>>16));
    cpu.eip = v & 0xFFFF;
    cpu.halted = 0;
}

/* a divide error: a fault under PE (EIP at the DIV), as the runner always
 * had it in real mode (EIP after it) */
static void div_err(void){ if(PE) fault(0, 0); else cpu_interrupt(0, 0); }

/* INT n, INT 3, INTO */
static void soft_int(int n){
    if(PE) pm_interrupt(n, IK_SOFT, 0, 0); else cpu_interrupt(n, 1);
}

/* the checks of a far JMP or CALL to a code segment (not through a gate) */
static void check_code_direct(uint16_t sel, Desc d){
    if(D_CONF(d) ? D_DPL(d) > cpu.cpl : ((sel & 3) > cpu.cpl || D_DPL(d) != cpu.cpl)) GP(sel & ~3);
    if(!D_P(d)) fault(11, sel & ~3);
}

/* a call gate's target: its code descriptor, the level it runs at */
static int gate_target(uint16_t sel, Desc g, Desc *d, uint16_t *tsel){
    if(D_DPL(g) < cpu.cpl || D_DPL(g) < (sel & 3)) GP(sel & ~3);
    if(!D_P(g)) fault(11, sel & ~3);
    *tsel = (uint16_t)(g.lo >> 16);
    if(!(*tsel & ~3)) GP(0);
    if(!read_desc(*tsel, d) || !D_CODE(*d) || D_DPL(*d) > cpu.cpl) GP(*tsel & ~3);
    if(!D_P(*d)) fault(11, *tsel & ~3);
    return D_CONF(*d) ? cpu.cpl : D_DPL(*d);
}

static void far_jmp(uint16_t sel, uint32_t off){
    Desc d, g; uint16_t tsel;
    if(!PE || cpu.vm){ set_sreg(S_CS, sel); cpu.eip = off & MASK(opsz); return; }
    if(!(sel & ~3)) GP(0);
    if(!read_desc(sel, &d)) GP(sel & ~3);
    if(D_CODE(d)){
        check_code_direct(sel, d);
        set_accessed(&d);
        set_cs(sel, d, cpu.cpl); cpu.eip = off & MASK(opsz);
        return;
    }
    if(D_S(d)) GP(sel & ~3);
    switch(D_TYPE(d)){
    case 4: case 12:
        g = d;
        if(gate_target(sel, g, &d, &tsel) != cpu.cpl && !D_CONF(d)) GP(tsel & ~3);
        set_cs(tsel, d, cpu.cpl);
        cpu.eip = (g.lo & 0xFFFF) | (D_TYPE(g) == 12 ? (g.hi & 0xFFFF0000u) : 0);
        return;
    case 1: case 5: case 9: pm_unsupported("a task switch (far JMP)"); return;
    default: GP(sel & ~3);
    }
}

void cpu_far_jump(uint16_t sel, uint32_t off){ opsz = 32; far_jmp(sel, off); }

static void far_call(uint16_t sel, uint32_t off){
    Desc d, g, sd; uint16_t tsel, nss; uint32_t nesp, base, sp, goff;
    int dpl, gsz, i, np;
    if(!PE || cpu.vm){
        pushv(cpu.sreg[S_CS]); pushv(cpu.eip);
        set_sreg(S_CS, sel); cpu.eip = off & MASK(opsz); return;
    }
    if(!(sel & ~3)) GP(0);
    if(!read_desc(sel, &d)) GP(sel & ~3);
    if(D_CODE(d)){
        check_code_direct(sel, d);
        base = cpu.sbase[S_SS]; sp = sp_get();
        xpush(base, STK32, &sp, opsz, cpu.sreg[S_CS]);
        xpush(base, STK32, &sp, opsz, cpu.eip);
        set_esp(sp);
        set_accessed(&d);
        set_cs(sel, d, cpu.cpl); cpu.eip = off & MASK(opsz);
        return;
    }
    if(D_S(d)) GP(sel & ~3);
    if(D_TYPE(d) == 1 || D_TYPE(d) == 5 || D_TYPE(d) == 9){ pm_unsupported("a task switch (far CALL)"); return; }
    if(D_TYPE(d) != 4 && D_TYPE(d) != 12) GP(sel & ~3);
    g = d;
    dpl = gate_target(sel, g, &d, &tsel);
    gsz = D_TYPE(g) == 12 ? 32 : 16;
    goff = (g.lo & 0xFFFF) | (gsz == 32 ? (g.hi & 0xFFFF0000u) : 0);
    if(dpl < cpu.cpl){
        /* to an inner level: its stack from the TSS, the parameters copied */
        uint32_t osp = sp_get(), obase = cpu.sbase[S_SS];
        int big;
        tss_stack(dpl, &nss, &nesp);
        check_stack(nss, dpl, &sd, 10);
        base = D_BASE(sd); big = D_BIG(sd); sp = big ? nesp : nesp & 0xFFFF;
        xpush(base, big, &sp, gsz, cpu.sreg[S_SS]);
        xpush(base, big, &sp, gsz, REG32(R_ESP));
        np = (int)(g.hi & 31);
        for(i = np - 1; i >= 0; i--){
            uint32_t a = obase + ((osp + (uint32_t)(i * gsz / 8)) & (STK32 ? 0xFFFFFFFFu : 0xFFFFu));
            xpush(base, big, &sp, gsz, gsz == 32 ? cpu_ld32(a) : cpu_ld16(a));
        }
        xpush(base, big, &sp, gsz, cpu.sreg[S_CS]);
        xpush(base, big, &sp, gsz, cpu.eip);
        seg_from_desc(S_SS, nss, sd);
        set_esp(sp);
    } else {
        base = cpu.sbase[S_SS]; sp = sp_get();
        xpush(base, STK32, &sp, gsz, cpu.sreg[S_CS]);
        xpush(base, STK32, &sp, gsz, cpu.eip);
        set_esp(sp);
    }
    set_cs(tsel, d, dpl);
    cpu.eip = goff;
}

/* the checks of the code segment a RETF or IRET goes back to */
static void check_code_return(uint16_t sel, Desc *d){
    int rpl = sel & 3;
    if(!(sel & ~3)) GP(0);
    if(rpl < cpu.cpl) GP(sel & ~3);
    if(!read_desc(sel, d) || !D_CODE(*d)) GP(sel & ~3);
    if(D_CONF(*d) ? D_DPL(*d) > rpl : D_DPL(*d) != rpl) GP(sel & ~3);
    if(!D_P(*d)) fault(11, sel & ~3);
}

/* to an outer level: SS:ESP from the stack at sp */
static void return_outer(uint32_t sp, int w, uint16_t cs, Desc d, uint32_t eip, int rpl){
    Desc sd;
    uint32_t base = cpu.sbase[S_SS], m = STK32 ? 0xFFFFFFFFu : 0xFFFFu;
    uint32_t nesp = w == 4 ? cpu_ld32(base + (sp & m)) : cpu_ld16(base + (sp & m));
    uint16_t nss = cpu_ld16(base + ((sp + (uint32_t)w) & m));
    check_stack(nss, rpl, &sd, 13);
    set_cs(cs, d, rpl);
    cpu.eip = eip;
    seg_from_desc(S_SS, nss, sd);
    set_esp(nesp);
    null_inner_segs();
}

static void far_ret(uint32_t n){
    uint32_t sp, base, m, eip;
    uint16_t cs;
    int w = opsz / 8;
    Desc d;
    if(!PE || cpu.vm){
        uint32_t o = popv(); uint16_t s = (uint16_t)popv();
        cpu.eip = o & MASK(opsz); set_sreg(S_CS, s); sp_add(n); return;
    }
    base = cpu.sbase[S_SS]; m = STK32 ? 0xFFFFFFFFu : 0xFFFFu; sp = sp_get();
    eip = w == 4 ? cpu_ld32(base + sp) : cpu_ld16(base + sp);
    cs = cpu_ld16(base + ((sp + (uint32_t)w) & m));
    check_code_return(cs, &d);
    if((cs & 3) == cpu.cpl){
        set_cs(cs, d, cpu.cpl); cpu.eip = eip;
        sp_add((uint32_t)(2 * w) + n);
        return;
    }
    return_outer(sp + (uint32_t)(2 * w) + n, w, cs, d, eip, cs & 3);
    sp_add(n);
}

static void iret_(void){
    uint32_t sp, base, m, eip, fl;
    uint16_t cs;
    int w = opsz / 8;
    Desc d;
    if(!PE || cpu.vm){
        if(cpu.vm && cpu.iopl < 3) GP(0);
        eip = popv(); cs = (uint16_t)popv(); fl = popv();
        cpu.eip = eip & MASK(opsz); set_sreg(S_CS, cs); write_flags(fl, opsz);
        return;
    }
    if(cpu.nt){ pm_unsupported("a task return (IRET with NT)"); return; }
    base = cpu.sbase[S_SS]; m = STK32 ? 0xFFFFFFFFu : 0xFFFFu; sp = sp_get();
#define STK(i) (w == 4 ? cpu_ld32(base + ((sp + (uint32_t)(i)*4) & m)) : cpu_ld16(base + ((sp + (uint32_t)(i)*2) & m)))
    eip = STK(0); cs = (uint16_t)STK(1); fl = STK(2);
    if(w == 4 && (fl & 0x20000) && cpu.cpl == 0){
        /* back to V86 mode: ESP, SS, ES, DS, FS, GS follow */
        uint32_t nesp = STK(3);
        uint16_t v[5]; int i;
        for(i = 0; i < 5; i++) v[i] = (uint16_t)STK(4 + i);
        cpu_setflags(fl); cpu.vm = 1; cpu.cpl = 3;
        set_sreg(S_CS, cs); set_sreg(S_SS, v[0]); set_sreg(S_ES, v[1]);
        set_sreg(S_DS, v[2]); set_sreg(S_FS, v[3]); set_sreg(S_GS, v[4]);
        REG32(R_ESP) = nesp;
        cpu.eip = eip & 0xFFFF;
        return;
    }
    check_code_return(cs, &d);
    if((cs & 3) == cpu.cpl){
        set_cs(cs, d, cpu.cpl); cpu.eip = eip & MASK(opsz);
        sp_add((uint32_t)(3 * w));
        write_flags(fl, opsz);
        return;
    }
    {   /* the flags as the old level may write them */
        int old = cpu.cpl;
        return_outer(sp + (uint32_t)(3 * w), w, cs, d, eip & MASK(opsz), cs & 3);
        cpu.cpl = old; write_flags(fl, opsz); cpu.cpl = cs & 3;
    }
#undef STK
}

/* IN, OUT, INS, OUTS: the I/O permission bitmap when CPL > IOPL or in V86 mode */
static void io_check(uint16_t port, int bytes){
    uint32_t off, bits;
    if(!PE || (!cpu.vm && cpu.cpl <= (int)cpu.iopl)) return;
    if(!(cpu.tr_type & 8) || cpu.tr_limit < 0x67) GP(0);
    off = cpu_ld16(cpu.tr_base + 0x66) + (uint32_t)(port >> 3);
    if(off + 1 > cpu.tr_limit) GP(0);
    bits = (uint32_t)cpu_ld16(cpu.tr_base + off) >> (port & 7);
    if(bits & ((1u << bytes) - 1)) GP(0);
}

/* CLI, STI (and PUSHF, POPF, INT, IRET in V86 mode): IOPL-sensitive */
static void iopl_check(void){
    if(PE && (cpu.vm ? cpu.iopl < 3 : cpu.cpl > (int)cpu.iopl)) GP(0);
}
/* LGDT, MOV CRn, HLT, ...: CPL 0 only */
static void cpl0_check(void){
    if(PE && (cpu.vm || cpu.cpl)) GP(0);
}

/* ------------------------------------------------------------- string    */
static void strop(int op, int sz){
    int step = (sz/8) * (cpu.df ? -1 : 1);
    uint32_t dsb = sb(S_DS), esb = cpu.sbase[S_ES];
    uint32_t cnt = 1, i;
    int use_rep = rep != 0;
    if(use_rep){ cnt = adsz==16 ? REG16(R_ECX) : REG32(R_ECX); if(cnt==0) return;
        /* Bulk fast path: REP MOVS/STOS forward over plain RAM.  The game
         * uses these for asset copies and buffer clears; the per-byte loop
         * below costs a full decode's worth of branches per byte.  Anything
         * touching VGA/ROM, wrapping the segment, or running backwards keeps
         * the exact slow path.
         *
         * So does a forward REP MOVS whose destination lies inside its own
         * source run, and that exclusion is not a detail - it is the whole
         * difference between this path and the instruction it stands in for.
         * x86 copies one element at a time, in order, so when DI is within
         * CX elements ahead of SI the bytes it just wrote are what it reads
         * next and the run propagates.  That is not a pathological case to
         * be tolerated: it is the LZ77 run-expansion idiom (`mov si,di; sub
         * si,dist; rep movsb` with a length greater than the distance), and
         * every self-extracting executable uses it.  memmove is specified to
         * do the opposite - it copies as if through a temporary, so the
         * source is the *old* contents - and an earlier comment here claimed
         * it "covers overlap", which is backwards.  A PKLITE-packed driver
         * unpacked through it came out subtly wrong: `02 AC 01` where
         * `EC A8 01` (in al,dx / test al,1) belonged, a calibration loop
         * that could never see the port it was counting.  The slow loop
         * below is byte-exact, so overlap simply goes there. */
        if((op == 0 || op == 2) && !cpu.df && cnt >= 16){
            int el = sz / 8;
            uint64_t n = ((uint64_t)cnt - 1u) * (uint64_t)el;
            int ok = 0;
            uint32_t s0 = 0, d0 = 0;
            if(adsz == 16){
                uint32_t soff = REG16(R_ESI), doff = REG16(R_EDI);
                if((uint64_t)soff + n <= 0xFFFFu && (uint64_t)doff + n <= 0xFFFFu &&
                   (uint64_t)dsb + soff + n < 0xA0000u && (uint64_t)esb + doff + n < 0xA0000u){
                    s0 = dsb + soff; d0 = esb + doff; ok = 1;
                }
            } else {
                uint64_t soff = REG32(R_ESI), doff = REG32(R_EDI);
                if(soff + n < 0xA0000u && doff + n < 0xA0000u &&
                   (uint64_t)dsb + soff + n < 0xA0000u && (uint64_t)esb + doff + n < 0xA0000u){
                    s0 = (uint32_t)(dsb + soff); d0 = (uint32_t)(esb + doff); ok = 1;
                }
            }
            /* Destination inside the source run: propagating copy, slow path. */
            if(ok && op == 0 && d0 > s0 &&
               (uint64_t)(d0 - s0) < (uint64_t)cnt * (uint64_t)el) ok = 0;
            if(ok){
                size_t len = (size_t)cnt * (size_t)el;
                /* This path writes ram[] directly, so -memwatch would miss a
                 * block store that happens to cover the watched byte - and a
                 * watch that can silently miss its writer is worse than none. */
                extern uint32_t insn_ip;
                if(memwatch_addr >= d0 && memwatch_addr < d0 + len)
                    printf("[memw] %05X covered by a %s of %u bytes at %05X"
                           "  t=%.6f  by %04X:%04X\n",
                           (unsigned)memwatch_addr, op == 0 ? "REP MOVS" : "REP STOS",
                           (unsigned)len, (unsigned)d0, emu_now(),
                           cpu.sreg[S_CS], (unsigned)insn_ip);
                if(op == 0) memmove(&ram[d0], &ram[s0], len);
                else if(sz == 8) memset(&ram[d0], REG8(0), len);
                else if(sz == 16){
                    uint16_t v = REG16(0); uint32_t d;
                    for(d = 0; d < cnt; d++){ ram[d0 + d*2] = (uint8_t)v; ram[d0 + d*2 + 1] = (uint8_t)(v >> 8); }
                } else {
                    uint32_t v = REG32(0); uint32_t d;
                    for(d = 0; d < cnt; d++){
                        ram[d0 + d*4] = (uint8_t)v; ram[d0 + d*4 + 1] = (uint8_t)(v >> 8);
                        ram[d0 + d*4 + 2] = (uint8_t)(v >> 16); ram[d0 + d*4 + 3] = (uint8_t)(v >> 24);
                    }
                }
                if(adsz == 16){
                    REG16(R_ESI) += (uint16_t)len; REG16(R_EDI) += (uint16_t)len;
                    REG16(R_ECX) = 0;
                } else {
                    REG32(R_ESI) += (uint32_t)len; REG32(R_EDI) += (uint32_t)len;
                    REG32(R_ECX) = 0;
                }
                cpu.cycles += cnt;
                return;
            }
        }
    }
    for(i=0;i<cnt;i++){
        uint32_t s = adsz==16 ? REG16(R_ESI) : REG32(R_ESI);
        uint32_t d = adsz==16 ? REG16(R_EDI) : REG32(R_EDI);
        uint32_t a,b;
        switch(op){
        case 0:
            a = sz==8?cpu_ld8(dsb+s): sz==16?cpu_ld16(dsb+s):cpu_ld32(dsb+s);
            if(sz==8) cpu_st8(esb+d,(uint8_t)a); else if(sz==16) cpu_st16(esb+d,(uint16_t)a); else cpu_st32(esb+d,a);
            break;
        case 1:
            a = sz==8?cpu_ld8(dsb+s): sz==16?cpu_ld16(dsb+s):cpu_ld32(dsb+s);
            b = sz==8?cpu_ld8(esb+d): sz==16?cpu_ld16(esb+d):cpu_ld32(esb+d);
            alu(7,a,b,sz); break;
        case 2:
            a = sz==8?REG8(0): sz==16?REG16(0):REG32(0);
            if(sz==8) cpu_st8(esb+d,(uint8_t)a); else if(sz==16) cpu_st16(esb+d,(uint16_t)a); else cpu_st32(esb+d,a);
            break;
        case 3:
            a = sz==8?cpu_ld8(dsb+s): sz==16?cpu_ld16(dsb+s):cpu_ld32(dsb+s);
            if(sz==8) REG8(0)=(uint8_t)a; else if(sz==16) REG16(0)=(uint16_t)a; else REG32(0)=a;
            break;
        case 4:
            a = sz==8?REG8(0): sz==16?REG16(0):REG32(0);
            b = sz==8?cpu_ld8(esb+d): sz==16?cpu_ld16(esb+d):cpu_ld32(esb+d);
            alu(7,a,b,sz); break;
        case 5:
            a = sz==8?io_r8(REG16(R_EDX)):io_r16(REG16(R_EDX));
            if(sz==8) cpu_st8(esb+d,(uint8_t)a); else cpu_st16(esb+d,(uint16_t)a);
            break;
        case 6:
            a = sz==8?cpu_ld8(dsb+s): cpu_ld16(dsb+s);
            if(sz==8) io_w8(REG16(R_EDX),(uint8_t)a); else io_w16(REG16(R_EDX),(uint16_t)a);
            break;
        }
        if(op==0||op==1||op==3||op==6){ if(adsz==16) REG16(R_ESI)+=(uint16_t)step; else REG32(R_ESI)+=step; }
        if(op==0||op==1||op==2||op==4||op==5){ if(adsz==16) REG16(R_EDI)+=(uint16_t)step; else REG32(R_EDI)+=step; }
        if(use_rep){
            if(adsz==16) REG16(R_ECX)--; else REG32(R_ECX)--;
            if(op==1||op==4){ if(rep==2 && !cpu.zf) break; if(rep==1 && cpu.zf) break; }
        }
    }
    /* one cycle an element done: REPE/REPNE may stop long before ECX runs
     * out (a string's length with ECX = -1 would be 4 billion) */
    cpu.cycles += i < cnt ? i + 1 : cnt;
}

/* ---------------------------------------------------- emulator callbacks */
void (*cb_table[256])(void);
void cpu_no_iret(void){ no_iret = 1; }

/* 0F FF id: a BIOS or DOS service in C, run from its stub in real or V86
 * mode; it returns through the IRET frame unless it says otherwise.  One
 * (INT 15h AH=89h) enters protected mode, so a fault is armed around it. */
static void run_callback(uint8_t id){
    int armed = fault_armed;
    no_iret = 0;
    if(!armed){
        save_state();
        if(FAULT_SETJMP(fault_jb)){ deliver_fault(); return; }
        fault_armed = 1;
    }
    if(cb_table[id]) cb_table[id]();
    if(!no_iret){ uint16_t ip=pop16(), c=pop16(), f=pop16();
                  cpu.eip=ip; set_sreg(S_CS,c); write_flags(f, 16); }
    fault_armed = armed;
}

/* LDS, LES, LSS, LFS, LGS: the offset (16 or 32 bits), then the selector */
static void load_far(int s){
    uint32_t o;
    if(ea_isreg){ cpu_undef("LDS/LES with a register"); return; }
    o = opsz == 32 ? cpu_ld32(ea) : cpu_ld16(ea);
    set_sreg(s, cpu_ld16(ea + (uint32_t)opsz/8));
    if(s == S_SS) cpu.inhibit = 1;
    wrG(opsz, o);
}

/* 0F 00: SLDT, STR, LLDT, LTR, VERR, VERW (protected mode only) */
static void group6(void){
    Desc d; uint16_t sel;
    modrm();
    if(!PE || cpu.vm){ cpu_undef("0F 00 outside protected mode"); return; }
    switch(reg_){
    case 0: wrE(ea_isreg ? opsz : 16, cpu.ldtr); break;
    case 1: wrE(ea_isreg ? opsz : 16, cpu.tr); break;
    case 2: cpl0_check(); sel = (uint16_t)rdE(16);
        if(!(sel & ~3)){ cpu.ldtr = 0; cpu.ldt_base = 0; cpu.ldt_limit = 0; break; }
        if((sel & 4) || !read_desc(sel, &d) || D_S(d) || D_TYPE(d) != 2) GP(sel & ~3);
        if(!D_P(d)) fault(11, sel & ~3);
        cpu.ldtr = sel; cpu.ldt_base = D_BASE(d); cpu.ldt_limit = d_limit(d);
        break;
    case 3: cpl0_check(); sel = (uint16_t)rdE(16);
        if(!(sel & ~3)) GP(0);
        if((sel & 4) || !read_desc(sel, &d) || D_S(d) || (D_TYPE(d) != 1 && D_TYPE(d) != 9)) GP(sel & ~3);
        if(!D_P(d)) fault(11, sel & ~3);
        d.hi |= 0x200; cpu_st32(desc_addr + 4, d.hi);            /* busy */
        cpu.tr = sel; cpu.tr_base = D_BASE(d); cpu.tr_limit = d_limit(d); cpu.tr_type = D_TYPE(d);
        break;
    case 4: case 5: {
        int ok;
        sel = (uint16_t)rdE(16);
        ok = (sel & ~3) && read_desc(sel, &d) && D_S(d);
        if(ok && !D_CONF(d) && (D_DPL(d) < cpu.cpl || D_DPL(d) < (sel & 3))) ok = 0;
        if(ok) ok = reg_ == 4 ? (!D_CODE(d) || (D_TYPE(d) & 2)) : D_WDATA(d);
        cpu.zf = (uint32_t)ok; break; }
    default: cpu_undef("0F 00 /6, /7"); break;
    }
}

/* 0F 01: SGDT, SIDT, LGDT, LIDT, SMSW, LMSW, INVLPG */
static void group7(void){
    uint32_t base;
    modrm();
    switch(reg_){
    case 0: case 1:
        cpu_st16(ea, (uint16_t)(reg_ ? cpu.idt_limit : cpu.gdt_limit));
        base = reg_ ? cpu.idt_base : cpu.gdt_base;
        cpu_st32(ea + 2, opsz == 32 ? base : (base & 0xFFFFFF) | 0xFF000000u);  /* the 386 stores FFh */
        break;
    case 2: case 3:
        cpl0_check();
        base = cpu_ld32(ea + 2);
        if(opsz == 16) base &= 0xFFFFFF;
        if(reg_ == 2){ cpu.gdt_limit = cpu_ld16(ea); cpu.gdt_base = base; }
        else         { cpu.idt_limit = cpu_ld16(ea); cpu.idt_base = base; }
        break;
    case 4: wrE(ea_isreg ? opsz : 16, cpu.cr0); break;
    case 6: cpl0_check();              /* sets PE, never clears it */
        cpu.cr0 = (cpu.cr0 & ~0xEu) | (cpu.cr0 & 1) | (rdE(16) & 0xF); break;
    case 7: cpl0_check(); break;
    default: cpu_undef("0F 01 /5"); break;
    }
}

/* 0F 02 LAR, 0F 03 LSL */
static void lar_lsl(int lsl){
    Desc d; uint16_t sel; int ok;
    modrm();
    if(!PE || cpu.vm){ cpu_undef("LAR/LSL outside protected mode"); return; }
    sel = (uint16_t)rdE(16);
    ok = (sel & ~3) && read_desc(sel, &d);
    if(ok && !D_S(d)){                   /* the system types each may read */
        int t = D_TYPE(d);
        ok = t == 1 || t == 2 || t == 3 || t == 9 || t == 11 ||
             (!lsl && (t == 4 || t == 5 || t == 12));
    }
    if(ok && !D_CONF(d) && (D_DPL(d) < cpu.cpl || D_DPL(d) < (sel & 3))) ok = 0;
    cpu.zf = (uint32_t)ok;
    if(ok) wrG(opsz, lsl ? d_limit(d) : (d.hi & (opsz == 32 ? 0x00FFFF00u : 0xFF00u)));
}

static void mov_cr(int to_cr){
    modrm();
    cpl0_check();
    if(!to_cr){
        REG32(rm_) = reg_ == 0 ? cpu.cr0 : reg_ == 2 ? cpu.cr2 : reg_ == 3 ? cpu.cr3 : 0;
        return;
    }
    switch(reg_){
    case 0: {
        uint32_t v = REG32(rm_) | 0x10;             /* ET stays set on a 387-less 386 too */
        if(v & 0x80000000u){ pm_unsupported("paging (CR0.PG)"); return; }
        if(!(v & 1) && PE) cpu.cpl = 0;
        cpu.cr0 = v;
        break; }
    case 2: cpu.cr2 = REG32(rm_); break;
    case 3: cpu.cr3 = REG32(rm_); break;
    default: cpu_undef("MOV CR4 or higher"); break;
    }
}

static void op0f(void){
    uint8_t op = fetch8();
    uint32_t a,b,r; int sz = opsz;
    switch(op){
    case 0x00: group6(); break;
    case 0x01: group7(); break;
    case 0x02: lar_lsl(0); break;
    case 0x03: lar_lsl(1); break;
    case 0x06: cpl0_check(); cpu.cr0 &= ~8u; break;          /* CLTS */
    case 0x08: case 0x09: cpl0_check(); break;               /* INVD, WBINVD */
    case 0x20: mov_cr(0); break;
    case 0x22: mov_cr(1); break;
    case 0x21: modrm(); cpl0_check(); REG32(rm_) = 0; break;  /* debug registers: none */
    case 0x23: modrm(); cpl0_check(); break;
    case 0x31: REG32(R_EAX)=(uint32_t)cpu.cycles; REG32(R_EDX)=(uint32_t)(cpu.cycles>>32); break;
    case 0xA2: REG32(R_EAX)=0; REG32(R_EBX)=0; REG32(R_ECX)=0; REG32(R_EDX)=0; break;
    case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
    case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F: {
        int32_t d = opsz==32 ? (int32_t)fetch32() : (int32_t)(int16_t)fetch16();
        if(cond(op&15)) cpu.eip = (cpu.eip + (uint32_t)d) & MASK(opsz);
        break; }
    case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
    case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F:
        modrm(); wrE(8, cond(op&15) ? 1 : 0); break;
    case 0xA0: pushv(cpu.sreg[S_FS]); break;
    case 0xA1: { uint16_t v = (uint16_t)cpu_ld16(cpu.sbase[S_SS] + sp_get()); set_sreg(S_FS, v); sp_add((uint32_t)opsz/8); break; }
    case 0xA8: pushv(cpu.sreg[S_GS]); break;
    case 0xA9: { uint16_t v = (uint16_t)cpu_ld16(cpu.sbase[S_SS] + sp_get()); set_sreg(S_GS, v); sp_add((uint32_t)opsz/8); break; }
    case 0xB2: modrm(); load_far(S_SS); break;
    case 0xB4: modrm(); load_far(S_FS); break;
    case 0xB5: modrm(); load_far(S_GS); break;
    case 0xB6: modrm(); wrG(sz, (uint8_t)rdE(8)); break;
    case 0xB7: modrm(); wrG(sz, (uint16_t)rdE(16)); break;
    case 0xBE: modrm(); wrG(sz, (uint32_t)(int32_t)(int8_t)rdE(8)); break;
    case 0xBF: modrm(); wrG(sz, (uint32_t)(int32_t)(int16_t)rdE(16)); break;
    case 0xAF: modrm();
        if(sz==16){ int32_t p=(int32_t)(int16_t)rdG(16)*(int16_t)rdE(16); wrG(16,(uint16_t)p);
                    cpu.cf=cpu.of=((int32_t)(int16_t)p != p); }
        else { int64_t p=(int64_t)(int32_t)rdG(32)*(int32_t)rdE(32); wrG(32,(uint32_t)p);
               cpu.cf=cpu.of=((int64_t)(int32_t)p != p); }
        break;
    case 0xA3: case 0xAB: case 0xB3: case 0xBB: {
        int32_t bit; modrm(); bit = (int32_t)rdG(sz);
        if(sz==16) bit = (int16_t)bit;
        if(!ea_isreg){ ea += (uint32_t)((bit >> (sz==32?5:4)) * (sz/8)); }
        bit &= (sz-1);
        a = rdE(sz); cpu.cf = (a>>bit)&1;
        if(op==0xAB) a |= (1u<<bit); else if(op==0xB3) a &= ~(1u<<bit); else if(op==0xBB) a ^= (1u<<bit);
        if(op!=0xA3) wrE(sz,a);
        break; }
    case 0xBA: { int bit; modrm(); bit = fetch8() & (sz-1);
        a = rdE(sz); cpu.cf = (a>>bit)&1;
        if(reg_==5) a |= (1u<<bit); else if(reg_==6) a &= ~(1u<<bit); else if(reg_==7) a ^= (1u<<bit);
        if(reg_!=4) wrE(sz,a);
        break; }
    case 0xBC: { modrm(); a = rdE(sz); if(!a){cpu.zf=1;} else { int i=0; cpu.zf=0; while(!((a>>i)&1)) i++; wrG(sz,(uint32_t)i);} break; }
    case 0xBD: { modrm(); a = rdE(sz); if(!a){cpu.zf=1;} else { int i=sz-1; cpu.zf=0; while(!((a>>i)&1)) i--; wrG(sz,(uint32_t)i);} break; }
    case 0xA4: case 0xAC: case 0xA5: case 0xAD: {
        int c; int left = (op==0xA4||op==0xA5);
        modrm();
        if(op==0xA4||op==0xAC) c = fetch8() & 31; else c = REG8(1) & 31;
        a = rdE(sz); b = rdG(sz);
        if(c){
            if(left){ cpu.cf=(a>>(sz-c))&1; r = (a<<c)|(b>>(sz-c)); }
            else    { cpu.cf=(a>>(c-1))&1;  r = (a>>c)|(b<<(sz-c)); }
            r &= MASK(sz); wrE(sz,r); cpu.zf=r==0; cpu.sf=(r>>(sz-1))&1; cpu.pf=ptab[r&0xFF];
        }
        break; }
    case 0xC8: case 0xC9: case 0xCA: case 0xCB: case 0xCC: case 0xCD: case 0xCE: case 0xCF: {
        int i = op&7; uint32_t v = REG32(i);
        REG32(i) = (v>>24)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|(v<<24); break; }
    case 0xFF: run_callback(fetch8()); break;
    default: {
        char w[32];
        snprintf(w, sizeof(w), "unhandled 0F %02X", op);
        cpu_undef(w);
        break; }
    }
}

/* Report an instruction this CPU does not implement.
 *
 * Two things this has to get right, learned the hard way from a 118 MB log
 * that was 3.2 million copies of one line.  An undecoded opcode is almost
 * never a one-off: either the guest loops over it forever, or the decoder has
 * drifted mid-instruction and every following byte is "unhandled" too.  So
 * each distinct site is reported once, the whole thing stops after a cap, and
 * what gets printed is the bytes - a bare opcode byte and a CS:IP say nothing
 * about whether this is a real instruction or data being executed, and the
 * bytes say both.  insn_ip is the start of the instruction, not wherever the
 * decoder had got to, so the dump can be pasted straight into a disassembler.
 *
 * Note this reports; it does not fault.  Execution continues past the bytes
 * the decoder consumed, exactly as it did before - the point is to find out
 * what they were.
 *
 * -undefdump goes further and writes the whole code segment out once, the
 * first time this fires.  Sixteen bytes tell you whether the decoder drifted;
 * they do not tell you where it left real code, and when the program that got
 * there arrived compressed on disk there is no file to disassemble instead.  The guest's own memory is the
 * only copy of what is actually executing. */
uint32_t insn_ip;   /* also read by -memwatch in vga.c, to name the writer */
#define UNDEF_SITES 32
#define UNDEF_MAX   64
static uint32_t undef_site[UNDEF_SITES];
static int undef_nsite = 0, undef_n = 0;
uint32_t memwatch_addr = 0xFFFFFFFFu;
#define MEMWATCH_HEAD 32                 /* printed live, as they happen */
#define MEMWATCH_TAIL 32                 /* kept, so the *end* is visible too */
static int memwatch_left = MEMWATCH_HEAD;
unsigned long memwatch_n = 0;
/* A watch that only shows a sequence's first N answers "did this ever happen"
 * and not "when did it stop", which is the question whenever a guest was
 * working and then wasn't.  So keep the last few as well. */
static struct { double t; uint32_t a, ip; uint8_t v, old; uint16_t cs; }
    mw_tail[MEMWATCH_TAIL];
static unsigned mw_tail_pos = 0;

void memwatch_hit(uint32_t a, uint8_t v){
    unsigned s = mw_tail_pos & (MEMWATCH_TAIL - 1);
    memwatch_n++;
    mw_tail[s].t = emu_now(); mw_tail[s].a = a;
    mw_tail[s].v = v;         mw_tail[s].old = ram[a];
    mw_tail[s].cs = cpu.sreg[S_CS]; mw_tail[s].ip = insn_ip;
    mw_tail_pos++;
    if(memwatch_left <= 0) return;
    memwatch_left--;
    printf("[memw] %05X <- %02X (was %02X)  t=%.6f  by %04X:%04X\n",
           (unsigned)a, v, ram[a], emu_now(),
           cpu.sreg[S_CS], (unsigned)insn_ip);
}

void memwatch_report(void){
    unsigned n, i, first;
    if(memwatch_addr == 0xFFFFFFFFu) return;
    n = memwatch_n < MEMWATCH_TAIL ? (unsigned)memwatch_n : MEMWATCH_TAIL;
    if(memwatch_n > MEMWATCH_HEAD){
        printf("[memw] last %u writes:\n", n);
        first = (mw_tail_pos - n) & (MEMWATCH_TAIL - 1);
        for(i = 0; i < n; i++){
            unsigned s = (first + i) & (MEMWATCH_TAIL - 1);
            printf("[memw]   %05X <- %02X (was %02X)  t=%.6f  by %04X:%04X\n",
                   (unsigned)mw_tail[s].a, mw_tail[s].v, mw_tail[s].old,
                   mw_tail[s].t, mw_tail[s].cs, (unsigned)mw_tail[s].ip);
        }
    }
    printf("[memw] %05X written %lu times total\n",
           (unsigned)memwatch_addr, memwatch_n);
}

/* -rwatch ADDR LEN: which instructions read these bytes.  A table polled
 * every frame is read millions of times, so the report is a count per
 * reader (CS:EIP and the first byte it read), not a line per read.  Data
 * reads through the CPU only: not instruction fetches, not the fast REP
 * MOVS path below A0000h, not the BIOS's or DOS's own reads. */
uint32_t rwatch_lo = 0, rwatch_hi = 0;
#define RWATCH_MAX 64
static struct { uint16_t cs; uint32_t ip, a; unsigned long n; double t0, t1; } rw[RWATCH_MAX];
static int rw_n = 0;
static unsigned long rw_lost = 0;

void rwatch_hit(uint32_t a, unsigned n){
    int i;
    if(a < rwatch_lo) a = rwatch_lo;    /* a word read that starts before the range */
    (void)n;
    for(i = 0; i < rw_n; i++)
        if(rw[i].ip == insn_ip && rw[i].cs == cpu.sreg[S_CS] && rw[i].a == a){
            rw[i].n++; rw[i].t1 = emu_now(); return;
        }
    if(rw_n == RWATCH_MAX){ rw_lost++; return; }
    rw[rw_n].cs = cpu.sreg[S_CS]; rw[rw_n].ip = insn_ip; rw[rw_n].a = a;
    rw[rw_n].n = 1; rw[rw_n].t0 = rw[rw_n].t1 = emu_now();
    rw_n++;
}

void rwatch_report(void){
    int i;
    if(!rwatch_hi) return;
    for(i = 0; i < rw_n; i++)
        printf("[memr] %05X+%02X read by %04X:%04X  %lu times  t=%.6f..%.6f\n",
               (unsigned)rwatch_lo, (unsigned)(rw[i].a - rwatch_lo), rw[i].cs,
               (unsigned)rw[i].ip, rw[i].n, rw[i].t0, rw[i].t1);
    if(rw_lost) printf("[memr] %lu reads by further readers not kept\n", rw_lost);
    printf("[memr] %05X..%05X read by %d readers\n",
           (unsigned)rwatch_lo, (unsigned)(rwatch_hi - 1), rw_n);
}

/* -prof: where the emulated CPU actually goes.
 *
 * "This loop is too slow" and "something else is eating the budget" look the
 * same from a counter of how often the loop ran, and reasoning about which it
 * is from instruction counts per iteration is exactly the guess this project
 * keeps getting wrong.  A sampling profiler answers it directly: every 4096th
 * instruction, note CS:IP; at exit, print the hottest sites with their share.
 * One predictable branch per instruction when off, and no allocation. */
int prof_on = 0;
#define PROF_N 2048
static uint32_t prof_key[PROF_N];
static unsigned long prof_hit[PROF_N];
static unsigned long prof_total = 0, prof_lost = 0;
static unsigned prof_tick = 0;
/* The per-instruction table answers "which instruction", but a flat profile -
 * every entry within a tenth of a percent of the next, which is what a big
 * unrolled loop body looks like - answers nothing.  The same samples bucketed
 * by 1K of linear address answer the question that actually matters first:
 * which *code* is running at all.  A guest that has stopped making progress
 * shows up here as one or two regions holding everything, and the regions map
 * straight onto the DOS allocations in the -t log. */
#define PROF_RGN_SHIFT 10
#define PROF_RGN_N     1104            /* (FFFF<<4)+FFFF, in 1K units */
static unsigned long prof_region[PROF_RGN_N];

static void prof_sample(void){
    uint32_t key = ((uint32_t)cpu.sreg[S_CS] << 16) | (cpu.eip & 0xFFFF);
    unsigned h = (unsigned)((key * 2654435761u) >> 21) & (PROF_N - 1);
    unsigned i;
    uint32_t lin = (cs_base + cpu.eip) >> PROF_RGN_SHIFT;
    prof_total++;
    if(lin < PROF_RGN_N) prof_region[lin]++;
    for(i = 0; i < 64; i++){
        unsigned s = (h + i) & (PROF_N - 1);
        if(prof_hit[s] == 0){ prof_key[s] = key; prof_hit[s] = 1; return; }
        if(prof_key[s] == key){ prof_hit[s]++; return; }
    }
    prof_lost++;
}

void prof_report(void){
    unsigned i, j, n = 0;
    unsigned idx[24];
    if(!prof_on) return;
    printf("[prof] %lu samples (1 per 4096 instructions), %lu unrecorded\n",
           prof_total, prof_lost);
    for(i = 0; i < PROF_N; i++){
        if(!prof_hit[i]) continue;
        if(n < 24){ idx[n++] = i; }
        else {
            unsigned lo = 0;
            for(j = 1; j < n; j++) if(prof_hit[idx[j]] < prof_hit[idx[lo]]) lo = j;
            if(prof_hit[i] > prof_hit[idx[lo]]) idx[lo] = i;
        }
    }
    for(i = 0; i < n; i++)
        for(j = i + 1; j < n; j++)
            if(prof_hit[idx[j]] > prof_hit[idx[i]]){ unsigned t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    for(i = 0; i < n; i++){
        uint32_t k = prof_key[idx[i]];
        printf("[prof]   %04X:%04X  lin=%05X  %8lu  %5.2f%%\n",
               k >> 16, k & 0xFFFF,
               ((k >> 16) << 4) + (k & 0xFFFF), prof_hit[idx[i]],
               prof_total ? 100.0 * (double)prof_hit[idx[i]] / (double)prof_total : 0.0);
    }
    {   /* ...and the same samples by 1K region, which is the view that shows
         * whether the guest is still running its program or only its ISRs. */
        unsigned r, q, top[16], tn = 0;
        for(r = 0; r < PROF_RGN_N; r++){
            if(!prof_region[r]) continue;
            if(tn < 16) top[tn++] = r;
            else {
                unsigned lo = 0;
                for(q = 1; q < tn; q++)
                    if(prof_region[top[q]] < prof_region[top[lo]]) lo = q;
                if(prof_region[r] > prof_region[top[lo]]) top[lo] = r;
            }
        }
        for(r = 0; r < tn; r++)
            for(q = r + 1; q < tn; q++)
                if(prof_region[top[q]] > prof_region[top[r]]){
                    unsigned t = top[r]; top[r] = top[q]; top[q] = t; }
        printf("[prof] by 1K region:\n");
        for(r = 0; r < tn; r++)
            printf("[prof]   lin %05X-%05X  %8lu  %5.2f%%\n",
                   top[r] << PROF_RGN_SHIFT,
                   (top[r] << PROF_RGN_SHIFT) + (1u << PROF_RGN_SHIFT) - 1,
                   prof_region[top[r]],
                   prof_total ? 100.0 * (double)prof_region[top[r]] /
                                (double)prof_total : 0.0);
    }
}

static void cpu_undef(const char *what){
    uint32_t at = cs_base + insn_ip;
    char bytes[64];
    int i, n = 0;
    for(i = 0; i < undef_nsite; i++) if(undef_site[i] == at) return;
    if(undef_nsite < UNDEF_SITES) undef_site[undef_nsite++] = at;
    if(undef_n >= UNDEF_MAX) return;
    if(++undef_n == UNDEF_MAX){
        trc("[cpu] %s at %04X:%04X lin=%05X - further reports suppressed\n",
            what, cpu.sreg[S_CS], (unsigned)insn_ip, at);
        return;
    }
    for(i = 0; i < 16; i++)
        n += snprintf(bytes + n, sizeof(bytes) - (size_t)n, "%02X ", cpu_ld8(at + (uint32_t)i));
    trc("[cpu] %s at %04X:%04X lin=%05X: %s\n",
        what, cpu.sreg[S_CS], (unsigned)insn_ip, at, bytes);
}

/* -cover: one bit per linear address where an instruction began */
uint8_t *cover_map = NULL;

/* Breakpoints and the execution trace (declared in dosrun.h). */
uint32_t brk_lin[BRK_MAX];
int brk_n = 0, brk_hit = -1;
uint32_t brk_resume = 0xFFFFFFFFu;
FILE *xtrace_fp = NULL;
uint32_t xtrace_lo = 0, xtrace_hi = 0xFFFFFFFFu;
uint64_t xtrace_left = 0;

static void xtrace_line(uint32_t lin){
    if(PE){
        fprintf(xtrace_fp, "%llu %04X:%08X EAX=%08X EBX=%08X ECX=%08X EDX=%08X ESI=%08X EDI=%08X EBP=%08X ESP=%08X DS=%04X ES=%04X FS=%04X GS=%04X SS=%04X F=%08X CPL=%d\n",
                (unsigned long long)cpu.cycles, cpu.sreg[S_CS], (unsigned)cpu.eip,
                REG32(R_EAX), REG32(R_EBX), REG32(R_ECX), REG32(R_EDX),
                REG32(R_ESI), REG32(R_EDI), REG32(R_EBP), REG32(R_ESP),
                cpu.sreg[S_DS], cpu.sreg[S_ES], cpu.sreg[S_FS], cpu.sreg[S_GS], cpu.sreg[S_SS],
                (unsigned)cpu_getflags(), cpu.cpl);
        if(--xtrace_left == 0) xtrace_fp = NULL;
        return;
    }
    fprintf(xtrace_fp, "%llu %04X:%04X AX=%04X BX=%04X CX=%04X DX=%04X SI=%04X DI=%04X BP=%04X SP=%04X DS=%04X ES=%04X SS=%04X F=%04X\n",
            (unsigned long long)cpu.cycles, cpu.sreg[S_CS], (unsigned)cpu.eip,
            REG16(R_EAX), REG16(R_EBX), REG16(R_ECX), REG16(R_EDX),
            REG16(R_ESI), REG16(R_EDI), REG16(R_EBP), REG16(R_ESP),
            cpu.sreg[S_DS], cpu.sreg[S_ES], cpu.sreg[S_SS],
            (unsigned)(cpu_getflags() & 0xFFFF));
    (void)lin;
    if(--xtrace_left == 0) xtrace_fp = NULL;
}

int int_watch = -1;                     /* -intwatch NN : log INT NN calls */

static void step(void){
    uint8_t op;
    uint32_t a,b,r;
    int sz;
    opsz = adsz = def32 ? 32 : 16; segovr = -1; rep = 0;
    cpu.inhibit = 0;
    if(brk_n){
        uint32_t la = cs_base + cpu.eip;
        int i;
        /* stop before the instruction; a run that goes on sets brk_resume
         * so the same instruction does not stop it again at once */
        if(la == brk_resume) brk_resume = 0xFFFFFFFFu;
        else for(i=0;i<brk_n;i++) if(la == brk_lin[i]){
            brk_hit = i; cpu.shutdown = 1; return;
        }
    }
    if(xtrace_fp){
        uint32_t la = cs_base + cpu.eip;
        if(la >= xtrace_lo && la <= xtrace_hi) xtrace_line(la);
    }
    if(prof_on && (++prof_tick & 0xFFF) == 0) prof_sample();
    if(cover_map){
        uint32_t la = cs_base + cpu.eip;
        if(la < RAM_SIZE) cover_map[la >> 3] |= (uint8_t)(1u << (la & 7));
    }
    /* Before the prefix loop, so a report names the first byte of the whole
     * instruction rather than wherever the decoder had got to. */
    insn_ip = cpu.eip;
again:
    op = fetch8();
    switch(op){
    case 0x26: segovr=S_ES; goto again;
    case 0x2E: segovr=S_CS; goto again;
    case 0x36: segovr=S_SS; goto again;
    case 0x3E: segovr=S_DS; goto again;
    case 0x64: segovr=S_FS; goto again;
    case 0x65: segovr=S_GS; goto again;
    case 0x66: opsz = def32 ? 16 : 32; goto again;
    case 0x67: adsz = def32 ? 16 : 32; goto again;
    case 0xF0: goto again;
    case 0xF2: rep=1; goto again;
    case 0xF3: rep=2; goto again;

    case 0x00: case 0x08: case 0x10: case 0x18: case 0x20: case 0x28: case 0x30: case 0x38:
        modrm(); r = alu(op>>3, rdE(8), rdG(8), 8); if((op>>3)!=7) wrE(8,r); break;
    case 0x01: case 0x09: case 0x11: case 0x19: case 0x21: case 0x29: case 0x31: case 0x39:
        modrm(); sz=opsz; r = alu(op>>3, rdE(sz), rdG(sz), sz); if((op>>3)!=7) wrE(sz,r); break;
    case 0x02: case 0x0A: case 0x12: case 0x1A: case 0x22: case 0x2A: case 0x32: case 0x3A:
        modrm(); r = alu(op>>3, rdG(8), rdE(8), 8); if((op>>3)!=7) wrG(8,r); break;
    case 0x03: case 0x0B: case 0x13: case 0x1B: case 0x23: case 0x2B: case 0x33: case 0x3B:
        modrm(); sz=opsz; r = alu(op>>3, rdG(sz), rdE(sz), sz); if((op>>3)!=7) wrG(sz,r); break;
    case 0x04: case 0x0C: case 0x14: case 0x1C: case 0x24: case 0x2C: case 0x34: case 0x3C:
        a=fetch8(); r = alu(op>>3, REG8(0), a, 8); if((op>>3)!=7) REG8(0)=(uint8_t)r; break;
    case 0x05: case 0x0D: case 0x15: case 0x1D: case 0x25: case 0x2D: case 0x35: case 0x3D:
        sz=opsz; a = (sz==32)?fetch32():fetch16();
        r = alu(op>>3, sz==32?REG32(0):REG16(0), a, sz);
        if((op>>3)!=7){ if(sz==32) REG32(0)=r; else REG16(0)=(uint16_t)r; } break;

    case 0x06: pushv(cpu.sreg[S_ES]); break;
    case 0x07: { uint16_t v = (uint16_t)cpu_ld16(cpu.sbase[S_SS] + sp_get()); set_sreg(S_ES, v); sp_add((uint32_t)opsz/8); break; }
    case 0x0E: pushv(cpu.sreg[S_CS]); break;
    case 0x16: pushv(cpu.sreg[S_SS]); break;
    case 0x17: { uint16_t v = (uint16_t)cpu_ld16(cpu.sbase[S_SS] + sp_get());
        set_sreg(S_SS, v); sp_add((uint32_t)opsz/8); cpu.inhibit = 1; break; }
    case 0x1E: pushv(cpu.sreg[S_DS]); break;
    case 0x1F: { uint16_t v = (uint16_t)cpu_ld16(cpu.sbase[S_SS] + sp_get()); set_sreg(S_DS, v); sp_add((uint32_t)opsz/8); break; }

    case 0x0F: op0f(); break;

    case 0x27: { uint8_t al=REG8(0); uint32_t oc=cpu.cf; uint32_t oa;
        cpu.cf=0; oa = cpu.af;
        if((al&15)>9 || oa){ int t=al+6; al=(uint8_t)t; cpu.cf=oc|((t>>8)&1); cpu.af=1; } else cpu.af=0;
        if((REG8(0))>0x99 || oc){ al+=0x60; cpu.cf=1; }
        REG8(0)=al; cpu.zf=al==0; cpu.sf=al>>7; cpu.pf=ptab[al]; break; }
    case 0x2F: { uint8_t al=REG8(0); uint32_t oc=cpu.cf; uint32_t oa=cpu.af;
        cpu.cf=0;
        if((al&15)>9 || oa){ int t=al-6; al=(uint8_t)t; cpu.cf=oc|((t>>8)&1); cpu.af=1; } else cpu.af=0;
        if((REG8(0))>0x99 || oc){ al-=0x60; cpu.cf=1; }
        REG8(0)=al; cpu.zf=al==0; cpu.sf=al>>7; cpu.pf=ptab[al]; break; }
    case 0x37:
        if(((REG8(0))&15)>9 || cpu.af){ REG16(0)+=0x106; cpu.af=1; cpu.cf=1; } else { cpu.af=0; cpu.cf=0; }
        REG8(0)&=0x0F; break;
    case 0x3F:
        if(((REG8(0))&15)>9 || cpu.af){ REG8(0)-=6; REG8(4)-=1; cpu.af=1; cpu.cf=1; } else { cpu.af=0; cpu.cf=0; }
        REG8(0)&=0x0F; break;

    case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        if(opsz==32) REG32(op&7)=do_inc(REG32(op&7),32); else REG16(op&7)=(uint16_t)do_inc(REG16(op&7),16); break;
    case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
        if(opsz==32) REG32(op&7)=do_dec(REG32(op&7),32); else REG16(op&7)=(uint16_t)do_dec(REG16(op&7),16); break;

    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        pushv(opsz==32?REG32(op&7):REG16(op&7)); break;
    case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
        if(opsz==32) REG32(op&7)=pop32(); else REG16(op&7)=pop16(); break;

    case 0x60: { int i;
        if(opsz==32){ uint32_t sp=REG32(R_ESP); for(i=0;i<8;i++) push32(i==4?sp:REG32(i)); }
        else { uint16_t sp=REG16(R_ESP); for(i=0;i<8;i++) push16(i==4?sp:REG16(i)); }
        break; }
    case 0x61: { int i;
        if(opsz==32){ for(i=7;i>=0;i--){ uint32_t v=pop32(); if(i!=4) REG32(i)=v; } }
        else { for(i=7;i>=0;i--){ uint16_t v=pop16(); if(i!=4) REG16(i)=v; } }
        break; }
    case 0x62: modrm(); break;
    case 0x63: modrm();                                 /* ARPL */
        if(!PE || cpu.vm){ cpu_undef("ARPL outside protected mode"); break; }
        a = rdE(16); b = rdG(16);
        if((a & 3) < (b & 3)){ wrE(16, (a & ~3u) | (b & 3)); cpu.zf = 1; } else cpu.zf = 0;
        break;
    case 0x68: pushv(opsz==32?fetch32():fetch16()); break;
    case 0x6A: pushv((uint32_t)(int32_t)(int8_t)fetch8()); break;
    case 0x69: modrm(); sz=opsz; a=rdE(sz); b=(sz==32)?fetch32():fetch16();
        if(sz==16){ int32_t p=(int32_t)(int16_t)a*(int16_t)b; wrG(16,(uint16_t)p); cpu.cf=cpu.of=((int32_t)(int16_t)p!=p); }
        else { int64_t p=(int64_t)(int32_t)a*(int32_t)b; wrG(32,(uint32_t)p); cpu.cf=cpu.of=((int64_t)(int32_t)p!=p); }
        break;
    case 0x6B: modrm(); sz=opsz; a=rdE(sz); b=(uint32_t)(int32_t)(int8_t)fetch8();
        if(sz==16){ int32_t p=(int32_t)(int16_t)a*(int16_t)b; wrG(16,(uint16_t)p); cpu.cf=cpu.of=((int32_t)(int16_t)p!=p); }
        else { int64_t p=(int64_t)(int32_t)a*(int32_t)b; wrG(32,(uint32_t)p); cpu.cf=cpu.of=((int64_t)(int32_t)p!=p); }
        break;
    case 0x6C: io_check(REG16(R_EDX), 1); strop(5,8); break;
    case 0x6D: io_check(REG16(R_EDX), opsz/8); strop(5,opsz); break;
    case 0x6E: io_check(REG16(R_EDX), 1); strop(6,8); break;
    case 0x6F: io_check(REG16(R_EDX), opsz/8); strop(6,opsz); break;

    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
        int8_t d = (int8_t)fetch8();
        if(cond(op&15)) cpu.eip = (cpu.eip + (uint32_t)(int32_t)d) & MASK(opsz);
        break; }

    case 0x80: case 0x82: modrm(); a=rdE(8); b=fetch8(); r=alu(reg_,a,b,8); if(reg_!=7) wrE(8,r); break;
    case 0x81: modrm(); sz=opsz; a=rdE(sz); b=(sz==32)?fetch32():fetch16(); r=alu(reg_,a,b,sz); if(reg_!=7) wrE(sz,r); break;
    case 0x83: modrm(); sz=opsz; a=rdE(sz); b=(uint32_t)(int32_t)(int8_t)fetch8(); r=alu(reg_,a,b,sz); if(reg_!=7) wrE(sz,r); break;

    case 0x84: modrm(); alu(4, rdE(8), rdG(8), 8); break;
    case 0x85: modrm(); sz=opsz; alu(4, rdE(sz), rdG(sz), sz); break;
    case 0x86: modrm(); a=rdE(8); wrE(8, rdG(8)); wrG(8,a); break;
    case 0x87: modrm(); sz=opsz; a=rdE(sz); wrE(sz, rdG(sz)); wrG(sz,a); break;
    case 0x88: modrm(); wrE(8, rdG(8)); break;
    case 0x89: modrm(); sz=opsz; wrE(sz, rdG(sz)); break;
    case 0x8A: modrm(); wrG(8, rdE(8)); break;
    case 0x8B: modrm(); sz=opsz; wrG(sz, rdE(sz)); break;
    case 0x8C: modrm(); if(ea_isreg) REG16(rm_)=cpu.sreg[reg_&7]; else cpu_st16(ea, cpu.sreg[reg_&7]); break;
    case 0x8D: modrm(); wrG(opsz, ea_off); break;
    case 0x8E: modrm();
        if(reg_ == S_CS || reg_ > S_GS){ cpu_undef("MOV to CS"); break; }
        set_sreg(reg_, (uint16_t)rdE(16));
        if(reg_ == S_SS) cpu.inhibit = 1;
        break;
    case 0x8F: modrm(); { uint32_t v = popv(); wrE(opsz, v); } break;

    case 0x90: break;
    case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
        if(opsz==32){ uint32_t t=REG32(0); REG32(0)=REG32(op&7); REG32(op&7)=t; }
        else { uint16_t t=REG16(0); REG16(0)=REG16(op&7); REG16(op&7)=t; } break;
    case 0x98: if(opsz==32) REG32(0)=(uint32_t)(int32_t)(int16_t)REG16(0); else REG16(0)=(uint16_t)(int16_t)(int8_t)REG8(0); break;
    case 0x99: if(opsz==32) REG32(R_EDX)=((int32_t)REG32(0)<0)?0xFFFFFFFFu:0; else REG16(R_EDX)=((int16_t)REG16(0)<0)?0xFFFF:0; break;
    case 0x9A: { uint32_t noff = (opsz==32)?fetch32():fetch16(); uint16_t nseg=fetch16();
        far_call(nseg, noff); break; }
    case 0x9B: break;
    case 0x9C: if(cpu.vm) iopl_check(); pushv(cpu_getflags() & ~0x30000u); break;
    case 0x9D: if(cpu.vm) iopl_check(); write_flags(popv(), opsz); break;
    case 0x9E: cpu_setflags((cpu_getflags()&0xFFFFFF00u)|REG8(4)); break;
    case 0x9F: REG8(4) = (uint8_t)((cpu_getflags()&0xD5)|2); break;

    case 0xA0: { uint32_t o = (adsz==32)?fetch32():fetch16(); REG8(0)=cpu_ld8(sb(S_DS)+o); break; }
    case 0xA1: { uint32_t o = (adsz==32)?fetch32():fetch16();
        if(opsz==32) REG32(0)=cpu_ld32(sb(S_DS)+o); else REG16(0)=cpu_ld16(sb(S_DS)+o); break; }
    case 0xA2: { uint32_t o = (adsz==32)?fetch32():fetch16(); cpu_st8(sb(S_DS)+o, REG8(0)); break; }
    case 0xA3: { uint32_t o = (adsz==32)?fetch32():fetch16();
        if(opsz==32) cpu_st32(sb(S_DS)+o, REG32(0)); else cpu_st16(sb(S_DS)+o, REG16(0)); break; }
    case 0xA4: strop(0,8); break;
    case 0xA5: strop(0,opsz); break;
    case 0xA6: strop(1,8); break;
    case 0xA7: strop(1,opsz); break;
    case 0xA8: a=fetch8(); alu(4, REG8(0), a, 8); break;
    case 0xA9: sz=opsz; a=(sz==32)?fetch32():fetch16(); alu(4, sz==32?REG32(0):REG16(0), a, sz); break;
    case 0xAA: strop(2,8); break;
    case 0xAB: strop(2,opsz); break;
    case 0xAC: strop(3,8); break;
    case 0xAD: strop(3,opsz); break;
    case 0xAE: strop(4,8); break;
    case 0xAF: strop(4,opsz); break;

    case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
        REG8(op&7) = fetch8(); break;
    case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
        if(opsz==32) REG32(op&7)=fetch32(); else REG16(op&7)=fetch16(); break;

    case 0xC0: modrm(); { int c=fetch8(); wrE(8, do_shift(reg_, rdE(8), c, 8)); } break;
    case 0xC1: modrm(); sz=opsz; { int c=fetch8(); wrE(sz, do_shift(reg_, rdE(sz), c, sz)); } break;
    case 0xC2: { uint16_t n=fetch16(); cpu.eip = popv() & MASK(opsz); sp_add(n); break; }
    case 0xC3: cpu.eip = popv() & MASK(opsz); break;
    case 0xC4: modrm(); load_far(S_ES); break;
    case 0xC5: modrm(); load_far(S_DS); break;
    case 0xC6: modrm(); { uint8_t v=fetch8(); wrE(8,v); } break;
    case 0xC7: modrm(); sz=opsz; { uint32_t v=(sz==32)?fetch32():fetch16(); wrE(sz,v); } break;
    case 0xC8: { uint16_t nb=fetch16(); uint8_t lvl=(uint8_t)(fetch8() & 31); uint32_t fp; int i;
        /* the frame pointer is EBP in a 32-bit stack, BP otherwise */
        pushv(REG32(R_EBP)); fp=sp_get();
        for(i=1;i<lvl;i++){
            uint32_t bp;
            if(STK32) bp = REG32(R_EBP) -= (uint32_t)opsz/8;
            else bp = REG16(R_EBP) = (uint16_t)(REG16(R_EBP) - opsz/8);
            pushv(opsz==32 ? cpu_ld32(cpu.sbase[S_SS]+bp) : cpu_ld16(cpu.sbase[S_SS]+bp));
        }
        if(lvl>0) pushv(fp);
        if(opsz==32) REG32(R_EBP)=fp; else REG16(R_EBP)=(uint16_t)fp;
        sp_add((uint32_t)-(int32_t)nb); break; }
    case 0xC9: set_esp(STK32 ? REG32(R_EBP) : REG16(R_EBP));
        if(opsz==32) REG32(R_EBP)=pop32(); else REG16(R_EBP)=pop16(); break;
    case 0xCA: { uint16_t n=fetch16(); far_ret(n); break; }
    case 0xCB: far_ret(0); break;
    case 0xCC: soft_int(3); break;
    case 0xCD: { uint8_t n=fetch8();
        if(int_watch >= 0 && n == int_watch)
            printf("[int%02X] AX=%04X BX=%04X CX=%04X DX=%04X DS=%04X ES=%04X from %04X:%04X\n",
                   n, REG16(R_EAX), REG16(R_EBX), REG16(R_ECX), REG16(R_EDX),
                   cpu.sreg[S_DS], cpu.sreg[S_ES],
                   cpu.sreg[S_CS], (unsigned)(cpu.eip-2));
        soft_int(n); break; }
    case 0xCE: if(cpu.of) soft_int(4); break;
    case 0xCF: iret_(); break;

    case 0xD0: modrm(); wrE(8, do_shift(reg_, rdE(8), 1, 8)); break;
    case 0xD1: modrm(); sz=opsz; wrE(sz, do_shift(reg_, rdE(sz), 1, sz)); break;
    case 0xD2: modrm(); wrE(8, do_shift(reg_, rdE(8), REG8(1), 8)); break;
    case 0xD3: modrm(); sz=opsz; wrE(sz, do_shift(reg_, rdE(sz), REG8(1), sz)); break;
    case 0xD4: { uint8_t base=fetch8(); if(base){ REG8(4)=REG8(0)/base; REG8(0)=REG8(0)%base; }
        cpu.zf=REG8(0)==0; cpu.sf=REG8(0)>>7; cpu.pf=ptab[REG8(0)]; break; }
    case 0xD5: { uint8_t base=fetch8(); REG8(0)=(uint8_t)(REG8(0)+REG8(4)*base); REG8(4)=0;
        cpu.zf=REG8(0)==0; cpu.sf=REG8(0)>>7; cpu.pf=ptab[REG8(0)]; break; }
    case 0xD6: REG8(0) = cpu.cf ? 0xFF : 0x00; break;
    case 0xD7: REG8(0) = cpu_ld8(sb(S_DS) + (adsz==32 ? REG32(R_EBX)+REG8(0) : ((REG16(R_EBX)+REG8(0))&0xFFFF))); break;
    case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
        modrm();
        { char w[32];
          snprintf(w, sizeof(w), "x87 esc %02X", op);
          cpu_undef(w); }
        break;

    case 0xE0: case 0xE1: case 0xE2: { int8_t d=(int8_t)fetch8(); uint32_t c; int take;
        if(adsz==32) c = --REG32(R_ECX); else c = --REG16(R_ECX);
        take = c!=0;
        if(op==0xE0) take = take && !cpu.zf;
        if(op==0xE1) take = take && cpu.zf;
        if(take) cpu.eip=(cpu.eip+(uint32_t)(int32_t)d)&MASK(opsz); break; }
    case 0xE3: { int8_t d=(int8_t)fetch8(); uint32_t c = adsz==32?REG32(R_ECX):REG16(R_ECX);
        if(!c) cpu.eip=(cpu.eip+(uint32_t)(int32_t)d)&MASK(opsz); break; }
    case 0xE4: { uint8_t p=fetch8(); io_check(p, 1); REG8(0)=io_r8(p); break; }
    case 0xE5: { uint8_t p=fetch8(); io_check(p, 2); REG16(0)=io_r16(p); break; }
    case 0xE6: { uint8_t p=fetch8(); io_check(p, 1); io_w8(p, REG8(0)); break; }
    case 0xE7: { uint8_t p=fetch8(); io_check(p, 2); io_w16(p, REG16(0)); break; }
    case 0xE8: { int32_t d = (opsz==32)?(int32_t)fetch32():(int32_t)(int16_t)fetch16();
        pushv(cpu.eip); cpu.eip=(cpu.eip+(uint32_t)d)&MASK(opsz); break; }
    case 0xE9: { int32_t d = (opsz==32)?(int32_t)fetch32():(int32_t)(int16_t)fetch16();
        cpu.eip=(cpu.eip+(uint32_t)d)&MASK(opsz); break; }
    case 0xEA: { uint32_t o=(opsz==32)?fetch32():fetch16(); uint16_t s=fetch16();
        far_jmp(s, o); break; }
    case 0xEB: { int8_t d=(int8_t)fetch8(); cpu.eip=(cpu.eip+(uint32_t)(int32_t)d)&MASK(opsz); break; }
    case 0xEC: io_check(REG16(R_EDX), 1); REG8(0)=io_r8(REG16(R_EDX)); break;
    case 0xED: io_check(REG16(R_EDX), 2); REG16(0)=io_r16(REG16(R_EDX)); break;
    case 0xEE: io_check(REG16(R_EDX), 1); io_w8(REG16(R_EDX), REG8(0)); break;
    case 0xEF: io_check(REG16(R_EDX), 2); io_w16(REG16(R_EDX), REG16(0)); break;

    case 0xF4: cpl0_check(); cpu.halted = 1; break;
    case 0xF5: cpu.cf = !cpu.cf; break;
    case 0xF6: modrm(); a=rdE(8);
        switch(reg_){
        case 0: case 1: b=fetch8(); alu(4,a,b,8); break;
        case 2: wrE(8, (~a)&0xFF); break;
        case 3: r=alu(5,0,a,8); wrE(8,r); break;
        case 4: { uint16_t p=(uint16_t)((uint16_t)REG8(0)*(uint8_t)a); REG16(0)=p;
                  cpu.cf=cpu.of=((p>>8)!=0); cpu.zf=(p&0xFF)==0; cpu.sf=(p>>7)&1; cpu.pf=ptab[p&0xFF]; break; }
        case 5: { int16_t p=(int16_t)((int16_t)(int8_t)REG8(0)*(int8_t)a); REG16(0)=(uint16_t)p;
                  cpu.cf=cpu.of=((int16_t)(int8_t)p != p); break; }
        case 6: { uint16_t n; if(!a){ div_err(); break; } n=REG16(0);
                  if(n/a > 0xFF){ div_err(); break; } REG8(0)=(uint8_t)(n/a); REG8(4)=(uint8_t)(n%a); break; }
        case 7: { int16_t n; int8_t d; int32_t q; if(!a){ div_err(); break; }
                  n=(int16_t)REG16(0); d=(int8_t)a; q=n/d;
                  if(q>127||q<-128){ div_err(); break; }
                  REG8(0)=(uint8_t)q; REG8(4)=(uint8_t)(n%d); break; }
        } break;
    case 0xF7: modrm(); sz=opsz; a=rdE(sz);
        switch(reg_){
        case 0: case 1: b=(sz==32)?fetch32():fetch16(); alu(4,a,b,sz); break;
        case 2: wrE(sz, (~a)&MASK(sz)); break;
        case 3: r=alu(5,0,a,sz); wrE(sz,r); break;
        case 4: if(sz==16){ uint32_t p=(uint32_t)REG16(0)*(uint16_t)a; REG16(0)=(uint16_t)p; REG16(R_EDX)=(uint16_t)(p>>16);
                            cpu.cf=cpu.of=((p>>16)!=0); }
                else { uint64_t p=(uint64_t)REG32(0)*a; REG32(0)=(uint32_t)p; REG32(R_EDX)=(uint32_t)(p>>32);
                       cpu.cf=cpu.of=((p>>32)!=0); } break;
        case 5: if(sz==16){ int32_t p=(int32_t)(int16_t)REG16(0)*(int16_t)a; REG16(0)=(uint16_t)p; REG16(R_EDX)=(uint16_t)(p>>16);
                            cpu.cf=cpu.of=((int32_t)(int16_t)p != p); }
                else { int64_t p=(int64_t)(int32_t)REG32(0)*(int32_t)a; REG32(0)=(uint32_t)p; REG32(R_EDX)=(uint32_t)(p>>32);
                       cpu.cf=cpu.of=((int64_t)(int32_t)p != p); } break;
        case 6: if(sz==16){ uint32_t n; if(!a){div_err();break;} n=((uint32_t)REG16(R_EDX)<<16)|REG16(0);
                            if(n/a > 0xFFFF){ div_err(); break; } REG16(0)=(uint16_t)(n/a); REG16(R_EDX)=(uint16_t)(n%a); }
                else { uint64_t n; if(!a){div_err();break;} n=((uint64_t)REG32(R_EDX)<<32)|REG32(0);
                       if(n/a > 0xFFFFFFFFull){ div_err(); break; } REG32(0)=(uint32_t)(n/a); REG32(R_EDX)=(uint32_t)(n%a); } break;
        case 7: if(sz==16){ int32_t n,q; if(!a){div_err();break;} n=(int32_t)(((uint32_t)REG16(R_EDX)<<16)|REG16(0));
                            q=n/(int16_t)a; if(q>32767||q<-32768){div_err();break;}
                            REG16(0)=(uint16_t)q; REG16(R_EDX)=(uint16_t)(n%(int16_t)a); }
                else { int64_t n,q; if(!a){div_err();break;} n=(int64_t)(((uint64_t)REG32(R_EDX)<<32)|REG32(0));
                       q=n/(int32_t)a; if(q>2147483647LL||q<-2147483648LL){div_err();break;}
                       REG32(0)=(uint32_t)q; REG32(R_EDX)=(uint32_t)(n%(int32_t)a); } break;
        } break;
    case 0xF8: cpu.cf=0; break;
    case 0xF9: cpu.cf=1; break;
    case 0xFA: iopl_check(); cpu.iflag=0; break;
    case 0xFB: iopl_check(); cpu.iflag=1; break;
    case 0xFC: cpu.df=0; break;
    case 0xFD: cpu.df=1; break;
    case 0xFE: modrm(); a=rdE(8);
        if(reg_==0) wrE(8, do_inc(a,8)); else wrE(8, do_dec(a,8)); break;
    case 0xFF: modrm(); sz=opsz;
        switch(reg_){
        case 0: wrE(sz, do_inc(rdE(sz),sz)); break;
        case 1: wrE(sz, do_dec(rdE(sz),sz)); break;
        case 2: { uint32_t t=rdE(sz); pushv(cpu.eip); cpu.eip=t&MASK(sz); break; }
        case 3: { uint32_t o=sz==32?cpu_ld32(ea):cpu_ld16(ea); uint16_t s=cpu_ld16(ea+(uint32_t)sz/8);
                  far_call(s, o); break; }
        case 4: cpu.eip = rdE(sz)&MASK(sz); break;
        case 5: { uint32_t o=sz==32?cpu_ld32(ea):cpu_ld16(ea); uint16_t s=cpu_ld16(ea+(uint32_t)sz/8);
                  far_jmp(s, o); break; }
        case 6: pushv(rdE(sz)); break;
        } break;

    default: {
        char w[32];
        snprintf(w, sizeof(w), "unhandled opcode %02X", op);
        cpu_undef(w);
        break; }
    }
    cpu.cycles++;
}

/* One instruction.  Under PE a fault jumps back here and is delivered. */
void cpu_step(void){
    if(!PE){ step(); return; }
    save_state();
    if(FAULT_SETJMP(fault_jb)){ deliver_fault(); cpu.cycles++; return; }
    fault_armed = 1;
    step();
    fault_armed = 0;
}

void cpu_reset(void){
    int i;
    init_ptab();
    memset(&cpu,0,sizeof(cpu));
    cpu.iflag = 1;
    cpu.cr0 = 0x10;
    cpu.idt_limit = 0x3FF;
    for(i = 0; i < 6; i++) cpu.slimit[i] = 0xFFFF;
    set_sreg(S_CS,0xF000); cpu.eip=0xFFF0;
}
