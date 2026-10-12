/* memtest.c - the DOS memory blocks of runtime/rmem.c (tests/selftest.py):
 * the program's own block and blocks behind it allocated, freed and
 * resized as INT 21h 48h, 49h and 4Ah do.
 *
 *     memtest PROGRAM.EXE
 */
#include <stdio.h>
#include "rmem.h"

#define PSP RM_LOAD_PSP
#define KEPT 0x1000u

static int failed;

/* a block's header: its kind, owner and size */
static void expect(const char *what, uint16_t mcb, char kind, uint16_t owner, uint16_t size)
{
    if (frb(mcb, 0) != (uint8_t)kind || frw(mcb, 1) != owner || frw(mcb, 3) != size) {
        printf("memtest: %s: block at %04X is %c owner %04X size %04X, not %c %04X %04X\n", what,
               mcb, frb(mcb, 0), frw(mcb, 1), frw(mcb, 3), kind, owner, size);
        failed = 1;
    }
}

int main(int argc, char **argv)
{
    char err[256];
    uint16_t a, b, most = 0, top_free = (uint16_t)(RM_TOP - (PSP + KEPT) - 1);

    if (argc != 2 || rm_load_exe(argv[1], 0, NULL, PSP, KEPT, err, sizeof err)) {
        printf("memtest: the program was not loaded\n");
        return 1;
    }
    expect("loaded", PSP - 1, 'M', PSP, KEPT);
    expect("loaded", PSP + KEPT, 'Z', 0, top_free);

    /* the program's block grown and shrunk: the free block follows it */
    if (dos_resize(PSP, KEPT + 0x40, &most) || most != KEPT + 0x40)
        failed = 1;
    expect("grown", PSP - 1, 'M', PSP, KEPT + 0x40);
    expect("grown", PSP + KEPT + 0x40, 'Z', 0, (uint16_t)(top_free - 0x40));
    if (dos_resize(PSP, KEPT, &most))
        failed = 1;
    expect("shrunk", PSP - 1, 'M', PSP, KEPT);
    expect("shrunk", PSP + KEPT, 'Z', 0, top_free);

    /* more than there is: refused, with what there is */
    if (!dos_resize(PSP, 0xA000, &most) || most != (uint16_t)(RM_TOP - PSP)) {
        printf("memtest: a block larger than memory was given (most %04X)\n", most);
        failed = 1;
    }
    expect("refused", PSP - 1, 'M', PSP, KEPT);

    /* a block behind an allocated one cannot grow, and shrinks */
    a = dos_alloc(0x100);
    b = dos_alloc(0x100);
    if (a != PSP + KEPT + 1 || b != a + 0x101) {
        printf("memtest: dos_alloc gave %04X and %04X\n", a, b);
        failed = 1;
    }
    if (!dos_resize(a, 0x200, &most) || most != 0x100)
        failed = 1;
    if (!dos_resize(PSP, KEPT + 1, &most) || most != KEPT)
        failed = 1;
    if (dos_resize(a, 0x80, &most))
        failed = 1;
    expect("first shrunk", a - 1, 'M', PSP, 0x80);
    expect("first shrunk", a + 0x80, 'M', 0, 0x7F);
    if (dos_resize(a, 0x100, &most))
        failed = 1;
    expect("first grown again", a - 1, 'M', PSP, 0x100);
    expect("second", b - 1, 'M', PSP, 0x100);
    dos_free(b);
    if (dos_resize(a, 0x180, &most))
        failed = 1;
    expect("first into the freed", a - 1, 'M', PSP, 0x180);
    expect("first into the freed", a + 0x180, 'Z', 0, (uint16_t)(top_free - 0x181));

    if (failed)
        return 1;
    printf("memtest ok: the program's block and blocks behind it resized\n");
    return 0;
}
