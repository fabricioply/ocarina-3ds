#include "dlglog.h"
#include "dlg.h"
#include "catalog.h"

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

static char *put_dec(char *p, unsigned v)   /* no division; exact up to 999999999 */
{
    static const unsigned pw[9] = {100000000u, 10000000u, 1000000u, 100000u, 10000u, 1000u, 100u, 10u, 1u};
    int i, started = 0;
    for (i = 0; i < 9; i++) {
        unsigned d = 0;
        while (v >= pw[i]) { v -= pw[i]; d++; }
        if (d || started || i == 8) { started = 1; *p++ = (char)('0' + d); }
    }
    return p;
}

static char *put_st(char *p, int st)
{
    if (st < 0 || st > 0xFF) return put_str(p, "--");
    return put_hexw(p, (unsigned)st, 2);
}

static char *put_opt(char *p, unsigned opt)
{
    if (opt > 1u) return put_str(p, "-");
    return put_dec(p, opt);
}

static int fin(char *out, char *p)
{
    *p++ = '\r'; *p++ = '\n'; *p = 0;
    return (int)(p - out);
}

int dlglog_line_arm(char *out, unsigned t_ms, int st)
{
    char *p = put_str(out, "dlg arm t="); p = put_dec(p, t_ms);
    p = put_str(p, " st="); p = put_st(p, st);
    return fin(out, p);
}

int dlglog_line_trans(char *out, unsigned t_ms, const char *phase, int st, unsigned opt)
{
    char *p = put_str(out, "dlg t="); p = put_dec(p, t_ms);
    p = put_str(p, " phase="); p = put_str(p, phase);
    p = put_str(p, " st="); p = put_st(p, st);
    p = put_str(p, " opt="); p = put_opt(p, opt);
    return fin(out, p);
}

int dlglog_line_closeprev(char *out, unsigned t_ms, int st)
{
    char *p = put_str(out, "dlg closeprev t="); p = put_dec(p, t_ms);
    p = put_str(p, " st="); p = put_st(p, st);
    p = put_str(p, " core_saved=1");
    return fin(out, p);
}

int dlglog_line_open(char *out, unsigned t_ms, unsigned patch, unsigned lang, unsigned textlen, unsigned oldsize,
                     unsigned loader_ret, int st_after)
{
    char *p = put_str(out, "dlg open t="); p = put_dec(p, t_ms);
    p = put_str(p, " patch="); p = put_dec(p, patch);
    p = put_str(p, " lang="); p = put_dec(p, lang);
    p = put_str(p, " textlen="); p = put_dec(p, textlen);
    p = put_str(p, " oldsize="); p = put_dec(p, oldsize);
    p = put_str(p, " loader_ret="); p = put_dec(p, loader_ret);
    p = put_str(p, " st_after="); p = put_st(p, st_after);
    return fin(out, p);
}

int dlglog_line_aedge(char *out, unsigned t_ms, int st, unsigned opt)
{
    char *p = put_str(out, "dlg a_edge t="); p = put_dec(p, t_ms);
    p = put_str(p, " st="); p = put_st(p, st);
    p = put_str(p, " opt="); p = put_opt(p, opt);
    return fin(out, p);
}

int dlglog_line_chosen(char *out, unsigned t_ms, unsigned opt)
{
    char *p = put_str(out, "dlg chosen t="); p = put_dec(p, t_ms);
    p = put_str(p, " opt="); p = put_opt(p, opt);
    p = put_str(p, opt == 0u ? " (Yes)" : " (No)");
    return fin(out, p);
}

int dlglog_line_force(char *out, unsigned t_ms, int st)
{
    char *p = put_str(out, "dlg force_close t="); p = put_dec(p, t_ms);
    p = put_str(p, " st="); p = put_st(p, st);
    return fin(out, p);
}

int dlglog_line_result(char *out, unsigned t_ms, int result, int fail, unsigned opt, int st_after)
{
    const char *r = result == DLG_RES_YES ? "YES" : result == DLG_RES_NO ? "NO" :
                    result == DLG_RES_FAIL ? "FAIL" : result == DLG_RES_WATCHDOG ? "WATCHDOG" : "NONE";
    char *p = put_str(out, "dlg result t="); p = put_dec(p, t_ms);
    p = put_str(p, " res="); p = put_str(p, r);
    if (result == DLG_RES_WATCHDOG) p = put_str(p, " watchdog");
    p = put_str(p, " fail="); p = put_dec(p, (unsigned)fail);
    p = put_str(p, " opt="); p = put_opt(p, opt);
    p = put_str(p, " st_after="); p = put_st(p, st_after);
    p = put_str(p, " core=restored");
    return fin(out, p);
}

int dlglog_line_skip(char *out, unsigned t_ms, unsigned why)
{
    char *p = put_str(out, "dlg skip t="); p = put_dec(p, t_ms);
    p = put_str(p, " why="); p = put_dec(p, why);
    return fin(out, p);
}
