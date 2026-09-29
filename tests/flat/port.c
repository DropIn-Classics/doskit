/* port.c - the start of FLAT.386 translated to C over its own memory image
 * (pmem.h), as a protected-mode program is ported with the kit
 * (docs/METHOD.md, stage 3): the image is loaded from the file at a linear
 * address with a selector for each descriptor, its variables are reached by
 * the names of the hints (flat_names.h, tools/symmap.py), and the memory is
 * written out for tools/memcmp.py --base, as the runner's -mem writes it.
 *
 *     port IMAGE BASE MEM_OUT          (BASE in hex)
 */
#include <stdio.h>
#include <stdlib.h>
#include "flat_names.h"
#include "pmem.h"

enum {
#define X(seg, name, addr) N_##name = addr,
    FLAT_NAMES(X)
#undef X
};

/* the selectors the test gives descriptors 0 and 1 */
static const uint16_t sels[2] = { 0x1C, 0x24 };

int main(int argc, char **argv)
{
    char err[256];
    PmImage img;

    if (argc != 4) {
        fprintf(stderr, "usage: port IMAGE BASE MEM_OUT\n");
        return 2;
    }
    if (pm_load(argv[1], FLAT_SIZE, FLAT_SHA256, (uint32_t)strtoul(argv[2], NULL, 16),
                sels, 2, &img, err, sizeof err) != 0) {
        fprintf(stderr, "port: %s\n", err);
        return 1;
    }
    if (img.entry != img.desc[FLAT_CODE].base - img.base + N_START) {
        fprintf(stderr, "port: the entry is not START\n");
        return 1;
    }
    /* START: DS and SAVED_DS the selector of CODE; COUNT from MORE's OTHER
     * through ES */
    pm_ds = img.desc[FLAT_CODE].base;
    ww(N_SAVED_DS, img.desc[FLAT_CODE].sel);
    wd(N_COUNT, lrd(img.desc[FLAT_MORE].base + N_OTHER));
    printf("port: COUNT %08X, entry %X\n", (unsigned)rd(N_COUNT), (unsigned)img.entry);
    if (pm_write(argv[3]) != 0) {
        fprintf(stderr, "port: %s cannot be written\n", argv[3]);
        return 1;
    }
    return 0;
}
