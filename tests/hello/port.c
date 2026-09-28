/* port.c - HELLO.EXE translated to C over its own memory image, the way a
 * game is ported with the kit (docs/METHOD.md, stage 3): the program is
 * loaded from the file (rmem.h), its variables are reached by the names
 * of the hints (hello_names.h, written by tools/symmap.py), its video
 * memory is vga.c's.  At the end the memory is written out for
 * tools/memcmp.py, as the runner's -ram and -vram write the original's.
 *
 *     port GAME_DIR RAM_OUT VRAM_OUT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hello_names.h"
#include "platform.h"
#include "rmem.h"
#include "sys.h"
#include "vga.h"

/* the names of the hints: N_name is the offset in its segment */
enum {
#define X(seg, name, addr) N_##name = addr,
    HELLO_NAMES(X)
#undef X
};

static uint16_t seg_code, seg_data;

static void fill_band(void)
{
    uint16_t di;

    for (di = 0; di < 6400; di++)
        vga_write(di, 2);
    ww(N_counter, (uint16_t)(rw(N_counter) + 1));
}

static void fill_dot(void)
{
    vga_write(32160, 4);
    ww(N_counter, (uint16_t)(rw(N_counter) + 0x10));
}

/* a near call through a table of the program: the routine at that offset */
static void call_code(uint16_t off)
{
    if (off == N_fill_band)
        fill_band();
    else if (off == N_fill_dot)
        fill_dot();
    else {
        fprintf(stderr, "port: CODE:%04X not translated\n", off);
        exit(1);
    }
}

int main(int argc, char **argv)
{
    char dir[SYS_PATH], sub[SYS_PATH], path[SYS_PATH], err[256];
    uint16_t bx, off;
    VgaFrame *pic;

    if (argc != 4) {
        fprintf(stderr, "usage: port GAME_DIR RAM_OUT VRAM_OUT\n");
        return 2;
    }
    if (!sys_find_game(argv[1], NULL, "HELLO/HELLO.EXE", dir, sizeof dir) ||
        !sys_find(dir, "HELLO", sub, sizeof sub) || !sys_find(sub, "HELLO.EXE", path, sizeof path)) {
        fprintf(stderr, "port: no HELLO/HELLO.EXE in %s\n", argv[1]);
        return 1;
    }
    /* the program keeps 1000h paragraphs; nothing is allocated here */
    if (rm_load_exe(path, HELLO_SIZE, HELLO_SHA256, RM_LOAD_PSP, 0x1000, err, sizeof err)) {
        fprintf(stderr, "port: %s\n", err);
        return 1;
    }
    seg_code = RM_LOAD_PSP + 0x10;
    seg_data = seg_code + HELLO_DATA;
    rm_ds = seg_data;

    /* start: INT 21h AH=9 prints up to the '$' */
    printf("con: ");
    for (off = N_greeting; rb(off) != '$'; off++)
        putchar(rb(off) == '\r' ? ' ' : rb(off));
    vga_set_mode(0x13);
    for (bx = 0; bx < 2; bx++)                  /* next_step */
        call_code(rw((uint16_t)(N_steps + 2 * bx)));

    plat_init("HELLO");
    pic = malloc(sizeof *pic);
    vga_render(pic);
    plat_present(pic->pixels, pic->width, pic->height, pic->palette);
    plat_shutdown();
    free(pic);

    if (rm_write(argv[2]) || vga_write_planes(argv[3])) {
        fprintf(stderr, "port: cannot write %s or %s\n", argv[2], argv[3]);
        return 1;
    }
    return 0;
}
