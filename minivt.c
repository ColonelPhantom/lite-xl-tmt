/** minivt - ANSI escape "parser" inspired by tmt
 *
 * MIT No Attribution
 * 
 * Copyright 2022 takase1121
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <string.h>
#include <stdlib.h>

#include "minivt.h"

#define PARSE_ARG_MAX 16
#define ANSI_ESC "\x1b"
#define ANSI_SOS "X"
#define ANSI_ST  "\\"
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define HANDLER(fn) static void fn(vt_parser_t *vt)

enum {
    S_NUL,
    S_ESC,
    S_ARG,
    S_STR,
    S_SES
};

struct vt_parser_s {
    callback_t cb;
    void *p;
    int state, narg;
    size_t args[PARSE_ARG_MAX], arg;
};


HANDLER(consumearg) {
    if (vt->narg < PARSE_ARG_MAX)
        vt->args[vt->narg++] = vt->arg;
    vt->arg = 0;
}

HANDLER(resetarg) {
    memset(vt->args, 0, sizeof(vt->args));
    vt->arg = vt->narg = 0;
}

vt_parser_t *
vtnew(callback_t cb, void *p) {
    vt_parser_t *vt = malloc(sizeof(vt_parser_t));
    if (vt == NULL) return NULL;
    memset(vt, 0, sizeof(vt_parser_t));
    vt->cb = cb;
    vt->p = p;
    return vt;
}

void
vtparse(vt_parser_t *vt, const char *data, size_t len) {
#define ON(S, C, A) if (vt->state == (S) && strchr(C, c)) { A; continue; }
#define ON_ANY(S, A) if (vt->state == (S)) { A; continue; }
#define DO(S, C, A) ON(S, C, consumearg(vt); A; resetarg(vt));
#define CB(T) vt->cb(T, &ans, vt->p)
#define BUF(T) int l = i - si - MIN(i, 1); if (l > 0) { ans.buffer.len = l; ans.buffer.b = data + si; CB(T); si = i + 1; }

    vt_answer_t ans;
    size_t si = 0;
    for (size_t i = 0; i < len; i++) {
        char c = data[i];
        char cs[] = { c, '\0' };
        ON(S_NUL, ANSI_ESC, vt->state = S_ESC)
        ON(S_ESC, ANSI_SOS, vt->state = S_STR; BUF(VT_MSG_PASS))

        /* ANYTHING BETWEEN THESE ARE NONSTANDARD EVENTS */
        ON(S_STR, "P",          vt->state = S_ARG; resetarg(vt))
        ON(S_ARG, ";",          consumearg(vt))
        ON(S_ARG, "0123456789", vt->arg = vt->arg * 10 + atoi(cs))
        DO(S_ARG, "R",          vt->state = S_STR; ans.point.r = vt->args[0]; ans.point.c = vt->args[1]; CB(VT_MSG_RESIZE))
        /* END OF NONSTANDARD EVENTS */

        ON(S_STR, ANSI_ESC, vt->state = S_SES)
        ON(S_SES, ANSI_ST,  vt->state = S_NUL; BUF(VT_MSG_CONTENT); CB(VT_MSG_END))
        ON_ANY(S_SES,       vt->state = S_STR)
    }

    if (len - si > 0) {
        ans.buffer.len = len - si;
        ans.buffer.b = data + si;
        CB(vt->state >= S_STR ? VT_MSG_CONTENT : VT_MSG_PASS);
    }
}

void
vtfree(vt_parser_t *vt) {
    if (vt != NULL) free(vt);
}
