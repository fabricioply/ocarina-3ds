/* Native dialog log lines (from probe10) (CRLF terminated, NUL terminated output, return the length). Pure C99, no libc, no division. */
#ifndef DLGLOG_H
#define DLGLOG_H

/* "dlg arm t=12345 st=06" */
int dlglog_line_arm(char *out, unsigned t_ms, int st);
/* "dlg t=12345 phase=OPENING st=01 opt=0" (opt -- if > 1) */
int dlglog_line_trans(char *out, unsigned t_ms, const char *phase, int st, unsigned opt);
/* "dlg closeprev t=.. st=06 core_saved=1" */
int dlglog_line_closeprev(char *out, unsigned t_ms, int st);
/* "dlg open t=.. patch=0 lang=1 textlen=41 oldsize=60 loader_ret=1 st_after=01" */
int dlglog_line_open(char *out, unsigned t_ms, unsigned patch, unsigned lang, unsigned textlen, unsigned oldsize,
                     unsigned loader_ret, int st_after);
/* "dlg a_edge t=.. st=07 opt=0" */
int dlglog_line_aedge(char *out, unsigned t_ms, int st, unsigned opt);
/* "dlg chosen t=.. opt=0 (Yes)" */
int dlglog_line_chosen(char *out, unsigned t_ms, unsigned opt);
/* "dlg force_close t=.. st=07" */
int dlglog_line_force(char *out, unsigned t_ms, int st);
/* "dlg result t=.. res=YES fail=0 opt=0 st_after=06 core=restored" ; res: DLG_RES_*; watchdog adds "watchdog" */
int dlglog_line_result(char *out, unsigned t_ms, int result, int fail, unsigned opt, int st_after);
/* "dlg skip t=.. why=NN" : a precondition failed, nothing was done */
int dlglog_line_skip(char *out, unsigned t_ms, unsigned why);

#endif
