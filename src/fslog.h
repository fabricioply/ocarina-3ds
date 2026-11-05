/* fs:USER raw IPC command builders + log line formatting for probe7-9. Pure C99, no libc, no division.
 * Every layout below was derived from the disassembly of libctru's fs.o (FSUSER_* / FSFILE_*), not guessed.
 * `cb` is the thread command buffer (TLS+0x80), at least 16 words. Pointers are passed as 32-bit addresses. */
#ifndef FSLOG_H
#define FSLOG_H

#define FSL_ARCHIVE_SDMC   9u
#define FSL_PATH_EMPTY     1u
#define FSL_PATH_ASCII     3u
#define FSL_OPEN_READ      1u
#define FSL_OPEN_WRITE     2u
#define FSL_OPEN_CREATE    4u
#define FSL_WRITE_FLUSH    1u

#define FSL_CMD_INITIALIZE     0x08010002u
#define FSL_CMD_OPEN_FILE_DIR  0x08030204u
#define FSL_CMD_OPEN_ARCHIVE   0x080C00C2u
#define FSL_CMD_CLOSE_ARCHIVE  0x080E0080u
#define FSL_CMD_CREATE_DIR     0x08090182u
#define FSL_CMD_FILE_WRITE     0x08030102u
#define FSL_CMD_FILE_SETSIZE   0x08050080u
#define FSL_CMD_FILE_CLOSE     0x08080000u

unsigned fsl_strlen(const char *s);

/* FSUSER_Initialize: [0]=cmd [1]=0x20 (process id placeholder) */
void fsl_build_initialize(unsigned *cb);
/* FSUSER_OpenArchive(id, PATH_EMPTY): [1]=id [2]=type [3]=size(1) [4]=1<<14|2 [5]=ptr to a zero byte.
 * reply: [1]=result, [2],[3] = archive handle lo,hi */
void fsl_build_open_archive(unsigned *cb, unsigned archive_id, unsigned empty_ptr);
/* FSUSER_CloseArchive: [1],[2] = archive handle lo,hi */
void fsl_build_close_archive(unsigned *cb, unsigned lo, unsigned hi);
/* FSUSER_CreateDirectory(archive, ASCII path, attributes 0): path_size includes the NUL */
void fsl_build_create_dir(unsigned *cb, unsigned alo, unsigned ahi, unsigned path_ptr, unsigned path_size);
/* FSUSER_OpenFileDirectly(archive id, PATH_EMPTY archive path, ASCII file path, flags, attributes 0).
 * reply: [1]=result, [3]=file handle */
void fsl_build_open_file(unsigned *cb, unsigned archive_id, unsigned empty_ptr,
                         unsigned path_ptr, unsigned path_size, unsigned flags);
/* FSFILE_Write on the file handle. reply: [1]=result, [2]=bytes written */
void fsl_build_write(unsigned *cb, unsigned off_lo, unsigned off_hi, unsigned buf_ptr,
                     unsigned size, unsigned option);
void fsl_build_set_size(unsigned *cb, unsigned lo, unsigned hi);
void fsl_build_close_file(unsigned *cb);

int fsl_line_tick(char *out, unsigned tick_lo, unsigned tick_hi, unsigned dir_res);
                                                                  /* "tick=HHHHHHHHLLLLLLLL dir=XXXXXXXX" */
int fsl_line_addrs(char *out, unsigned io, unsigned gs, unsigned ld, unsigned tb);
                                                                  /* "io=.. gs=.. ld=.. tb=.." (8 hex digits each) */
int fsl_line_status(char *out, unsigned s0, unsigned s1, unsigned s2, unsigned s3, unsigned hook);
                                                                  /* "scan=1111 hook=0" (scan code 0 missing, 1 found, 2 ambiguous) */
                                                                  /* "sd stage=N res=0x........" (no CRLF) */
int fsl_format_sd_error(char *out, int stage, unsigned res);
#endif
