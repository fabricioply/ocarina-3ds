/* Host tests for fslog.c: IPC buffer layouts (cross-checked against libctru fs.o disassembly) and log lines. */
#include <stdio.h>
#include <string.h>
#include "fslog.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
#define CHECK_STR(a, b) do { if (strcmp((a), (b))) { printf("FAIL line %d: \"%s\" != \"%s\"\n", __LINE__, (a), (b)); fails++; } } while (0)

int main(void)
{
    unsigned cb[64];
    char out[256];
    int n;

    CHECK(fsl_strlen("") == 0);
    CHECK(fsl_strlen("/ocarina/log.txt") == 16);

    /* Initialize */
    memset(cb, 0xAA, sizeof cb);
    fsl_build_initialize(cb);
    CHECK(cb[0] == 0x08010002u && cb[1] == 0x20u && cb[2] == 0);

    /* OpenArchive(SDMC, empty) : 0x080C00C2 = 3 normal, 2 translate */
    fsl_build_open_archive(cb, 9, 0x1000);
    CHECK(cb[0] == 0x080C00C2u);
    CHECK(cb[1] == 9 && cb[2] == 1 && cb[3] == 1);
    CHECK(cb[4] == ((1u << 14) | 2u) && cb[5] == 0x1000);

    /* CloseArchive */
    fsl_build_close_archive(cb, 0x11, 0x22);
    CHECK(cb[0] == 0x080E0080u && cb[1] == 0x11 && cb[2] == 0x22);

    /* CreateDirectory("/ocarina", size 9): 0x08090182 = 6 normal, 2 translate */
    fsl_build_create_dir(cb, 0xAAAA, 0xBBBB, 0x2000, 9);
    CHECK(cb[0] == 0x08090182u);
    CHECK(cb[1] == 0 && cb[2] == 0xAAAA && cb[3] == 0xBBBB);
    CHECK(cb[4] == 3 && cb[5] == 9 && cb[6] == 0);
    CHECK(cb[7] == ((9u << 14) | 2u) && cb[8] == 0x2000);

    /* OpenFileDirectly: 0x08030204 = 8 normal, 4 translate */
    fsl_build_open_file(cb, 9, 0x1000, 0x3000, 17, FSL_OPEN_WRITE | FSL_OPEN_CREATE);
    CHECK(cb[0] == 0x08030204u);
    CHECK(cb[1] == 0 && cb[2] == 9);
    CHECK(cb[3] == 1 && cb[4] == 1);          /* archive path: PATH_EMPTY, size 1 */
    CHECK(cb[5] == 3 && cb[6] == 17);         /* file path: PATH_ASCII, size incl NUL */
    CHECK(cb[7] == 6 && cb[8] == 0);          /* flags write|create, attributes */
    CHECK(cb[9] == (0x802u | (1u << 14)) && cb[9] == ((1u << 14) | (2u << 10) | 2u) && cb[10] == 0x1000);
    CHECK(cb[11] == ((17u << 14) | 2u) && cb[12] == 0x3000);

    /* Write: 0x08030102 = 4 normal, 2 translate */
    fsl_build_write(cb, 0x100, 0, 0x4000, 50, FSL_WRITE_FLUSH);
    CHECK(cb[0] == 0x08030102u);
    CHECK(cb[1] == 0x100 && cb[2] == 0 && cb[3] == 50 && cb[4] == 1);
    CHECK(cb[5] == ((50u << 4) | 10u) && cb[6] == 0x4000);

    fsl_build_set_size(cb, 5, 0);
    CHECK(cb[0] == 0x08050080u && cb[1] == 5 && cb[2] == 0);
    fsl_build_close_file(cb);
    CHECK(cb[0] == 0x08080000u);

    /* header word sanity: command id / normal count / translate count */
    CHECK(((0x080C00C2u >> 6) & 0x3F) == 3 && (0x080C00C2u & 0x3F) == 2);
    CHECK(((0x08090182u >> 6) & 0x3F) == 6 && (0x08090182u & 0x3F) == 2);
    CHECK(((0x08030204u >> 6) & 0x3F) == 8 && (0x08030204u & 0x3F) == 4);
    CHECK(((0x08030102u >> 6) & 0x3F) == 4 && (0x08030102u & 0x3F) == 2);

    /* lines */
    n = fsl_line_tick(out, 0x89ABCDEFu, 0x01234567u, 0xC82044BEu);
    CHECK_STR(out, "tick=0123456789ABCDEF dir=C82044BE\r\n"); CHECK(n == (int)strlen(out));
    n = fsl_line_addrs(out, 0x41E968, 0x3769D8, 0x2CD2B4, 0x367C7C);
    CHECK_STR(out, "io=0041E968 gs=003769D8 ld=002CD2B4 tb=00367C7C\r\n"); CHECK(n == (int)strlen(out));
    n = fsl_line_status(out, 1, 1, 1, 2, 0);
    CHECK_STR(out, "scan=1112 hook=0\r\n"); CHECK(n == (int)strlen(out));
    n = fsl_format_sd_error(out, 7, 0xC8804478u);
    CHECK_STR(out, "sd stage=7 res=0xC8804478"); CHECK(n == (int)strlen(out));
    n = fsl_format_sd_error(out, 0xB, 0xE0E01BF5u);
    CHECK_STR(out, "sd stage=B res=0xE0E01BF5");
    CHECK(strlen(out) < 95);

    printf(fails ? "FAILED (%d)\n" : "fslog: all tests passed\n", fails);
    return fails ? 1 : 0;
}
