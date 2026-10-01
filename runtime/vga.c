/* vga.c - see vga.h.  Register meanings follow the IBM VGA documentation
 * (FreeVGA's register reference is a good one to read along). */
#include "vga.h"
#include <stdio.h>
#include <string.h>

static struct {
    uint8_t mem[4][VGA_PLANE_SIZE];
    uint8_t latch[4];

    uint8_t misc;
    uint8_t seq[5], seq_index;
    uint8_t gc[9], gc_index;
    uint8_t crtc[25], crtc_index;
    uint8_t attr[21], attr_index, attr_flipflop;

    uint8_t dac[256][3];
    uint8_t dac_write_index, dac_write_comp;
    uint8_t dac_read_index, dac_read_comp;
    uint8_t pel_mask;

    int beam;                             /* virtual scan line for 3DAh */
    int start_latch;                      /* vga_set_start_latch */
    uint8_t start_hi, start_lo;           /* CRTC 0Ch, 0Dh as taken at the retrace */
    int vesa_w;                           /* the VESA mode's width, 0 in a VGA mode */
} v;

/* ---- helpers derived from the registers ---- */

static int total_scanlines(void)
{
    int vt = v.crtc[0x06] | ((v.crtc[0x07] & 0x01) << 8) | ((v.crtc[0x07] & 0x20) << 4);
    return vt + 2;
}

static int display_scanlines(void)
{
    int vde = v.crtc[0x12] | ((v.crtc[0x07] & 0x02) << 7) | ((v.crtc[0x07] & 0x40) << 3);
    return vde + 1;
}

static int line_compare(void)
{
    return v.crtc[0x18] | ((v.crtc[0x07] & 0x10) << 4) | ((v.crtc[0x09] & 0x40) << 3);
}

static int retrace_start(void)
{
    return v.crtc[0x10] | ((v.crtc[0x07] & 0x04) << 6) | ((v.crtc[0x07] & 0x80) << 2);
}

static int chain4(void)
{
    return (v.seq[4] & 0x08) != 0;
}

/* ---- I/O ports ---- */

void vga_outb(uint16_t port, uint8_t value)
{
    switch (port) {
    case 0x3C0:
        if (!v.attr_flipflop) {
            v.attr_index = value & 0x1F;
        } else if (v.attr_index < sizeof v.attr) {
            v.attr[v.attr_index] = value;
        }
        v.attr_flipflop ^= 1;
        break;
    case 0x3C2: v.misc = value; break;
    case 0x3C4: v.seq_index = value; break;
    case 0x3C5:
        if (v.seq_index < sizeof v.seq)
            v.seq[v.seq_index] = value;
        break;
    case 0x3C6: v.pel_mask = value; break;
    case 0x3C7: v.dac_read_index = value; v.dac_read_comp = 0; break;
    case 0x3C8: v.dac_write_index = value; v.dac_write_comp = 0; break;
    case 0x3C9:
        v.dac[v.dac_write_index][v.dac_write_comp] = value & 0x3F;
        if (++v.dac_write_comp == 3) {
            v.dac_write_comp = 0;
            v.dac_write_index++;
        }
        break;
    case 0x3CE: v.gc_index = value; break;
    case 0x3CF:
        if (v.gc_index < sizeof v.gc)
            v.gc[v.gc_index] = value;
        break;
    case 0x3D4: v.crtc_index = value; break;
    case 0x3D5:
        if (v.crtc_index < sizeof v.crtc) {
            /* registers 0-7 are write protected while 11h bit 7 is set,
             * except for the line compare bit in register 7 */
            if (v.crtc_index <= 7 && (v.crtc[0x11] & 0x80)) {
                if (v.crtc_index == 7)
                    v.crtc[7] = (uint8_t)((v.crtc[7] & ~0x10) | (value & 0x10));
            } else {
                v.crtc[v.crtc_index] = value;
            }
        }
        break;
    default:
        break;
    }
}

void vga_outw(uint16_t port, uint16_t value)
{
    vga_outb(port, (uint8_t)value);
    vga_outb((uint16_t)(port + 1), (uint8_t)(value >> 8));
}

uint8_t vga_inb(uint16_t port)
{
    uint8_t r;

    switch (port) {
    case 0x3C1: return v.attr_index < sizeof v.attr ? v.attr[v.attr_index] : 0;
    case 0x3C4: return v.seq_index;
    case 0x3C5: return v.seq_index < sizeof v.seq ? v.seq[v.seq_index] : 0;
    case 0x3C6: return v.pel_mask;
    case 0x3C8: return v.dac_write_index;
    case 0x3C9:
        r = v.dac[v.dac_read_index][v.dac_read_comp];
        if (++v.dac_read_comp == 3) {
            v.dac_read_comp = 0;
            v.dac_read_index++;
        }
        return r;
    case 0x3CC: return v.misc;
    case 0x3CE: return v.gc_index;
    case 0x3CF: return v.gc_index < sizeof v.gc ? v.gc[v.gc_index] : 0;
    case 0x3D4: return v.crtc_index;
    case 0x3D5: return v.crtc_index < sizeof v.crtc ? v.crtc[v.crtc_index] : 0;
    case 0x3DA: {
        /* bit 3: vertical retrace, bit 0: display disabled.  The beam moves
         * on by one scan line per read. */
        int total = total_scanlines();
        int rs = retrace_start();
        int line = v.beam;
        v.beam = (v.beam + 1) % (total > 0 ? total : 1);
        v.attr_flipflop = 0;
        r = 0;
        if (line >= display_scanlines())
            r |= 0x01;
        if (line >= rs && line < rs + 2 + (v.crtc[0x11] & 0x0F))
            r |= 0x08;
        return r;
    }
    default:
        return 0xFF;
    }
}

void vga_frame_start(void)
{
    v.beam = 0;
    v.start_hi = v.crtc[0x0C];
    v.start_lo = v.crtc[0x0D];
}

void vga_set_start_latch(int on)
{
    v.start_latch = on;
    v.start_hi = v.crtc[0x0C];
    v.start_lo = v.crtc[0x0D];
}

/* ---- CPU access to A000h ---- */

static uint8_t alu(uint8_t value, uint8_t latch)
{
    switch ((v.gc[3] >> 3) & 3) {
    case 1: return value & latch;
    case 2: return value | latch;
    case 3: return value ^ latch;
    default: return value;
    }
}

void vga_write(uint16_t offset, uint8_t value)
{
    int p;
    uint8_t mask = v.seq[2] & 0x0F;
    uint16_t addr = offset;
    int mode = v.gc[5] & 3;

    if (chain4()) {
        /* chain-4: the low two address bits pick the plane */
        mask &= (uint8_t)(1 << (offset & 3));
        addr = (uint16_t)(offset & ~3);
    }

    for (p = 0; p < 4; p++) {
        uint8_t out, bitmask = v.gc[8];
        if (!(mask & (1 << p)))
            continue;
        switch (mode) {
        case 1:                                  /* latch copy */
            v.mem[p][addr] = v.latch[p];
            continue;
        case 2:                                  /* colour from the value's bits */
            out = (value & (1 << p)) ? 0xFF : 0x00;
            out = alu(out, v.latch[p]);
            break;
        case 3: {                                /* set/reset, value ANDs the bit mask */
            uint8_t rot = (uint8_t)((value >> (v.gc[3] & 7)) | (value << (8 - (v.gc[3] & 7))));
            bitmask &= rot;
            out = (v.gc[0] & (1 << p)) ? 0xFF : 0x00;
            break;
        }
        default: {                               /* mode 0 */
            uint8_t rot = (uint8_t)((value >> (v.gc[3] & 7)) | (value << (8 - (v.gc[3] & 7))));
            out = rot;
            if (v.gc[1] & (1 << p))
                out = (v.gc[0] & (1 << p)) ? 0xFF : 0x00;
            out = alu(out, v.latch[p]);
            break;
        }
        }
        v.mem[p][addr] = (uint8_t)((out & bitmask) | (v.latch[p] & ~bitmask));
    }
}

uint8_t vga_read(uint16_t offset)
{
    int p;
    uint16_t addr = chain4() ? (uint16_t)(offset & ~3) : offset;

    for (p = 0; p < 4; p++)
        v.latch[p] = v.mem[p][addr];

    if (v.gc[5] & 0x08) {                        /* read mode 1: colour compare */
        uint8_t r = 0xFF;
        for (p = 0; p < 4; p++) {
            if (!(v.gc[7] & (1 << p)))
                continue;
            r &= (v.gc[2] & (1 << p)) ? v.latch[p] : (uint8_t)~v.latch[p];
        }
        return r;
    }
    if (chain4())
        return v.latch[offset & 3];
    return v.latch[v.gc[4] & 3];
}

/* ---- mode set ---- */

void vga_set_mode(int mode)
{
    static const uint8_t seq13[5] = {0x03, 0x01, 0x0F, 0x00, 0x0E};
    static const uint8_t crtc13[25] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0x41, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF};
    static const uint8_t gc13[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF};
    static const uint8_t attr13[21] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
        0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00, 0x0F, 0x00, 0x00};
    static const uint8_t seq12[5] = {0x03, 0x01, 0x0F, 0x00, 0x06};
    static const uint8_t crtc12[25] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xEA, 0x8C, 0xDF, 0x28, 0x00, 0xE7, 0x04, 0xE3, 0xFF};
    static const uint8_t gc12[9] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0F, 0xFF};
    static const uint8_t attr12[21] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3A,
        0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x01, 0x00, 0x0F, 0x00, 0x00};
    /* the 16-colour 200-line modes 0Dh (320 wide) and 0Eh (640 wide):
     * planar, scan-doubled; the tables the runner's BIOS mode set uses
     * (tools/run/vga.c), the graphics controller as mode 12h's */
    static const uint8_t seq0d[5] = {0x03, 0x09, 0x0F, 0x00, 0x06};
    static const uint8_t crtc0d[25] = {
        0x2D, 0x27, 0x28, 0x90, 0x2B, 0x80, 0xBF, 0x1F, 0x00, 0xC0, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x9C, 0x8E, 0x8F, 0x14, 0x00, 0x96, 0xB9, 0xE3, 0xFF};
    static const uint8_t seq0e[5] = {0x03, 0x01, 0x0F, 0x00, 0x06};
    static const uint8_t crtc0e[25] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0xC0, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x9C, 0x8E, 0x8F, 0x28, 0x00, 0x96, 0xB9, 0xE3, 0xFF};
    static const uint8_t attr0d[21] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x12,
        0x13, 0x14, 0x15, 0x16, 0x17, 0x01, 0x00, 0x0F, 0x00, 0x00};
    int m12 = mode == 0x12, m0d = mode == 0x0D || mode == 0x0E;

    memset(&v, 0, sizeof v);
    v.misc = m12 ? 0xE3 : 0x63;
    memcpy(v.seq, m12 ? seq12 : mode == 0x0D ? seq0d : mode == 0x0E ? seq0e : seq13, sizeof v.seq);
    memcpy(v.crtc, m12 ? crtc12 : mode == 0x0D ? crtc0d : mode == 0x0E ? crtc0e : crtc13,
           sizeof v.crtc);
    memcpy(v.gc, m12 || m0d ? gc12 : gc13, sizeof v.gc);
    memcpy(v.attr, m12 ? attr12 : m0d ? attr0d : attr13, sizeof v.attr);
    v.pel_mask = 0xFF;
    /* modes 12h, 0Dh and 0Eh: the BIOS's 64 EGA colours in DAC 0-3Fh
     * (bits 0-2 blue, green, red at 2/3, bits 3-5 at 1/3), which the
     * attribute registers above pick the 16 from */
    if (m12 || m0d) {
        int i;
        for (i = 0; i < 0x40; i++) {
            v.dac[i][0] = (uint8_t)((i >> 2 & 1) * 42 + (i >> 5 & 1) * 21);
            v.dac[i][1] = (uint8_t)((i >> 1 & 1) * 42 + (i >> 4 & 1) * 21);
            v.dac[i][2] = (uint8_t)((i & 1) * 42 + (i >> 3 & 1) * 21);
        }
    }
}

/* the runner's tables (tools/run/vga.c): 70, 60 and 60 Hz */
static const uint8_t crtc_v100[25] = {
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x9C, 0x8E, 0x8F, 0x50, 0x40, 0x96, 0xB9, 0xA3, 0xFF};
static const uint8_t crtc_v101[25] = {
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xEA, 0x8C, 0xDF, 0x50, 0x40, 0xE7, 0x04, 0xA3, 0xFF};
/* 800x600: 1056 x 628 dots of 40 MHz */
static const uint8_t crtc_v103[25] = {
    0x7F, 0x63, 0x64, 0x82, 0x69, 0x19, 0x72, 0xF0, 0x00, 0x60, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x59, 0x8D, 0x57, 0x64, 0x40, 0x58, 0x70, 0xA3, 0xFF};

int vga_set_mode_vesa(int mode, int clear)
{
    static const uint8_t seq[5] = {0x03, 0x01, 0x0F, 0x00, 0x0E};
    const uint8_t *crtc;
    uint8_t misc;
    int w, p;
    static uint8_t mem[4][VGA_PLANE_SIZE];

    switch (mode) {
    case 0x100: crtc = crtc_v100; misc = 0x63; w = 640; break;
    case 0x101: crtc = crtc_v101; misc = 0xE3; w = 640; break;
    case 0x103: crtc = crtc_v103; misc = 0x2B; w = 800; break;
    default: return 0;
    }
    if (!clear)
        memcpy(mem, v.mem, sizeof mem);
    vga_set_mode(0x13);
    if (!clear)
        memcpy(v.mem, mem, sizeof mem);
    for (p = 0; p < 5; p++)
        v.seq[p] = seq[p];
    memcpy(v.crtc, crtc, sizeof v.crtc);
    v.misc = misc;
    v.vesa_w = w;
    return 1;
}

/* ---- scan-out ---- */

double vga_refresh_hz(void)
{
    int cs = (v.misc >> 2) & 3;
    double dotclock = cs == 1 ? 28322000.0 : cs >= 2 && v.vesa_w ? 40000000.0 : 25175000.0;
    int chardots = (v.seq[1] & 0x01) ? 8 : 9;
    int htotal = v.crtc[0] + 5;
    int vtotal = total_scanlines();

    if (v.seq[1] & 0x08)
        dotclock /= 2;
    if (htotal <= 0 || vtotal <= 0)
        return 60.0;
    return dotclock / ((double)chardots * htotal * vtotal);
}

/* the DAC entry for a 4-bit colour in the 16-colour modes */
static uint8_t attr_color(int c)
{
    uint8_t pal = v.attr[c & 0x0F];
    if (v.attr[0x10] & 0x80)                    /* P5-4 from the colour select register */
        return (uint8_t)(((v.attr[0x14] & 0x0C) << 4) | ((v.attr[0x14] & 0x03) << 4) | (pal & 0x0F));
    return (uint8_t)(((v.attr[0x14] & 0x0C) << 4) | (pal & 0x3F));
}

void vga_render(VgaFrame *f)
{
    int i, s, x;
    int scanlines = display_scanlines();
    int per_row = (v.crtc[0x09] & 0x1F) + 1;     /* scan lines per character row */
    int dscan = (v.crtc[0x09] & 0x80) ? 2 : 1;   /* double scan */
    int repeat = per_row * dscan;                /* scan lines before the next row address */
    int c256 = (v.gc[0x05] & 0x40) != 0;         /* 256-colour shift mode */
    int char_px = c256 && !v.vesa_w ? 4 : 8;     /* pixels a character clock */
    /* the picture ends at the horizontal display end */
    int width = (v.crtc[0x01] + 1) * char_px;
    unsigned stride = v.crtc[0x13] * 2u;         /* address counter step per row */
    int shift = (v.crtc[0x14] & 0x40) ? 2 : (v.crtc[0x17] & 0x40) ? 0 : 1;
    int lc = line_compare();
    unsigned row_addr = v.start_latch ? ((unsigned)v.start_hi << 8 | v.start_lo)
                                      : ((unsigned)v.crtc[0x0C] << 8 | v.crtc[0x0D]);
    int row_line = 0, in_split = 0, out_y = 0;

    int left = 0;                                /* the picture's first pixel in the frame */

    if (width <= 0 || width > VGA_MAX_W)
        width = width <= 0 ? (v.crtc[0x01] + 1) * char_px : VGA_MAX_W;
    if (v.vesa_w && width < v.vesa_w) {
        left = (v.vesa_w - width) / 2;
        width = v.vesa_w;
    }

    /* one output row per row of memory (per `repeat` scan lines): the
     * picture as the game draws it */
    for (s = 0; s < scanlines && out_y < VGA_MAX_H; s++) {
        if (s == lc + 1) {
            /* split screen: the address counter starts over at 0 */
            row_addr = 0;
            row_line = 0;
            in_split = 1;
        }
        if (row_line == 0) {
            uint8_t *dst = f->pixels + out_y * width;
            int pan = c256 ? (v.attr[0x13] & 7) >> 1 : (v.attr[0x13] & 7);
            int p0 = (in_split && (v.attr[0x10] & 0x20)) ? 0 : pan;
            int shown = (v.crtc[0x01] + 1) * char_px;
            if (shown > width - left)
                shown = width - left;
            for (x = 0; x < left; x++)
                dst[x] = v.attr[0x11];                            /* overscan */
            dst += left;
            if (c256) {
                for (x = 0; x < shown; x++) {
                    int px = x + p0;
                    unsigned addr = ((row_addr + (unsigned)(px >> 2)) << shift) & 0xFFFF;
                    dst[x] = (uint8_t)(v.mem[px & 3][addr] & v.pel_mask);
                }
            } else {
                /* 16 colours: bit 7 of a byte is the leftmost of its 8
                 * pixels, one bit from each plane, then the attribute
                 * controller's palette picks the DAC entry */
                for (x = 0; x < shown; x++) {
                    int px = x + p0, bit = 7 - (px & 7), p, c = 0;
                    unsigned addr = ((row_addr + (unsigned)(px >> 3)) << shift) & 0xFFFF;
                    for (p = 0; p < 4; p++)
                        c |= ((v.mem[p][addr] >> bit) & 1) << p;
                    c &= v.attr[0x12] & 0x0F;                     /* colour plane enable */
                    dst[x] = (uint8_t)(attr_color(c) & v.pel_mask);
                }
            }
            for (x = shown; x < width - left; x++)
                dst[x] = v.attr[0x11];                            /* overscan */
            out_y++;
        }
        if (++row_line >= repeat) {
            row_line = 0;
            row_addr += stride;
        }
    }

    f->width = width;
    f->height = out_y;
    for (i = 0; i < 256; i++) {
        uint32_t r = v.dac[i][0], g = v.dac[i][1], b = v.dac[i][2];
        r = (r << 2) | (r >> 4);
        g = (g << 2) | (g >> 4);
        b = (b << 2) | (b >> 4);
        f->palette[i] = (r << 16) | (g << 8) | b;
        if (v.seq[1] & 0x20)
            f->palette[i] = 0;                   /* screen off (INT 10h AH=12h BL=36h) */
    }
}

int vga_write_planes(const char *path)
{
    FILE *f = fopen(path, "wb");
    long a;
    int ok = f != NULL;

    for (a = 0; ok && a < 4L * VGA_PLANE_SIZE; a++)
        ok = fputc(v.mem[a & 3][a >> 2], f) != EOF;
    if (f && fclose(f) != 0)
        ok = 0;
    return ok ? 0 : -1;
}

uint8_t vga_plane_byte(int plane, uint16_t addr)
{
    return v.mem[plane & 3][addr];
}

uint8_t vga_crtc_reg(int index)
{
    return index >= 0 && index < (int)sizeof v.crtc ? v.crtc[index] : 0;
}
