#include "fslog.h"

unsigned fsl_strlen(const char *s)
{
    unsigned n = 0;
    while (s[n]) n++;
    return n;
}

#define STATIC_DESC(size, id) (((size) << 14) | ((id) << 10) | 2u)

void fsl_build_initialize(unsigned *cb)
{
    cb[0] = FSL_CMD_INITIALIZE; cb[1] = 0x20u; cb[2] = 0;
}

void fsl_build_open_archive(unsigned *cb, unsigned archive_id, unsigned empty_ptr)
{
    cb[0] = FSL_CMD_OPEN_ARCHIVE;
    cb[1] = archive_id; cb[2] = FSL_PATH_EMPTY; cb[3] = 1u;
    cb[4] = STATIC_DESC(1u, 0u); cb[5] = empty_ptr;
}

void fsl_build_close_archive(unsigned *cb, unsigned lo, unsigned hi)
{
    cb[0] = FSL_CMD_CLOSE_ARCHIVE; cb[1] = lo; cb[2] = hi;
}

void fsl_build_create_dir(unsigned *cb, unsigned alo, unsigned ahi, unsigned path_ptr, unsigned path_size)
{
    cb[0] = FSL_CMD_CREATE_DIR;
    cb[1] = 0; cb[2] = alo; cb[3] = ahi;
    cb[4] = FSL_PATH_ASCII; cb[5] = path_size; cb[6] = 0;
    cb[7] = STATIC_DESC(path_size, 0u); cb[8] = path_ptr;
}

void fsl_build_open_file(unsigned *cb, unsigned archive_id, unsigned empty_ptr,
                         unsigned path_ptr, unsigned path_size, unsigned flags)
{
    cb[0] = FSL_CMD_OPEN_FILE_DIR;
    cb[1] = 0; cb[2] = archive_id;
    cb[3] = FSL_PATH_EMPTY; cb[4] = 1u;
    cb[5] = FSL_PATH_ASCII; cb[6] = path_size;
    cb[7] = flags; cb[8] = 0;
    cb[9] = STATIC_DESC(1u, 2u); cb[10] = empty_ptr;          /* archive path: static buffer id 2 */
    cb[11] = STATIC_DESC(path_size, 0u); cb[12] = path_ptr;   /* file path: static buffer id 0 */
}

void fsl_build_write(unsigned *cb, unsigned off_lo, unsigned off_hi, unsigned buf_ptr,
                     unsigned size, unsigned option)
{
    cb[0] = FSL_CMD_FILE_WRITE;
    cb[1] = off_lo; cb[2] = off_hi; cb[3] = size; cb[4] = option;
    cb[5] = (size << 4) | 10u;                                /* read-only buffer translate descriptor */
    cb[6] = buf_ptr;
}

void fsl_build_set_size(unsigned *cb, unsigned lo, unsigned hi)
{
    cb[0] = FSL_CMD_FILE_SETSIZE; cb[1] = lo; cb[2] = hi;
}

void fsl_build_close_file(unsigned *cb)
{
    cb[0] = FSL_CMD_FILE_CLOSE;
}

static char *put_str(char *p, const char *s) { while (*s) *p++ = *s++; return p; }

static char *put_hexw(char *p, unsigned v, int digits)
{
    int i;
    for (i = digits - 1; i >= 0; i--) {
        unsigned d = (v >> (i * 4)) & 0xFu;
        *p++ = (char)(d < 10 ? '0' + d : 'A' + d - 10);
    }
    return p;
}


static int finish(char *out, char *p)
{
    *p++ = '\r'; *p++ = '\n'; *p = 0;
    return (int)(p - out);
}

int fsl_line_tick(char *out, unsigned tick_lo, unsigned tick_hi, unsigned dir_res)
{
    char *p = put_str(out, "tick=");
    p = put_hexw(p, tick_hi, 8); p = put_hexw(p, tick_lo, 8);
    p = put_str(p, " dir="); p = put_hexw(p, dir_res, 8);
    return finish(out, p);
}

int fsl_line_addrs(char *out, unsigned io, unsigned gs, unsigned ld, unsigned tb)
{
    char *p = put_str(out, "io=");
    p = put_hexw(p, io, 8); p = put_str(p, " gs="); p = put_hexw(p, gs, 8);
    p = put_str(p, " ld="); p = put_hexw(p, ld, 8); p = put_str(p, " tb="); p = put_hexw(p, tb, 8);
    return finish(out, p);
}

int fsl_line_status(char *out, unsigned s0, unsigned s1, unsigned s2, unsigned s3, unsigned hook)
{
    char *p = put_str(out, "scan=");
    p = put_hexw(p, s0, 1); p = put_hexw(p, s1, 1); p = put_hexw(p, s2, 1); p = put_hexw(p, s3, 1);
    p = put_str(p, " hook="); p = put_hexw(p, hook, 1);
    return finish(out, p);
}

int fsl_format_sd_error(char *out, int stage, unsigned res)
{
    char *p = put_str(out, "sd stage=");
    p = put_hexw(p, (unsigned)stage & 0xFu, 1);
    p = put_str(p, " res=0x"); p = put_hexw(p, res, 8);
    *p = 0;
    return (int)(p - out);
}
