/* Microsoft-compatible INT 33h mouse driver for dosrun.
 *
 * The runner is headless, so main.c supplies deterministic absolute mouse
 * positions and button states.  This module implements the documented core
 * interface through Microsoft Mouse 7.05.  The cursor is composed onto PNG
 * output rather than written into VGA memory: looking at the screen must not
 * change a run, just as -shot never changes one without it.
 */
#include "dosrun.h"

#define AX REG16(R_EAX)
#define BX REG16(R_EBX)
#define CX REG16(R_ECX)
#define DX REG16(R_EDX)
#define SI REG16(R_ESI)
#define DI REG16(R_EDI)
#define CL REG8(1)
#define DL REG8(2)
#define BL REG8(3)
#define CH REG8(5)
#define DH REG8(6)
#define BH REG8(7)

#define MOUSE_MAGIC 0x334B444Du       /* "MDK3", only in the opaque saved state */
#define MOUSE_BUTTONS 3
#define ALT_HANDLERS 3
#define CURSOR_WORDS 256              /* up to a 64 by 64 cursor */
#define NOTIFY_QUEUE 64
#define RETURN_CB 0xFE
#define RETURN_OFF 0x1A00
#define DATA_SEG 0xF000
#define DATA_OFF 0x1800

static const uint16_t video_modes[] = {3,0x0D,0x0E,0x10,0x12,0x13,0x100,0x101,0x103};
static const char *const video_descriptions[] = {
    "80x25 text$", "320x200 16-color graphics$", "640x200 16-color graphics$",
    "640x350 16-color graphics$", "640x480 16-color graphics$",
    "320x200 256-color graphics$", "640x400 256-color graphics$",
    "640x480 256-color graphics$", "800x600 256-color graphics$"
};

typedef struct {
    uint16_t count, x, y;
} ButtonCount;

typedef struct {
    uint16_t mask, seg, off;
} Handler;

typedef struct {
    uint32_t magic;
    uint16_t enabled, buttons, x, y;
    uint16_t min_x, max_x, min_y, max_y;
    uint16_t abs_max_x, abs_max_y;
    int16_t visible;
    int32_t motion_x, motion_y;
    ButtonCount press[MOUSE_BUTTONS], release[MOUSE_BUTTONS];
    uint16_t ratio_x, ratio_y, sensitivity_x, sensitivity_y, threshold;
    uint16_t interrupt_rate, page, language, light_pen;
    uint16_t cursor_kind, hot_x, hot_y, cursor_words, cursor_rows;
    uint16_t text_screen, text_cursor, text_start, text_end;
    uint16_t screen_mask[CURSOR_WORDS], cursor_mask[CURSOR_WORDS];
    Handler handler, alt[ALT_HANDLERS];
    uint16_t update_on, update_left, update_top, update_right, update_bottom;
    uint16_t profile;
    uint8_t profiles[324];
    uint8_t profile_names[64];
    uint16_t lcd_style, lcd_size, lcd_threshold, lcd_active, lcd_delay;
} MouseState;

typedef struct {
    uint16_t condition, buttons, x, y;
    int16_t dx, dy;
    uint8_t modifiers;
} Notify;

static MouseState m;
static int ready;
static Notify queue[NOTIFY_QUEUE], current;
static int q_head, q_tail, dispatch_index, callback_active;
static CPU callback_cpu;

extern void (*cb_table[256])(void);

static uint16_t clamp16(uint16_t v, uint16_t lo, uint16_t hi){
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

static void mode_limits(int mode, uint16_t *x, uint16_t *y){
    switch(mode){
    case 0x0D: case 0x13: *x = 639; *y = 199; break;
    case 0x0E:             *x = 639; *y = 199; break;
    case 0x10:             *x = 639; *y = 349; break;
    case 0x12:             *x = 639; *y = 479; break;
    case 0x100:            *x = 639; *y = 399; break;
    case 0x101:            *x = 639; *y = 479; break;
    case 0x103:            *x = 799; *y = 599; break;
    default:               *x = 639; *y = 199; break;
    }
}

static void default_cursor(void){
    static const uint16_t arrow[16] = {
        0x8000,0xC000,0xA000,0x9000,0x8800,0x8400,0x8200,0x8100,
        0x8080,0x9F80,0xA400,0xC400,0x0200,0x0200,0x0100,0x0100
    };
    int i;
    m.cursor_kind = 2;
    m.hot_x = m.hot_y = 0;
    m.cursor_words = 1; m.cursor_rows = 16;
    for(i=0;i<CURSOR_WORDS;i++){
        m.screen_mask[i] = 0xFFFF;
        m.cursor_mask[i] = i < 16 ? arrow[i] : 0;
    }
    m.text_screen = 0xFFFF; m.text_cursor = 0x7700;
    m.text_start = 6; m.text_end = 7;
}

static void default_profiles(void){
    static const char names[4][16] = {
        "Default         ", "Slow            ",
        "Medium          ", "Fast            "
    };
    int p, i;
    memset(m.profiles, 0, sizeof(m.profiles));
    for(p=0;p<4;p++){
        m.profiles[p] = 1;
        for(i=0;i<32;i++){
            m.profiles[4 + p*32 + i] = i ? 0x7F : 0;
            m.profiles[0x84 + p*32 + i] = 0x10;
        }
        memcpy(&m.profiles[0x104 + p*16], names[p], 16);
        memcpy(&m.profile_names[p*16], names[p], 16);
    }
    m.profile = 1;
}

static void reset_counts(void){
    int i;
    m.motion_x = m.motion_y = 0;
    for(i=0;i<MOUSE_BUTTONS;i++){
        memset(&m.press[i], 0, sizeof(m.press[i]));
        memset(&m.release[i], 0, sizeof(m.release[i]));
    }
}

static void reset_driver(int keep_position){
    uint16_t x = m.x, y = m.y, ax, ay;
    memset(&m, 0, sizeof(m));
    m.magic = MOUSE_MAGIC; m.enabled = 1; m.visible = -1;
    mode_limits(vga_get_mode(), &ax, &ay);
    m.abs_max_x = m.max_x = ax; m.abs_max_y = m.max_y = ay;
    m.x = keep_position ? clamp16(x, 0, ax) : ax / 2;
    m.y = keep_position ? clamp16(y, 0, ay) : ay / 2;
    m.ratio_x = 8; m.ratio_y = 16;
    m.sensitivity_x = m.sensitivity_y = 50; m.threshold = 64;
    default_cursor(); default_profiles(); reset_counts();
    q_head = q_tail = callback_active = 0;
    dispatch_index = ALT_HANDLERS + 1;
}

void mouse_mode_changed(int mode){
    uint16_t ax, ay;
    if(!ready) return;
    mode_limits(mode, &ax, &ay);
    m.abs_max_x = m.max_x = ax; m.abs_max_y = m.max_y = ay;
    m.min_x = m.min_y = 0;
    m.x = ax / 2; m.y = ay / 2;
    m.visible = -1; m.update_on = 0;
    reset_counts();
}

static uint16_t get16(uint32_t p){ return mem_r16(p); }

static void state_save(uint32_t p){
    const uint8_t *s = (const uint8_t *)&m;
    size_t i;
    for(i=0;i<sizeof(m);i++) mem_w8(p + (uint32_t)i, s[i]);
}

static int state_restore(uint32_t p){
    MouseState n;
    uint8_t *d = (uint8_t *)&n;
    size_t i;
    for(i=0;i<sizeof(n);i++) d[i] = mem_r8(p + (uint32_t)i);
    if(n.magic != MOUSE_MAGIC || n.cursor_words == 0 ||
       (uint32_t)n.cursor_words * n.cursor_rows > CURSOR_WORDS) return 0;
    m = n;
    m.x = clamp16(m.x, m.min_x, m.max_x);
    m.y = clamp16(m.y, m.min_y, m.max_y);
    return 1;
}

static void copy_to_guest(uint32_t p, const uint8_t *s, size_t n){
    size_t i;
    for(i=0;i<n;i++) mem_w8(p + (uint32_t)i, s[i]);
}

static void copy_from_guest(uint8_t *d, uint32_t p, size_t n){
    size_t i;
    for(i=0;i<n;i++) d[i] = mem_r8(p + (uint32_t)i);
}

static void set_data_pointer(uint16_t off){ set_sreg(S_ES, DATA_SEG); DI = off; }

static void profile_to_rom(void){
    copy_to_guest((uint32_t)DATA_SEG*16 + DATA_OFF, m.profiles, sizeof(m.profiles));
}

static void settings_to_guest(uint32_t p, uint16_t n){
    uint8_t b[0x154];
    size_t k;
    memset(b, 0, sizeof(b));
    b[0] = 4; b[1] = (uint8_t)m.language;
    b[2] = (uint8_t)m.sensitivity_x; b[3] = (uint8_t)m.sensitivity_y;
    b[4] = (uint8_t)m.threshold; b[5] = (uint8_t)m.profile;
    b[6] = (uint8_t)m.interrupt_rate; b[13] = 1; b[14] = 2;
    memcpy(b + 0x10, m.profiles, sizeof(m.profiles));
    if(n > sizeof(b)) n = (uint16_t)sizeof(b);
    for(k=0;k<n;k++) mem_w8(p + (uint32_t)k, b[k]);
}

static int handler_matches(const Handler *h, const Notify *n, int alternate){
    if(!h->seg && !h->off) return 0;
    if(!(h->mask & n->condition)) return 0;
    if(alternate && (!(h->mask & 0xE0) || !(h->mask & n->modifiers))) return 0;
    return 1;
}

static void far_call_handler(const Handler *h, const Notify *n){
    uint16_t sp;
    callback_cpu = cpu;
    sp = (uint16_t)(REG16(R_ESP) - 4);
    REG16(R_ESP) = sp;
    mem_w16(cpu.sbase[S_SS] + sp, RETURN_OFF);
    mem_w16(cpu.sbase[S_SS] + (uint16_t)(sp + 2), DATA_SEG);
    AX = n->condition; BX = n->buttons; CX = n->x; DX = n->y;
    SI = (uint16_t)n->dx; DI = (uint16_t)n->dy;
    set_sreg(S_CS, h->seg); cpu.eip = h->off; cpu.halted = 0;
    callback_active = 1;
}

static void dispatch(void){
    while(!callback_active){
        Handler *h = NULL;
        if(dispatch_index == 0) h = &m.handler;
        else if(dispatch_index <= ALT_HANDLERS) h = &m.alt[dispatch_index-1];
        else {
            if(q_head == q_tail) return;
            current = queue[q_head]; q_head = (q_head + 1) % NOTIFY_QUEUE;
            dispatch_index = 0; continue;
        }
        if(handler_matches(h, &current, dispatch_index != 0)){
            far_call_handler(h, &current); return;
        }
        dispatch_index++;
    }
}

static void callback_return(void){
    if(callback_active){
        uint64_t cycles = cpu.cycles;
        int shutdown = cpu.shutdown;
        cpu = callback_cpu;
        cpu_state_restored();
        cpu.cycles = cycles;
        if(shutdown) cpu.shutdown = 1;
        callback_active = 0; dispatch_index++;
        dispatch();
        cpu_no_iret();
    }
}

static void notify(uint16_t condition, int16_t dx, int16_t dy){
    int next = (q_tail + 1) % NOTIFY_QUEUE;
    uint8_t shift = ram[0x417];
    Notify *n;
    if(next == q_head) return;                 /* only possible under a stuck handler */
    n = &queue[q_tail]; q_tail = next;
    n->condition = condition; n->buttons = m.buttons;
    n->x = m.x; n->y = m.y; n->dx = dx; n->dy = dy;
    n->modifiers = (shift & 3 ? 0x20 : 0) | (shift & 4 ? 0x40 : 0) |
                   (shift & 8 ? 0x80 : 0);
    dispatch();
}

void mouse_input(uint16_t x, uint16_t y, uint16_t buttons){
    uint16_t old_buttons, condition = 0;
    int32_t dx, dy, mx, my;
    int i;
    if(!ready || !m.enabled) return;
    x = clamp16(x, m.min_x, m.max_x); y = clamp16(y, m.min_y, m.max_y);
    dx = (int32_t)x - m.x; dy = (int32_t)y - m.y;
    mx = dx * m.ratio_x / 8; my = dy * m.ratio_y / 8;
    if(dx || dy) condition |= 1;
    old_buttons = m.buttons; buttons &= 7;
    m.x = x; m.y = y; m.buttons = buttons;
    m.motion_x += mx; m.motion_y += my;
    for(i=0;i<MOUSE_BUTTONS;i++){
        uint16_t bit = (uint16_t)(1u << i);
        if(!(old_buttons & bit) && (buttons & bit)){
            condition |= (uint16_t)(2u << (i*2));
            if(m.press[i].count != 0xFFFF) m.press[i].count++;
            m.press[i].x = x; m.press[i].y = y;
        } else if((old_buttons & bit) && !(buttons & bit)){
            condition |= (uint16_t)(4u << (i*2));
            if(m.release[i].count != 0xFFFF) m.release[i].count++;
            m.release[i].x = x; m.release[i].y = y;
        }
    }
    if(condition) notify(condition, (int16_t)mx, (int16_t)my);
}

static void set_range(int horizontal, uint16_t lo, uint16_t hi){
    if(lo > hi){ uint16_t t = lo; lo = hi; hi = t; }
    if(horizontal){ m.min_x = lo; m.max_x = hi; m.x = clamp16(m.x, lo, hi); }
    else { m.min_y = lo; m.max_y = hi; m.y = clamp16(m.y, lo, hi); }
}

static void button_data(ButtonCount *a){
    unsigned b = BX;
    AX = m.buttons;
    if(b < MOUSE_BUTTONS){ BX = a[b].count; CX = a[b].x; DX = a[b].y; a[b].count = 0; }
    else BX = CX = DX = 0;
}

static void mouse_int33(void){
    uint32_t p;
    unsigned i, n;
    switch(AX){
    case 0x0000: reset_driver(0); AX = 0xFFFF; BX = MOUSE_BUTTONS; break;
    case 0x0001: if(m.visible < 0) m.visible++; break;
    case 0x0002: m.visible--; break;
    case 0x0003: BX = m.buttons; CX = m.x; DX = m.y; break;
    case 0x0004: m.x = clamp16(CX, m.min_x, m.max_x); m.y = clamp16(DX, m.min_y, m.max_y); break;
    case 0x0005: button_data(m.press); break;
    case 0x0006: button_data(m.release); break;
    case 0x0007: set_range(1, CX, DX); break;
    case 0x0008: set_range(0, CX, DX); break;
    case 0x0009:
        m.cursor_kind = 2; m.hot_x = BX; m.hot_y = CX; m.cursor_words = 1; m.cursor_rows = 16;
        p = cpu.sbase[S_ES] + DX;
        for(i=0;i<16;i++) m.screen_mask[i] = get16(p + i*2);
        for(i=0;i<16;i++) m.cursor_mask[i] = get16(p + 32 + i*2);
        break;
    case 0x000A:
        m.cursor_kind = BX ? 1 : 0;
        if(BX){ m.text_start = CX; m.text_end = DX; }
        else { m.text_screen = CX; m.text_cursor = DX; }
        break;
    case 0x000B: CX = (uint16_t)m.motion_x; DX = (uint16_t)m.motion_y; m.motion_x = m.motion_y = 0; break;
    case 0x000C: m.handler.mask = CX; m.handler.seg = cpu.sreg[S_ES]; m.handler.off = DX; break;
    case 0x000D: m.light_pen = 1; break;
    case 0x000E: m.light_pen = 0; break;
    case 0x000F: if(CX) m.ratio_x = CX; if(DX) m.ratio_y = DX; break;
    case 0x0010:
        m.update_on = 1; m.update_left = CX; m.update_top = DX;
        m.update_right = SI; m.update_bottom = DI;
        if(m.x >= CX && m.x <= SI && m.y >= DX && m.y <= DI) m.visible = -1;
        break;
    case 0x0012:
        n = (unsigned)BH * CH;
        if(!BH || !CH || n > CURSOR_WORDS){ AX = 0; break; }
        m.cursor_kind = 2; m.cursor_words = BH; m.cursor_rows = CH;
        m.hot_x = (uint16_t)(int16_t)(int8_t)BL; m.hot_y = (uint16_t)(int16_t)(int8_t)CL;
        p = cpu.sbase[S_ES] + DX;
        for(i=0;i<n;i++) m.screen_mask[i] = get16(p + i*2);
        for(i=0;i<n;i++) m.cursor_mask[i] = get16(p + (uint32_t)n*2 + i*2);
        AX = 0xFFFF; break;
    case 0x0013: m.threshold = DX ? DX : 64; break;
    case 0x0014: {
        Handler old = m.handler;
        m.handler.mask = CX; m.handler.seg = cpu.sreg[S_ES]; m.handler.off = DX;
        CX = old.mask; set_sreg(S_ES, old.seg); DX = old.off; break; }
    case 0x0015: BX = (uint16_t)sizeof(m); break;
    case 0x0016: state_save(cpu.sbase[S_ES] + DX); break;
    case 0x0017: AX = state_restore(cpu.sbase[S_ES] + DX) ? 0x0017 : 0xFFFF; break;
    case 0x0018:
        AX = 0xFFFF;
        if(CX == 0){ memset(m.alt, 0, sizeof(m.alt)); AX = 0x0018; break; }
        for(i=0;i<ALT_HANDLERS;i++) if(m.alt[i].mask == CX || (!m.alt[i].seg && !m.alt[i].off)){
            m.alt[i].mask = CX; m.alt[i].seg = cpu.sreg[S_ES]; m.alt[i].off = DX; AX = 0x0018; break;
        }
        break;
    case 0x0019:
        for(i=0;i<ALT_HANDLERS;i++) if(m.alt[i].mask == CX){ BX = m.alt[i].seg; DX = m.alt[i].off; return; }
        CX = BX = DX = 0; break;
    case 0x001A: m.sensitivity_x = BX; m.sensitivity_y = CX; m.threshold = DX; break;
    case 0x001B: BX = m.sensitivity_x; CX = m.sensitivity_y; DX = m.threshold; break;
    case 0x001C: m.interrupt_rate = BX <= 4 ? BX : 4; break;
    case 0x001D: m.page = BX; break;
    case 0x001E: BX = m.page; break;
    case 0x001F: m.enabled = 0; m.visible = -1; AX = 0x001F; set_sreg(S_ES, 0); BX = 0; break;
    case 0x0020: m.enabled = 1; AX = 0x0020; break;
    case 0x0021: reset_driver(1); AX = 0xFFFF; BX = MOUSE_BUTTONS; break;
    case 0x0022: m.language = BX; break;
    case 0x0023: BX = m.language; break;
    case 0x0024: BX = 0x0705; CH = 4; CL = 0; break;
    case 0x0025:
        AX = m.cursor_kind == 2 ? 0x2000 : m.cursor_kind == 1 ? 0x1000 : 0;
        BX = 0; CX = 1; DX = callback_active ? 1 : 0; break;
    case 0x0026: BX = m.enabled ? 0 : 1; CX = m.abs_max_x; DX = m.abs_max_y; break;
    case 0x0027:
        AX = m.cursor_kind == 1 ? m.text_start : m.text_screen;
        BX = m.cursor_kind == 1 ? m.text_end : m.text_cursor;
        CX = (uint16_t)m.motion_x; DX = (uint16_t)m.motion_y;
        m.motion_x = m.motion_y = 0; break;
    case 0x0028: if(CX) mouse_mode_changed(CX); CL = 0; break;
    case 0x0029: {
        uint16_t previous = CX; CX = 0;
        for(i=0;i<sizeof(video_modes)/sizeof(video_modes[0]);i++)
            if(previous == 0 || video_modes[i] == previous){
                unsigned k = previous == 0 ? i : i+1;
                if(k < sizeof(video_modes)/sizeof(video_modes[0])){
                    const char *s = video_descriptions[k];
                    CX = video_modes[k];
                    copy_to_guest((uint32_t)DATA_SEG*16 + DATA_OFF,
                                  (const uint8_t *)s, strlen(s) + 1);
                    set_sreg(S_DS, DATA_SEG); DX = DATA_OFF;
                }
                break;
            }
        if(!CX){ set_sreg(S_DS, 0); DX = 0; }
        break; }
    case 0x002A: AX = (uint16_t)m.visible; BX = m.hot_x; CX = m.hot_y; DX = 4; break;
    case 0x002B:
        if(BX != 0xFFFF && (BX < 1 || BX > 4)){ AX = 0xFFFF; break; }
        if(BX == 0xFFFF) default_profiles();
        else { copy_from_guest(m.profiles, cpu.sbase[S_ES] + SI, sizeof(m.profiles)); m.profile = BX; }
        AX = 0; break;
    case 0x002C: AX = 0; BX = m.profile; profile_to_rom(); set_sreg(S_ES, DATA_SEG); SI = DATA_OFF; break;
    case 0x002D:
        if(BX != 0xFFFF && (BX < 1 || BX > 4)){ AX = 0xFFFE; set_sreg(S_ES, 0); SI = 0; break; }
        if(BX != 0xFFFF) m.profile = BX;
        copy_to_guest((uint32_t)DATA_SEG*16 + DATA_OFF, &m.profile_names[(m.profile-1)*16], 16);
        AX = 0; BX = m.profile; set_sreg(S_ES, DATA_SEG); SI = DATA_OFF; break;
    case 0x002E:
        if(BL) copy_to_guest(cpu.sbase[S_ES] + SI, m.profile_names, sizeof(m.profile_names));
        else copy_from_guest(m.profile_names, cpu.sbase[S_ES] + SI, sizeof(m.profile_names));
        AX = 0; break;
    case 0x002F: reset_driver(1); AX = 0xFFFF; BX = MOUSE_BUTTONS; break;
    case 0x0030: AX = 0xFFFF; break;
    case 0x0031: AX = m.min_x; BX = m.min_y; CX = m.max_x; DX = m.max_y; break;
    case 0x0032: AX = 0xFFF8; BX = 0; CX = 0xE000; DX = 0; break;
    case 0x0033:
        AX = 0;
        if(CX == 0) CX = 0x154;
        else { n = CX < 0x154 ? CX : 0x154; settings_to_guest(cpu.sbase[S_ES] + DX, (uint16_t)n); CX = (uint16_t)n; }
        break;
    case 0x0034: AX = 0xFFFF; set_sreg(S_ES, 0); DX = 0; break;
    case 0x0035:
        if(BX != 0xFFFF){ m.lcd_style = BH; m.lcd_size = BL; m.lcd_threshold = CH; m.lcd_active = CL; m.lcd_delay = DX; }
        AX = 0; BH = (uint8_t)m.lcd_style; BL = (uint8_t)m.lcd_size;
        CH = (uint8_t)m.lcd_threshold; CL = (uint8_t)m.lcd_active; DX = m.lcd_delay; break;
    case 0x004D: {
        static const uint8_t s[] = "doskit INT 33h mouse driver";
        copy_to_guest((uint32_t)DATA_SEG*16 + DATA_OFF, s, sizeof(s)); set_data_pointer(DATA_OFF); break; }
    case 0x006D:
        mem_w8((uint32_t)DATA_SEG*16 + DATA_OFF, 7); mem_w8((uint32_t)DATA_SEG*16 + DATA_OFF + 1, 5);
        set_data_pointer(DATA_OFF); break;
    default: AX = 0xFFFF; break;
    }
}

void mouse_overlay(uint32_t *pixels, int w, int h){
    int px, py, x, y, width, rows, hotx, hoty;
    uint32_t black = 0xFF000000u;
    if(!ready || !m.enabled || m.visible < 0 || w <= 0 || h <= 0) return;
    if(m.max_x == m.min_x || m.max_y == m.min_y) return;
    px = (int)(((uint32_t)(m.x - m.min_x) * (uint32_t)(w-1)) / (m.max_x - m.min_x));
    py = (int)(((uint32_t)(m.y - m.min_y) * (uint32_t)(h-1)) / (m.max_y - m.min_y));
    if(m.cursor_kind != 2){
        int cellw = 8, cellh = h >= 350 ? 16 : 8;
        px -= px % cellw; py -= py % cellh;
        for(y=0;y<cellh && py+y<h;y++) for(x=0;x<cellw && px+x<w;x++){
            uint32_t *p = &pixels[(py+y)*w + px+x]; *p = (*p ^ 0x00FFFFFFu) | 0xFF000000u;
        }
        return;
    }
    width = m.cursor_words * 16; rows = m.cursor_rows;
    hotx = (int16_t)m.hot_x; hoty = (int16_t)m.hot_y;
    px -= hotx; py -= hoty;
    for(y=0;y<rows;y++) for(x=0;x<width;x++){
        int sx = px+x, sy = py+y, k = y*m.cursor_words + x/16;
        uint16_t bit = (uint16_t)(0x8000u >> (x & 15));
        int screen = (m.screen_mask[k] & bit) != 0;
        int cursor = (m.cursor_mask[k] & bit) != 0;
        uint32_t *p;
        if(sx<0 || sy<0 || sx>=w || sy>=h || (screen && !cursor)) continue;
        p = &pixels[sy*w+sx];
        if(!screen) *p = black;
        if(cursor) *p = (*p ^ 0x00FFFFFFu) | 0xFF000000u;
    }
}

void mouse_init(void){
    ready = 1;
    reset_driver(0);
    cb_table[0x33] = mouse_int33;
    cb_table[RETURN_CB] = callback_return;
    ram[(uint32_t)DATA_SEG*16 + RETURN_OFF + 0] = 0x0F;
    ram[(uint32_t)DATA_SEG*16 + RETURN_OFF + 1] = 0xFF;
    ram[(uint32_t)DATA_SEG*16 + RETURN_OFF + 2] = RETURN_CB;
    ram[(uint32_t)DATA_SEG*16 + RETURN_OFF + 3] = 0xF4;
}
