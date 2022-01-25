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
#ifndef MINIVT_H
#define MINIVT_H

#include <stddef.h>
enum {
    VT_MSG_PASS,
    VT_MSG_CONTENT,
    VT_MSG_END,
    VT_MSG_RESIZE
};

typedef struct vt_parser_s vt_parser_t;
typedef union {
    struct { size_t len; const char *b; } buffer;
    struct { size_t r, c; } point;
} vt_answer_t;

typedef void (*callback_t)(int type, vt_answer_t *, void *);

vt_parser_t *vtnew(callback_t cb, void *p);
void vtparse(vt_parser_t *vt, const char *data, size_t len);
void vtfree(vt_parser_t *vt);

#endif
