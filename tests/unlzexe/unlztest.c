/* unlztest.c - runtime/unlzexe.c and rm_load_exe with a packed program
 * (tests/selftest.py):
 *
 *     unlztest PACKED.EXE UNPACKED.EXE
 *
 * PACKED is a program packed as LZEXE 0.91 lays a file out, UNPACKED what
 * tools/unlzexe.py made of it.  lzexe_unpack must give UNPACKED's bytes,
 * rm_load_exe the same memory for both, and a packed file cut short must
 * be refused by both. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rmem.h"
#include "sys.h"
#include "unlzexe.h"

static uint8_t first[MEM_SIZE];

int main(int argc, char **argv)
{
    char err[256], cut_path[SYS_PATH];
    size_t packed_size, plain_size, size;
    uint8_t *packed, *plain, *made;
    FILE *cut;

    if (argc != 3)
        return 2;
    packed = sys_load(argv[1], &packed_size);
    plain = sys_load(argv[2], &plain_size);
    if (!packed || !plain || !lzexe_packed(packed, packed_size) || lzexe_packed(plain, plain_size)) {
        printf("unlztest: the files are not a packed and a plain program\n");
        return 1;
    }
    made = lzexe_unpack(packed, packed_size, &size);
    if (!made || size != plain_size || memcmp(made, plain, size)) {
        printf("unlztest: lzexe_unpack does not give the unpacked file\n");
        return 1;
    }
    if (lzexe_unpack(packed, packed_size - 40, &size) || lzexe_unpack(plain, plain_size, &size)) {
        printf("unlztest: lzexe_unpack took a file cut short or not packed\n");
        return 1;
    }
    if (rm_load_exe(argv[2], 0, NULL, RM_LOAD_PSP, 0x1000, err, sizeof err)) {
        printf("unlztest: %s\n", err);
        return 1;
    }
    memcpy(first, mem, MEM_SIZE);
    if (rm_load_exe(argv[1], 0, NULL, RM_LOAD_PSP, 0x1000, err, sizeof err)
        || memcmp(first, mem, MEM_SIZE)) {
        printf("unlztest: the packed program is not loaded as the unpacked one\n");
        return 1;
    }
    snprintf(cut_path, sizeof cut_path, "%s.cut", argv[1]);
    cut = fopen(cut_path, "wb");
    if (!cut || fwrite(packed, 1, packed_size - 40, cut) != packed_size - 40 || fclose(cut)) {
        printf("unlztest: %s cannot be written\n", cut_path);
        return 1;
    }
    if (!rm_load_exe(cut_path, 0, NULL, RM_LOAD_PSP, 0x1000, err, sizeof err)) {
        printf("unlztest: rm_load_exe took a packed program cut short\n");
        return 1;
    }
    remove(cut_path);
    printf("unlztest ok: %lu bytes unpacked to %lu, loaded as the unpacked program\n",
           (unsigned long)packed_size, (unsigned long)plain_size);
    free(packed);
    free(plain);
    free(made);
    return 0;
}
