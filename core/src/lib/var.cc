/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 *  OSSP var - Variable Expansion
 *  Copyright (c) 2001-2002 Ralf S. Engelschall <rse@engelschall.com>
 *  Copyright (c) 2001-2002 The OSSP Project (http://www.ossp.org/)
 *  Copyright (c) 2001-2002 Cable & Wireless Deutschland (http://www.cw.com/de/)
 *
 *  This file is part of OSSP var, a variable expansion
 *  library which can be found at http://www.ossp.org/pkg/lib/var/.
 *
 *  Permission to use, copy, modify, and distribute this software for
 *  any purpose with or without fee is hereby granted, provided that
 *  the above copyright notice and this permission notice appear in all
 *  copies.
 *
 *  For disclaimer see below.
 */
/*
 * Adapted by Kern Sibbald to BACULA June 2003
 */

#include "bareos.h"
#if defined(HAVE_PCREPOSIX)
#  include <pcreposix.h>
#elif defined(HAVE_WIN32)
#  include "bregex.h"
#else
#  include <regex.h>
#endif
#include "var.h"

/* support for OSSP ex based exception throwing */
#ifdef WITH_EX
#include "ex.h"
#define VAR_RC(rv) \
    (  (rv) != VAR_OK && (ex_catching && !ex_shielding) \
     ? (ex_throw(var_id, NULL, (rv)), (rv)) : (rv) )
#else
#define VAR_RC(rv) (rv)
#endif /* WITH_EX */

#ifndef EOS
#define EOS '\0'
#endif

/*
**
**  ==== INTERNAL DATA STRUCTURES ====
**
*/

typedef char char_class_t[256]; /* 256 == 2 ^ sizeof(unsigned char)*8 */

/* the external context structure */
struct var_st {
    var_syntax_t         syntax;
    char_class_t         syntax_nameclass;
    var_cb_value_t       cb_value_fct;
    void                *cb_value_ctx;
    var_cb_operation_t   cb_operation_fct;
    void                *cb_operation_ctx;
};

/* the internal expansion context structure */
struct var_parse_st {
    struct var_parse_st *lower;
    int                  force_expand;
    int                  rel_lookup_flag;
    int                  rel_lookup_cnt;
    int                  index_this;
};
typedef struct var_parse_st var_parse_t;

/* the default syntax configuration */
static const var_syntax_t var_syntax_default = {
    '\\',         /* escape */
    '$',          /* delim_init */
    '{',          /* delim_open */
    '}',          /* delim_close */
    '[',          /* index_open */
    ']',          /* index_close */
    '#',          /* index_mark */
    "a-zA-Z0-9_"  /* name_chars */
};

/*
**
**  ==== FORMATTING FUNCTIONS ====
**
*/

/* minimal output-independent vprintf(3) variant which supports %{c,s,d,%} only */
static int
var_mvxprintf(
    int (*output)(void *ctx, const char *buffer, int bufsize), void *ctx,
    const char *format, va_list ap)
{
    /* sufficient integer buffer: <available-bits> x log_10(2) + safety */
    char ibuf[((sizeof(int)*8)/3)+10];
    const char *cp;
    char c;
    int d;
    int n;
    int bytes;

    if (format == NULL)
        return -1;
    bytes = 0;
    while (*format != '\0') {
        if (*format == '%') {
            c = *(format+1);
            if (c == '%') {
                /* expand "%%" */
                cp = &c;
                n = sizeof(char);
            }
            else if (c == 'c') {
                /* expand "%c" */
                c = (char)va_arg(ap, int);
                cp = &c;
                n = sizeof(char);
            }
            else if (c == 's') {
                /* expand "%s" */
                if ((cp = (char *)va_arg(ap, char *)) == NULL)
                    cp = "(null)";
                n = strlen(cp);
            }
            else if (c == 'd') {
                /* expand "%d" */
                d = (int)va_arg(ap, int);
                bsnprintf(ibuf, sizeof(ibuf), "%d", d); /* explicitly secure */
                cp = ibuf;
                n = strlen(cp);
            }
            else {
                /* any other "%X" */
                cp = (char *)format;
                n  = 2;
            }
            format += 2;
        }
        else {
            /* plain text */
            cp = (char *)format;
            if ((format = strchr(cp, '%')) == NULL)
                format = strchr(cp, '\0');
            n = format - cp;
        }
        /* perform output operation */
        if (output != NULL)
            if ((n = output(ctx, cp, n)) == -1)
                break;
        bytes += n;
    }
    return bytes;
}

/* output callback function context for var_mvsnprintf() */
typedef struct {
    char *bufptr;
    int buflen;
} var_mvsnprintf_cb_t;

/* output callback function for var_mvsnprintf() */
static int
var_mvsnprintf_cb(
    void *_ctx,
    const char *buffer, int bufsize)
{
    var_mvsnprintf_cb_t *ctx = (var_mvsnprintf_cb_t *)_ctx;

    if (bufsize > ctx->buflen)
        return -1;
    memcpy(ctx->bufptr, buffer, bufsize);
    ctx->bufptr += bufsize;
    ctx->buflen -= bufsize;
    return bufsize;
}

/* minimal vsnprintf(3) variant which supports %{c,s,d} only */
static int
var_mvsnprintf(
    char *buffer, int bufsize,
    const char *format, va_list ap)
{
    int n;
    var_mvsnprintf_cb_t ctx;

    if (format == NULL)
        return -1;
    if (buffer != NULL && bufsize == 0)
        return -1;
    if (buffer == NULL)
        /* just determine output length */
        n = var_mvxprintf(NULL, NULL, format, ap);
    else {
        /* perform real output */
        ctx.bufptr = buffer;
        ctx.buflen = bufsize;
        n = var_mvxprintf(var_mvsnprintf_cb, &ctx, format, ap);
        if (n != -1 && ctx.buflen == 0)
            n = -1;
        if (n != -1)
            *(ctx.bufptr) = '\0';
    }
    return n;
}

/*
**
**  ==== PARSE CONTEXT FUNCTIONS ====
**
*/

static var_parse_t *
var_parse_push(
    var_parse_t *lower, var_parse_t *upper)
{
    if (upper == NULL)
        return NULL;
    memcpy(upper, lower, sizeof(var_parse_t));
    upper->lower = lower;
    return upper;
}

static var_parse_t *
var_parse_pop(
    var_parse_t *upper)
{
    if (upper == NULL)
        return NULL;
    return upper->lower;
}

/*
**
**  ==== TOKEN BUFFER FUNCTIONS ====
**
*/

#define TOKENBUF_INITIAL_BUFSIZE 64

typedef struct {
    const char *begin;
    const char *end;
    int buffer_size;
} tokenbuf_t;

static void
tokenbuf_init(
    tokenbuf_t *buf)
{
    buf->begin = NULL;
    buf->end = NULL;
    buf->buffer_size = 0;
    return;
}

static int
tokenbuf_isundef(
    tokenbuf_t *buf)
{
    if (buf->begin == NULL && buf->end == NULL)
        return 1;
    return 0;
}

static int
tokenbuf_isempty(
    tokenbuf_t *buf)
{
    if (buf->begin == buf->end)
        return 1;
    return 0;
}

static void
tokenbuf_set(
    tokenbuf_t *buf, const char *begin, const char *end, int buffer_size)
{
    buf->begin = begin;
    buf->end = end;
    buf->buffer_size = buffer_size;
    return;
}

static void
tokenbuf_move(
    tokenbuf_t *src, tokenbuf_t *dst)
{
    dst->begin = src->begin;
    dst->end = src->end;
    dst->buffer_size = src->buffer_size;
    tokenbuf_init(src);
    return;
}

static int
tokenbuf_assign(
    tokenbuf_t *buf, const char *data, int len)
{
    char *p;

    if ((p = (char *)malloc(len + 1)) == NULL)
        return 0;
    memcpy(p, data, len);
    buf->begin = p;
    buf->end = p + len;
    buf->buffer_size = len + 1;
    *((char *)(buf->end)) = EOS;
    return 1;
}

static int
tokenbuf_append(
    tokenbuf_t *output, const char *data, int len)
{
    char *new_buffer;
    int new_size;
    char *tmp;

    /* Is the tokenbuffer initialized at all? If not, allocate a
       standard-sized buffer to begin with. */
    if (output->begin == NULL) {
        if ((output->begin = output->end = (const char *)malloc(TOKENBUF_INITIAL_BUFSIZE)) == NULL)
            return 0;
        output->buffer_size = TOKENBUF_INITIAL_BUFSIZE;
    }

    /* does the token contain text, but no buffer has been allocated yet? */
    if (output->buffer_size == 0) {
        /* check whether data borders to output. If, we can append
           simly by increasing the end pointer. */
        if (output->end == data) {
            output->end += len;
            return 1;
        }
        /* ok, so copy the contents of output into an allocated buffer
           so that we can append that way. */
        if ((tmp = (char *)malloc(output->end - output->begin + len + 1)) == NULL)
            return 0;
        memcpy(tmp, output->begin, output->end - output->begin);
        output->buffer_size = output->end - output->begin;
        output->begin = tmp;
        output->end = tmp + output->buffer_size;
        output->buffer_size += len + 1;
    }

    /* does the token fit into the current buffer? If not, realloc a
       larger buffer that fits. */
    if ((output->buffer_size - (output->end - output->begin)) <= len) {
        new_size = output->buffer_size;
        do {
            new_size *= 2;
        } while ((new_size - (output->end - output->begin)) <= len);
        if ((new_buffer = (char *)realloc((char *)output->begin, new_size)) == NULL)
            return 0;
        output->end = new_buffer + (output->end - output->begin);
        output->begin = new_buffer;
        output->buffer_size = new_size;
    }

    /* append the data at the end of the current buffer. */
    if (len > 0)
        memcpy((char *)output->end, data, len);
    output->end += len;
    *((char *)output->end) = EOS;
    return 1;
}

static int
tokenbuf_merge(
    tokenbuf_t *output, tokenbuf_t *input)
{
    return tokenbuf_append(output, input->begin, input->end - input->begin);
}

static void
tokenbuf_free(
    tokenbuf_t *buf)
{
    if (buf->begin != NULL && buf->buffer_size > 0)
        free((char *)buf->begin);
    buf->begin = buf->end = NULL;
    buf->buffer_size = 0;
    return;
}

/*
**
**  ==== CHARACTER CLASS EXPANSION ====
**
*/

static void
expand_range(char a, char b, char_class_t chrclass)
{
    do {
        chrclass[(int)a] = 1;
    } while (++a <= b);
    return;
}

static var_rc_t
expand_character_class(const char *desc, char_class_t chrclass)
{
    int i;

    /* clear the class array. */
    for (i = 0; i < 256; ++i)
        chrclass[i] = 0;

    /* walk through class description and set appropriate entries in array */
    while (*desc != EOS) {
        if (desc[1] == '-' && desc[2] != EOS) {
            if (desc[0] > desc[2])
                return VAR_ERR_INCORRECT_CLASS_SPEC;
            expand_range(desc[0], desc[2], chrclass);
            desc += 3;
        } else {
            chrclass[(int) *desc] = 1;
            desc++;
        }
    }
    return VAR_OK;
}

/*
**
**  ==== ESCAPE SEQUENCE EXPANSION FUNCTIONS ====
**
*/

static int
expand_isoct(
    int c)
{
    if (c >= '0' && c <= '7')
        return 1;
    else
        return 0;
}

static var_rc_t
expand_octal(
    const char **src, char **dst, const char *end)
{
    int c;

    if (end - *src < 3)
        return VAR_ERR_INCOMPLETE_OCTAL;
    if (   !expand_isoct(**src)
        || !expand_isoct((*src)[1])
        || !expand_isoct((*src)[2]))
        return VAR_ERR_INVALID_OCTAL;

    c = **src - '0';
    if (c > 3)
        return VAR_ERR_OCTAL_TOO_LARGE;
    c *= 8;
    (*src)++;

    c += **src - '0';
    c *= 8;
    (*src)++;

    c += **src - '0';

    **dst = (char) c;
    (*dst)++;
    return VAR_OK;
}

static int
expand_ishex(
    int c)
{
    if ((c >= '0' && c <= '9') ||
        (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
        return 1;
    else
        return 0;
}

static var_rc_t
expand_simple_hex(
    const char **src, char **dst, const char *end)
{
    int c = 0;

    if (end - *src < 2)
        return VAR_ERR_INCOMPLETE_HEX;
    if (   !expand_ishex(**src)
        || !expand_ishex((*src)[1]))
        return VAR_ERR_INVALID_HEX;

    if (**src >= '0' && **src <= '9')
        c = **src - '0';
    else if (**src >= 'a' && **src <= 'f')
        c = **src - 'a' + 10;
    else if (**src >= 'A' && **src <= 'F')
        c = **src - 'A' + 10;

    c = c << 4;
    (*src)++;

    if (**src >= '0' && **src <= '9')
        c += **src - '0';
    else if (**src >= 'a' && **src <= 'f')
        c += **src - 'a' + 10;
    else if (**src >= 'A' && **src <= 'F')
        c += **src - 'A' + 10;

    **dst = (char)c;
    (*dst)++;
    return VAR_OK;
}

static var_rc_t
expand_grouped_hex(
    const char **src, char **dst, const char *end)
{
    var_rc_t rc;

    while (*src < end && **src != '}') {
        if ((rc = expand_simple_hex(src, dst, end)) != VAR_OK)
            return rc;
        (*src)++;
    }
    if (*src == end)
        return VAR_ERR_INCOMPLETE_GROUPED_HEX;

    return VAR_OK;
}

static var_rc_t
expand_hex(
    const char **src, char **dst, const char *end)
{
    if (*src == end)
        return VAR_ERR_INCOMPLETE_HEX;
    if (**src == '{') {
        (*src)++;
        return expand_grouped_hex(src, dst, end);
    } else
        return expand_simple_hex(src, dst, end);
}

/*
**
**  ==== RECURSIVE-DESCEND VARIABLE EXPANSION PARSER ====
**
*/

/* forward declarations */
static int parse_variable(var_t *var, var_parse_t *ctx, const char *begin, const char *end, tokenbuf_t *result);
static int parse_numexp (var_t *var, var_parse_t *ctx, const char *begin, const char *end, int *result, int *failed);
static int parse_name   (var_t *var, var_parse_t *ctx, const char *begin, const char *end);

/* parse pattern text */
static int
parse_pattern(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end)
{
    const char *p;

    /* parse until '/' */
    for (p = begin; p != end && *p != '/'; p++) {
        if (*p == var->syntax.escape) {
            if (p + 1 == end)
                return VAR_ERR_INCOMPLETE_QUOTED_PAIR;
            p++;
        }
    }
    return (p - begin);
}

/* parse substitution text */
static int
parse_substext(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end)
{
    const char *p;

    /* parse until delim_init or '/' */
    for (p = begin; p != end && *p != var->syntax.delim_init && *p != '/'; p++) {
        if (*p == var->syntax.escape) {
            if (p + 1 == end)
                return VAR_ERR_INCOMPLETE_QUOTED_PAIR;
            p++;
        }
    }
    return (p - begin);
}

/* parse expression text */
static int
parse_exptext(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end)
{
    const char *p;

    /* parse until delim_init or delim_close or ':' */
    for (p = begin;     p != end
                    && *p != var->syntax.delim_init
                    && *p != var->syntax.delim_close
                    && *p != ':'; p++) {
        if (*p == var->syntax.escape) {
            if (p + 1 == end)
                return VAR_ERR_INCOMPLETE_QUOTED_PAIR;
            p++;
        }
    }
    return (p - begin);
}

/* parse opertion argument text */
static int
parse_opargtext(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end)
{
    const char *p;

    /* parse until delim_init or ')' */
    for (p = begin; p != end && *p != var->syntax.delim_init && *p != ')'; p++) {
        if (*p == var->syntax.escape) {
            if (p + 1 == end)
                return VAR_ERR_INCOMPLETE_QUOTED_PAIR;
            p++;
        }
    }
    return (p - begin);
}

static int
parse_opargtext_or_variable(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    tokenbuf_t *result)
{
    const char *p;
    tokenbuf_t tmp;
    int rc;

    tokenbuf_init(result);
    tokenbuf_init(&tmp);
    p = begin;
    if (p == end)
        return 0;
    do {
        rc = parse_opargtext(var, ctx, p, end);
        if (rc < 0)
            goto error_return;
        if (rc > 0) {
            if (!tokenbuf_append(result, p, rc)) {
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
            p += rc;
        }
        rc = parse_variable(var, ctx, p, end, &tmp);
        if (rc < 0)
            goto error_return;
        if (rc > 0) {
            p += rc;
            if (!tokenbuf_merge(result, &tmp)) {
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
        }
        tokenbuf_free(&tmp);          /* KES 11/9/2003 */
    } while (rc > 0);
    tokenbuf_free(&tmp);
    return (p - begin);

error_return:
    tokenbuf_free(&tmp);
    tokenbuf_free(result);
    return rc;
}

/* parse expression or variable */
static int
parse_exptext_or_variable(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    tokenbuf_t *result)
{
    const char *p = begin;
    tokenbuf_t tmp;
    int rc;

    tokenbuf_init(result);
    tokenbuf_init(&tmp);
    if (begin == end)
        return 0;
    do {
        /* try to parse expression text */
        rc = parse_exptext(var, ctx, p, end);
        if (rc < 0)
            goto error_return;
        if (rc > 0) {
            if (!tokenbuf_append(result, p, rc)) {
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
            p += rc;
        }

        /* try to parse variable construct */
        rc = parse_variable(var, ctx, p, end, &tmp);
        if (rc < 0)
            goto error_return;
        if (rc > 0) {
            p += rc;
            if (!tokenbuf_merge(result, &tmp)) {
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
        }
        tokenbuf_free(&tmp);          /* KES 11/9/2003 */
    } while (rc > 0);

    tokenbuf_free(&tmp);
    return (p - begin);

error_return:
    tokenbuf_free(&tmp);
    tokenbuf_free(result);
    return rc;
}

/* parse substitution text or variable */
static int
parse_substext_or_variable(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    tokenbuf_t *result)
{
    const char *p = begin;
    tokenbuf_t tmp;
    int rc;

    tokenbuf_init(result);
    tokenbuf_init(&tmp);
    if (begin == end)
        return 0;
    do {
        /* try to parse substitution text */
        rc = parse_substext(var, ctx, p, end);
        if (rc < 0)
            goto error_return;
        if (rc > 0) {
            if (!tokenbuf_append(result, p, rc)) {
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
            p += rc;
        }

        /* try to parse substitution text */
        rc = parse_variable(var, ctx, p, end, &tmp);
        if (rc < 0)
            goto error_return;
        if (rc > 0) {
            p += rc;
            if (!tokenbuf_merge(result, &tmp)) {
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
        }
        tokenbuf_free(&tmp);          /* KES 11/9/2003 */
    } while (rc > 0);

    tokenbuf_free(&tmp);
    return (p - begin);

error_return:
    tokenbuf_free(&tmp);
    tokenbuf_free(result);
    return rc;
}

/* parse class description */
static int
parse_class_description(
    var_t *var, var_parse_t *ctx,
    tokenbuf_t *src, tokenbuf_t *dst)
{
    unsigned char c, d;
    const char *p;

    p = src->begin;
    while (p != src->end) {
        if ((src->end - p) >= 3 && p[1] == '-') {
            if (*p > p[2])
                return VAR_ERR_INCORRECT_TRANSPOSE_CLASS_SPEC;
            for (c = *p, d = p[2]; c <= d; ++c) {
                if (!tokenbuf_append(dst, (char *)&c, 1))
                    return VAR_ERR_OUT_OF_MEMORY;
            }
            p += 3;
        } else {
            if (!tokenbuf_append(dst, p, 1))
                return VAR_ERR_OUT_OF_MEMORY;
            p++;
        }
    }
    return VAR_OK;
}

/* parse regex replace part */
static int
parse_regex_replace(
    var_t *var, var_parse_t *ctx,
    const char *data,
    tokenbuf_t *orig,
    regmatch_t *pmatch,
    tokenbuf_t *expanded)
{
    const char *p;
    int i;

    p = orig->begin;
    tokenbuf_init(expanded);

    while (p != orig->end) {
        if (*p == '\\') {
            if (orig->end - p <= 1) {
                tokenbuf_free(expanded);
                return VAR_ERR_INCOMPLETE_QUOTED_PAIR;
            }
            p++;
            if (*p == '\\') {
                if (!tokenbuf_append(expanded, p, 1)) {
                    tokenbuf_free(expanded);
                    return VAR_ERR_OUT_OF_MEMORY;
                }
                p++;
                continue;
            }
            if (!isdigit((int)*p)) {
                tokenbuf_free(expanded);
                return VAR_ERR_UNKNOWN_QUOTED_PAIR_IN_REPLACE;
            }
            i = (*p - '0');
            p++;
            if (pmatch[i].rm_so == -1 || pmatch[i].rm_eo == -1) {
                tokenbuf_free(expanded);
                return VAR_ERR_SUBMATCH_OUT_OF_RANGE;
            }
            if (!tokenbuf_append(expanded, data + pmatch[i].rm_so,
                                 pmatch[i].rm_eo - pmatch[i].rm_so)) {
                tokenbuf_free(expanded);
                return VAR_ERR_OUT_OF_MEMORY;
            }
        } else {
            if (!tokenbuf_append(expanded, p, 1)) {
                tokenbuf_free(expanded);
                return VAR_ERR_OUT_OF_MEMORY;
            }
            p++;
        }
    }

    return VAR_OK;
}

/* operation: transpose */
static int
op_transpose(
    var_t *var, var_parse_t *ctx,
    tokenbuf_t *data,
    tokenbuf_t *search,
    tokenbuf_t *replace)
{
    tokenbuf_t srcclass, dstclass;
    const char *p;
    int rc;
    int i;

    tokenbuf_init(&srcclass);
    tokenbuf_init(&dstclass);
    if ((rc = parse_class_description(var, ctx, search, &srcclass)) != VAR_OK)
        goto error_return;
    if ((rc = parse_class_description(var, ctx, replace, &dstclass)) != VAR_OK)
        goto error_return;
    if (srcclass.begin == srcclass.end) {
        rc = VAR_ERR_EMPTY_TRANSPOSE_CLASS;
        goto error_return;
    }
    if ((srcclass.end - srcclass.begin) != (dstclass.end - dstclass.begin)) {
        rc = VAR_ERR_TRANSPOSE_CLASSES_MISMATCH;
        goto error_return;
    }
    if (data->buffer_size == 0) {
        tokenbuf_t tmp;
        if (!tokenbuf_assign(&tmp, data->begin, data->end - data->begin)) {
            rc = VAR_ERR_OUT_OF_MEMORY;
            goto error_return;
        }
        tokenbuf_move(&tmp, data);
    }
    for (p = data->begin; p != data->end; ++p) {
        for (i = 0; i <= (srcclass.end - srcclass.begin); ++i) {
            if (*p == srcclass.begin[i]) {
                *((char *)p) = dstclass.begin[i];
                break;
            }
        }
    }
    tokenbuf_free(&srcclass);
    tokenbuf_free(&dstclass);
    return VAR_OK;

error_return:
    tokenbuf_free(search);
    tokenbuf_free(replace);
    tokenbuf_free(&srcclass);
    tokenbuf_free(&dstclass);
    return rc;
}

/* operation: search & replace */
static int
op_search_and_replace(
    var_t *var, var_parse_t *ctx,
    tokenbuf_t *data,
    tokenbuf_t *search,
    tokenbuf_t *replace,
    tokenbuf_t *flags)
{
    tokenbuf_t tmp;
    const char *p;
    int case_insensitive = 0;
    int multiline = 0;
    int global = 0;
    int no_regex = 0;
    bool rc;

    if (search->begin == search->end)
        return VAR_ERR_EMPTY_SEARCH_STRING;

    for (p = flags->begin; p != flags->end; p++) {
        switch (tolower(*p)) {
            case 'm':
                multiline = 1;
                break;
            case 'i':
                case_insensitive = 1;
                break;
            case 'g':
                global = 1;
                break;
            case 't':
                no_regex = 1;
                break;
            default:
                return VAR_ERR_UNKNOWN_REPLACE_FLAG;
        }
    }

    if (no_regex) {
        /* plain text pattern based operation */
        tokenbuf_init(&tmp);
        for (p = data->begin; p != data->end;) {
            if (case_insensitive)
                rc = bstrncasecmp(p, search->begin, search->end - search->begin);
            else
                rc = bstrncmp(p, search->begin, search->end - search->begin);
            if (!rc) {
                /* not matched, copy character */
                if (!tokenbuf_append(&tmp, p, 1)) {
                    tokenbuf_free(&tmp);
                    return VAR_ERR_OUT_OF_MEMORY;
                }
                p++;
            } else {
                /* matched, copy replacement string */
                tokenbuf_merge(&tmp, replace);
                p += (search->end - search->begin);
                if (!global) {
                    /* append remaining text */
                    if (!tokenbuf_append(&tmp, p, data->end - p)) {
                        tokenbuf_free(&tmp);
                        return VAR_ERR_OUT_OF_MEMORY;
                    }
                    break;
                }
            }
        }
        tokenbuf_free(data);
        tokenbuf_move(&tmp, data);
    } else {
        /* regular expression pattern based operation */
        tokenbuf_t mydata;
        tokenbuf_t myreplace;
        regex_t preg;
        regmatch_t pmatch[10];
        int regexec_flag;

        /* copy pattern and data to own buffer to make sure they are EOS-terminated */
        if (!tokenbuf_assign(&tmp, search->begin, search->end - search->begin))
            return VAR_ERR_OUT_OF_MEMORY;
        if (!tokenbuf_assign(&mydata, data->begin, data->end - data->begin)) {
            tokenbuf_free(&tmp);
            return VAR_ERR_OUT_OF_MEMORY;
        }

        /* compile the pattern. */
        rc = regcomp(&preg, tmp.begin,
                     (  REG_EXTENDED
                      | (multiline ? REG_NEWLINE : 0)
                      | (case_insensitive ? REG_ICASE : 0)));
        tokenbuf_free(&tmp);
        if (rc != 0) {
            tokenbuf_free(&mydata);
            return VAR_ERR_INVALID_REGEX_IN_REPLACE;
        }

        /* match the pattern and create the result string in the tmp buffer */
        tokenbuf_append(&tmp, "", 0);
        for (p = mydata.begin; p < mydata.end; ) {
            if (p == mydata.begin || p[-1] == '\n')
                regexec_flag = 0;
            else
                regexec_flag = REG_NOTBOL;
            rc = regexec(&preg, p, sizeof(pmatch) / sizeof(regmatch_t), pmatch, regexec_flag);
            if (rc != 0) {
                /* no (more) matching */
                tokenbuf_append(&tmp, p, mydata.end - p);
                break;
            } else if (multiline &&
                       (p + pmatch[0].rm_so) == mydata.end &&
                       (pmatch[0].rm_eo - pmatch[0].rm_so) == 0) {
                /* special case: found empty pattern (usually /^/ or /$/ only)
                   in multi-line at end of data (after the last newline) */
                tokenbuf_append(&tmp, p, mydata.end - p);
                break;
            }
            else {
                /* append prolog string */
                if (!tokenbuf_append(&tmp, p, pmatch[0].rm_so)) {
                    regfree(&preg);
                    tokenbuf_free(&tmp);
                    tokenbuf_free(&mydata);
                    return VAR_ERR_OUT_OF_MEMORY;
                }
                /* create replace string */
                rc = parse_regex_replace(var, ctx, p, replace, pmatch, &myreplace);
                if (rc != VAR_OK) {
                    regfree(&preg);
                    tokenbuf_free(&tmp);
                    tokenbuf_free(&mydata);
                    return rc;
                }
                /* append replace string */
                if (!tokenbuf_merge(&tmp, &myreplace)) {
                    regfree(&preg);
                    tokenbuf_free(&tmp);
                    tokenbuf_free(&mydata);
                    tokenbuf_free(&myreplace);
                    return VAR_ERR_OUT_OF_MEMORY;
                }
                tokenbuf_free(&myreplace);
                /* skip now processed data */
                p += pmatch[0].rm_eo;
                /* if pattern matched an empty part (think about
                   anchor-only regular expressions like /^/ or /$/) we
                   skip the next character to make sure we do not enter
                   an infinitive loop in matching */
                if ((pmatch[0].rm_eo - pmatch[0].rm_so) == 0) {
                    if (p >= mydata.end)
                        break;
                    if (!tokenbuf_append(&tmp, p, 1)) {
                        regfree(&preg);
                        tokenbuf_free(&tmp);
                        tokenbuf_free(&mydata);
                        return VAR_ERR_OUT_OF_MEMORY;
                    }
                    p++;
                }
                /* append prolog string and stop processing if we
                   do not perform the search & replace globally */
                if (!global) {
                    if (!tokenbuf_append(&tmp, p, mydata.end - p)) {
                        regfree(&preg);
                        tokenbuf_free(&tmp);
                        tokenbuf_free(&mydata);
                        return VAR_ERR_OUT_OF_MEMORY;
                    }
                    break;
                }
            }
        }
        regfree(&preg);
        tokenbuf_free(data);
        tokenbuf_move(&tmp, data);
        tokenbuf_free(&mydata);
    }

    return VAR_OK;
}

/* operation: offset substring */
static int
op_offset(
    var_t *var, var_parse_t *ctx,
    tokenbuf_t *data,
    int num1,
    int num2,
    int isrange)
{
    tokenbuf_t res;
    const char *p;

    /* determine begin of result string */
    if ((data->end - data->begin) < num1)
        return VAR_ERR_OFFSET_OUT_OF_BOUNDS;
    p = data->begin + num1;

    /* if num2 is zero, we copy the rest from there. */
    if (num2 == 0) {
        if (!tokenbuf_assign(&res, p, data->end - p))
            return VAR_ERR_OUT_OF_MEMORY;
    } else {
        /* ok, then use num2. */
        if (isrange) {
            if ((p + num2) > data->end)
                return VAR_ERR_RANGE_OUT_OF_BOUNDS;
            if (!tokenbuf_assign(&res, p, num2))
                return VAR_ERR_OUT_OF_MEMORY;
        } else {
            if (num2 < num1)
                return VAR_ERR_OFFSET_LOGIC;
            if ((data->begin + num2) > data->end)
                return VAR_ERR_RANGE_OUT_OF_BOUNDS;
            if (!tokenbuf_assign(&res, p, num2 - num1 + 1))
                return VAR_ERR_OUT_OF_MEMORY;
        }
    }
    tokenbuf_free(data);
    tokenbuf_move(&res, data);
    return VAR_OK;
}

/* operation: padding */
static int
op_padding(
    var_t *var, var_parse_t *ctx,
    tokenbuf_t *data,
    int width,
    tokenbuf_t *fill,
    char position)
{
    tokenbuf_t result;
    int i;

    if (fill->begin == fill->end)
        return VAR_ERR_EMPTY_PADDING_FILL_STRING;
    tokenbuf_init(&result);
    if (position == 'l') {
        /* left padding */
        i = width - (data->end - data->begin);
        if (i > 0) {
            i = i / (fill->end - fill->begin);
            while (i > 0) {
                if (!tokenbuf_append(data, fill->begin, fill->end - fill->begin))
                    return VAR_ERR_OUT_OF_MEMORY;
                i--;
            }
            i = (width - (data->end - data->begin)) % (fill->end - fill->begin);
            if (!tokenbuf_append(data, fill->begin, i))
                return VAR_ERR_OUT_OF_MEMORY;
        }
    } else if (position == 'r') {
        /* right padding */
        i = width - (data->end - data->begin);
        if (i > 0) {
            i = i / (fill->end - fill->begin);
            while (i > 0) {
                if (!tokenbuf_merge(&result, fill)) {
                    tokenbuf_free(&result);
                    return VAR_ERR_OUT_OF_MEMORY;
                }
                i--;
            }
            i = (width - (data->end - data->begin)) % (fill->end - fill->begin);
            if (!tokenbuf_append(&result, fill->begin, i)) {
                tokenbuf_free(&result);
                return VAR_ERR_OUT_OF_MEMORY;
            }
            if (!tokenbuf_merge(&result, data)) {
                tokenbuf_free(&result);
                return VAR_ERR_OUT_OF_MEMORY;
            }
            /* move string from temporary buffer to data buffer */
            tokenbuf_free(data);
            tokenbuf_move(&result, data);
        }
    } else if (position == 'c') {
        /* centered padding */
        i = (width - (data->end - data->begin)) / 2;
        if (i > 0) {
            /* create the prefix */
            i = i / (fill->end - fill->begin);
            while (i > 0) {
                if (!tokenbuf_merge(&result, fill)) {
                    tokenbuf_free(&result);
                    return VAR_ERR_OUT_OF_MEMORY;
                }
                i--;
            }
            i = ((width - (data->end - data->begin)) / 2)
                % (fill->end - fill->begin);
            if (!tokenbuf_append(&result, fill->begin, i)) {
                tokenbuf_free(&result);
                return VAR_ERR_OUT_OF_MEMORY;
            }
            /* append the actual data string */
            if (!tokenbuf_merge(&result, data)) {
                tokenbuf_free(&result);
                return VAR_ERR_OUT_OF_MEMORY;
            }
            /* append the suffix */
            i = width - (result.end - result.begin);
            i = i / (fill->end - fill->begin);
            while (i > 0) {
                if (!tokenbuf_merge(&result, fill)) {
                    tokenbuf_free(&result);
                    return VAR_ERR_OUT_OF_MEMORY;
                }
                i--;
            }
            i = width - (result.end - result.begin);
            if (!tokenbuf_append(&result, fill->begin, i)) {
                tokenbuf_free(&result);
                return VAR_ERR_OUT_OF_MEMORY;
            }
            /* move string from temporary buffer to data buffer */
            tokenbuf_free(data);
            tokenbuf_move(&result, data);
        }
    }
    return VAR_OK;
}

/* parse an integer number ("123") */
static int
parse_integer(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    int *result)
{
    const char *p;
    int num;

    p = begin;
    num = 0;
    while (isdigit(*p) && p != end) {
        num *= 10;
        num += (*p - '0');
        p++;
    }
    if (result != NULL)
        *result = num;
    return (p - begin);
}

/* parse an operation (":x...") */
static int
parse_operation(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    tokenbuf_t *data)
{
    const char *p;
    tokenbuf_t tmptokbuf;
    tokenbuf_t search, replace, flags;
    tokenbuf_t number1, number2;
    int num1, num2;
    int isrange;
    int rc;
    char *ptr;

    /* initialization */
    tokenbuf_init(&tmptokbuf);
    tokenbuf_init(&search);
    tokenbuf_init(&replace);
    tokenbuf_init(&flags);
    tokenbuf_init(&number1);
    tokenbuf_init(&number2);
    p = begin;
    if (p == end)
        return 0;

    /* dispatch through the first operation character */
    switch (tolower(*p)) {
        case 'l': {
            /* turn value to lowercase. */
            if (data->begin != NULL) {
                /* if the buffer does not live in an allocated buffer,
                   we have to copy it before modifying the contents. */
                if (data->buffer_size == 0) {
                    if (!tokenbuf_assign(data, data->begin, data->end - data->begin)) {
                        rc = VAR_ERR_OUT_OF_MEMORY;
                        goto error_return;
                    }
                }
                /* convert value */
                for (ptr = (char *)data->begin; ptr != data->end; ptr++)
                    *ptr = (char)tolower((int)(*ptr));
            }
            p++;
            break;
        }
        case 'u': {
            /* turn value to uppercase. */
            if (data->begin != NULL) {
                /* if the buffer does not live in an allocated buffer,
                   we have to copy it before modifying the contents. */
                if (data->buffer_size == 0) {
                    if (!tokenbuf_assign(data, data->begin, data->end - data->begin)) {
                        rc = VAR_ERR_OUT_OF_MEMORY;
                        goto error_return;
                    }
                }
                /* convert value */
                for (ptr = (char *)data->begin; ptr != data->end; ptr++)
                    *ptr = (char)toupper((int)(*ptr));
            }
            p++;
            break;
        }
        case 'o': {
            /* cut out substring of value. */
            p++;
            rc = parse_integer(var, ctx, p, end, &num1);
            if (rc == 0) {
                rc = VAR_ERR_MISSING_START_OFFSET;
                goto error_return;
            }
            else if (rc < 0)
                goto error_return;
            p += rc;
            if (*p == ',') {
                isrange = 0;
                p++;
            } else if (*p == '-') {
                isrange = 1;
                p++;
            } else {
                rc = VAR_ERR_INVALID_OFFSET_DELIMITER;
                goto error_return;
            }
            rc = parse_integer(var, ctx, p, end, &num2);
            p += rc;
            if (data->begin != NULL) {
                rc = op_offset(var, ctx, data, num1, num2, isrange);
                if (rc < 0)
                    goto error_return;
            }
            break;
        }
        case '#': {
            /* determine length of the value */
            if (data->begin != NULL) {
                char buf[((sizeof(int)*8)/3)+10]; /* sufficient size: <#bits> x log_10(2) + safety */
                sprintf(buf, "%d", (int)(data->end - data->begin));
                tokenbuf_free(data);
                if (!tokenbuf_assign(data, buf, strlen(buf))) {
                    rc = VAR_ERR_OUT_OF_MEMORY;
                    goto error_return;
                }
            }
            p++;
            break;
        }
        case '-': {
            /* substitute parameter if data is empty */
            p++;
            rc = parse_exptext_or_variable(var, ctx, p, end, &tmptokbuf);
            if (rc < 0)
                goto error_return;
            if (rc == 0) {
                rc = VAR_ERR_MISSING_PARAMETER_IN_COMMAND;
                goto error_return;
            }
            p += rc;
            if (tokenbuf_isundef(data))
                tokenbuf_move(&tmptokbuf, data);
            else if (tokenbuf_isempty(data)) {
                tokenbuf_free(data);
                tokenbuf_move(&tmptokbuf, data);
            }
            break;
        }
        case '*': {
            /* substitute empty string if data is not empty, parameter otherwise. */
            p++;
            rc = parse_exptext_or_variable(var, ctx, p, end, &tmptokbuf);
            if (rc < 0)
                goto error_return;
            if (rc == 0) {
                rc = VAR_ERR_MISSING_PARAMETER_IN_COMMAND;
                goto error_return;
            }
            p += rc;
            if (data->begin != NULL) {
                if (data->begin == data->end) {
                    tokenbuf_free(data);
                    tokenbuf_move(&tmptokbuf, data);
                } else {
                    tokenbuf_free(data);
                    data->begin = data->end = "";
                    data->buffer_size = 0;
                }
            }
            break;
        }
        case '+': {
            /* substitute parameter if data is not empty. */
            p++;
            rc = parse_exptext_or_variable(var, ctx, p, end, &tmptokbuf);
            if (rc < 0)
                goto error_return;
            if (rc == 0) {
                rc = VAR_ERR_MISSING_PARAMETER_IN_COMMAND;
                goto error_return;
            }
            p += rc;
            if (data->begin != NULL && data->begin != data->end) {
                tokenbuf_free(data);
                tokenbuf_move(&tmptokbuf, data);
            }
            break;
        }
        case 's': {
            /* search and replace. */
            p++;
            if (*p != '/')
                return VAR_ERR_MALFORMATTED_REPLACE;
            p++;
            rc = parse_pattern(var, ctx, p, end);
            if (rc < 0)
                goto error_return;
            tokenbuf_set(&search, p, p + rc, 0);
            p += rc;
            if (*p != '/') {
                rc = VAR_ERR_MALFORMATTED_REPLACE;
                goto error_return;
            }
            p++;
            rc = parse_substext_or_variable(var, ctx, p, end, &replace);
            if (rc < 0)
                goto error_return;
            p += rc;
            if (*p != '/') {
                rc = VAR_ERR_MALFORMATTED_REPLACE;
                goto error_return;
            }
            p++;
            rc = parse_exptext(var, ctx, p, end);
            if (rc < 0)
                goto error_return;
            tokenbuf_set(&flags, p, p + rc, 0);
            p += rc;
            if (data->begin != NULL) {
                rc = op_search_and_replace(var, ctx, data, &search, &replace, &flags);
                if (rc < 0)
                    goto error_return;
            }
            break;
        }
        case 'y': {
            /* transpose characters from class A to class B. */
            p++;
            if (*p != '/')
                return VAR_ERR_MALFORMATTED_TRANSPOSE;
            p++;
            rc = parse_substext_or_variable(var, ctx, p, end, &search);
            if (rc < 0)
                goto error_return;
            p += rc;
            if (*p != '/') {
                rc = VAR_ERR_MALFORMATTED_TRANSPOSE;
                goto error_return;
            }
            p++;
            rc = parse_substext_or_variable(var, ctx, p, end, &replace);
            if (rc < 0)
                goto error_return;
            p += rc;
            if (*p != '/') {
                rc = VAR_ERR_MALFORMATTED_TRANSPOSE;
                goto error_return;
            } else
                p++;
            if (data->begin) {
                rc = op_transpose(var, ctx, data, &search, &replace);
                if (rc < 0)
                    goto error_return;
            }
            break;
        }
        case 'p': {
            /* padding. */
            p++;
            if (*p != '/')
                return VAR_ERR_MALFORMATTED_PADDING;
            p++;
            rc = parse_integer(var, ctx, p, end, &num1);
            if (rc == 0) {
                rc = VAR_ERR_MISSING_PADDING_WIDTH;
                goto error_return;
            }
            p += rc;
            if (*p != '/') {
                rc = VAR_ERR_MALFORMATTED_PADDING;
                goto error_return;
            }
            p++;
            rc = parse_substext_or_variable(var, ctx, p, end, &replace);
            if (rc < 0)
                goto error_return;
            p += rc;
            if (*p != '/') {
                rc = VAR_ERR_MALFORMATTED_PADDING;
                goto error_return;
            }
            p++;
            if (*p != 'l' && *p != 'c' && *p != 'r') {
                rc = VAR_ERR_MALFORMATTED_PADDING;
                goto error_return;
            }
            p++;
            if (data->begin) {
                rc = op_padding(var, ctx, data, num1, &replace, p[-1]);
                if (rc < 0)
                    goto error_return;
            }
            break;
        }
        case '%': {
            /* operation callback function */
            const char *op_ptr;
            int op_len;
            const char *arg_ptr;
            int arg_len;
            const char *val_ptr;
            int val_len;
            const char *out_ptr;
            int out_len;
            int out_size;
            tokenbuf_t args;

            p++;
            rc = parse_name(var, ctx, p, end);
            if (rc < 0)
                goto error_return;
            op_ptr = p;
            op_len = rc;
            p += rc;
            if (*p == '(') {
                p++;
                tokenbuf_init(&args);
                rc = parse_opargtext_or_variable(var, ctx, p, end, &args);
                if (rc < 0)
                    goto error_return;
                p += rc;
                arg_ptr = args.begin;
                arg_len = args.end - args.begin;
                if (*p != ')') {
                    rc = VAR_ERR_MALFORMED_OPERATION_ARGUMENTS;
                    goto error_return;
                }
                p++;
            }
            else {
                arg_ptr = NULL;
                arg_len = 0;
            }
            val_ptr = data->begin;
            val_len = data->end - data->begin;

            if (data->begin != NULL && var->cb_operation_fct != NULL) {
                /* call operation callback function */
                rc = (*var->cb_operation_fct)(var, var->cb_operation_ctx,
                                              op_ptr, op_len,
                                              arg_ptr, arg_len,
                                              val_ptr, val_len,
                                              &out_ptr, &out_len, &out_size);
                if (rc < 0) {
                    if (arg_ptr != NULL)
                        free((void *)arg_ptr);
                    goto error_return;
                }
                tokenbuf_free(data);
                tokenbuf_set(data, out_ptr, out_ptr+out_len, out_size);
            }
            if (arg_ptr != NULL)
               free((void *)arg_ptr);
            break;
        }
        default:
            return VAR_ERR_UNKNOWN_COMMAND_CHAR;
    }

    /* return successfully */
    tokenbuf_free(&tmptokbuf);
    tokenbuf_free(&search);
    tokenbuf_free(&replace);
    tokenbuf_free(&flags);
    tokenbuf_free(&number1);
    tokenbuf_free(&number2);
    return (p - begin);

    /* return with an error */
error_return:
    tokenbuf_free(data);
    tokenbuf_free(&tmptokbuf);
    tokenbuf_free(&search);
    tokenbuf_free(&replace);
    tokenbuf_free(&flags);
    tokenbuf_free(&number1);
    tokenbuf_free(&number2);
    return rc;
}

/* parse numerical expression operand */
static int
parse_numexp_operand(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    int *result, int *failed)
{
    const char *p;
    tokenbuf_t tmp;
    int rc;
    var_parse_t myctx;

    /* initialization */
    p = begin;
    tokenbuf_init(&tmp);
    if (p == end)
        return VAR_ERR_INCOMPLETE_INDEX_SPEC;

    /* parse opening numerical expression */
    if (*p == '(') {
        /* parse inner numerical expression */
        rc = parse_numexp(var, ctx, ++p, end, result, failed);
        if (rc < 0)
            return rc;
        p += rc;
        if (p == end)
            return VAR_ERR_INCOMPLETE_INDEX_SPEC;
        /* parse closing parenthesis */
        if (*p != ')')
            return VAR_ERR_UNCLOSED_BRACKET_IN_INDEX;
        p++;
    }
    /* parse contained variable */
    else if (*p == var->syntax.delim_init) {
        /* parse variable with forced expansion */
        ctx = var_parse_push(ctx, &myctx);
        ctx->force_expand = 1;
        rc = parse_variable(var, ctx, p, end, &tmp);
        ctx = var_parse_pop(ctx);

        if (rc == VAR_ERR_UNDEFINED_VARIABLE) {
            *failed = 1;
            /* parse variable without forced expansion */
            ctx = var_parse_push(ctx, &myctx);
            ctx->force_expand = 0;
            rc = parse_variable(var, ctx, p, end, &tmp);
            ctx = var_parse_pop(ctx);
            if (rc < 0)
                return rc;
            p += rc;
            *result = 0;
            tokenbuf_free(&tmp);      /* KES 11/9/2003 */
        } else if (rc < 0) {
            return rc;
        } else {
            p += rc;
            /* parse remaining numerical expression */
            rc = parse_numexp(var, ctx, tmp.begin, tmp.end, result, failed);
            tokenbuf_free(&tmp);
            if (rc < 0)
                return rc;
        }
    }
    /* parse relative index mark ("#") */
    else if (   var->syntax.index_mark != EOS
             && *p == var->syntax.index_mark) {
        p++;
        *result = ctx->index_this;
        if (ctx->rel_lookup_flag)
            ctx->rel_lookup_cnt++;
    }
    /* parse plain integer number */
    else if (isdigit(*p)) {
        rc = parse_integer(var, ctx, p, end, result);
        p += rc;
    }
    /* parse signed positive integer number */
    else if (*p == '+') {
        if ((end - p) > 1 && isdigit(p[1])) {
            p++;
            rc = parse_integer(var, ctx, p, end, result);
            p += rc;
        }
        else
            return VAR_ERR_INVALID_CHAR_IN_INDEX_SPEC;
    }
    /* parse signed negative integer number */
    else if (*p == '-') {
        if (end - p > 1 && isdigit(p[1])) {
            p++;
            rc = parse_integer(var, ctx, p, end, result);
            *result = -(*result);
            p += rc;
        }
        else
            return VAR_ERR_INVALID_CHAR_IN_INDEX_SPEC;
    }
    /* else we failed to parse anything reasonable */
    else
        return VAR_ERR_INVALID_CHAR_IN_INDEX_SPEC;

    return (p - begin);
}

/* parse numerical expression ("x+y") */
static int
parse_numexp(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    int *result, int *failed)
{
    const char *p;
    char op;
    int right;
    int rc;

    /* initialization */
    p = begin;
    if (p == end)
        return VAR_ERR_INCOMPLETE_INDEX_SPEC;

    /* parse left numerical operand */
    rc = parse_numexp_operand(var, ctx, p, end, result, failed);
    if (rc < 0)
        return rc;
    p += rc;

    /* parse numerical operator */
    while (p != end) {
        if (*p == '+' || *p == '-') {
            op = *p++;
            /* recursively parse right operand (light binding) */
            rc = parse_numexp(var, ctx, p, end, &right, failed);
            if (rc < 0)
                return rc;
            p += rc;
            if (op == '+')
                *result = (*result + right);
            else
                *result = (*result - right);
        }
        else if (*p == '*' || *p == '/' || *p == '%') {
            op = *p++;
            /* recursively parse right operand (string binding) */
            rc = parse_numexp_operand(var, ctx, p, end, &right, failed);
            if (rc < 0)
                return rc;
            p += rc;
            if (op == '*')
                *result = (*result * right);
            else if (op == '/') {
                if (right == 0) {
                    if (*failed)
                        *result = 0;
                    else
                        return VAR_ERR_DIVISION_BY_ZERO_IN_INDEX;
                }
                else
                    *result = (*result / right);
            }
            else if (op == '%') {
                if (right == 0) {
                    if (*failed)
                        *result = 0;
                    else
                        return VAR_ERR_DIVISION_BY_ZERO_IN_INDEX;
                }
                else
                    *result = (*result % right);
            }
        }
        else
            break;
    }

    /* return amount of parsed input */
    return (p - begin);
}

/* parse variable name ("abc") */
static int
parse_name(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end)
{
    const char *p;

    /* parse as long as name class characters are found */
    for (p = begin; p != end && var->syntax_nameclass[(int)(*p)]; p++)
        ;
    return (p - begin);
}

/* lookup a variable value through the callback function */
static int
lookup_value(
    var_t *var, var_parse_t *ctx,
    const char  *var_ptr, int  var_len, int var_inc, int var_idx,
    const char **val_ptr, int *val_len, int *val_size)
{
    char buf[1];
    int rc;

    /* pass through to original callback */
    rc = (*var->cb_value_fct)(var, var->cb_value_ctx,
                              var_ptr, var_len, var_inc, var_idx,
                              val_ptr, val_len, val_size);

    /* convert undefined variable into empty variable if relative
       lookups are counted. This is the case inside an active loop
       construct if no limits are given. There the parse_input()
       has to proceed until all variables have undefined values.
       This trick here allows it to determine this case. */
    if (ctx->rel_lookup_flag && rc == VAR_ERR_UNDEFINED_VARIABLE) {
        ctx->rel_lookup_cnt--;
        buf[0] = EOS;
        *val_ptr = bstrdup(buf);
        *val_len = 0;
        *val_size = 0;
        return VAR_OK;
    }

    return rc;
}

/* parse complex variable construct ("${name...}") */
static int
parse_variable_complex(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    tokenbuf_t *result)
{
    const char *p;
    const char *data;
    int len, buffer_size;
    int failed = 0;
    int rc;
    int idx = 0;
    int inc;
    tokenbuf_t name;
    tokenbuf_t tmp;

    /* initializations */
    p = begin;
    tokenbuf_init(&name);
    tokenbuf_init(&tmp);
    tokenbuf_init(result);

    /* parse open delimiter */
    if (p == end || *p != var->syntax.delim_open)
        return 0;
    p++;
    if (p == end)
        return VAR_ERR_INCOMPLETE_VARIABLE_SPEC;

    /* parse name of variable to expand. The name may consist of an
       arbitrary number of variable name character and contained variable
       constructs. */
    do {
        /* parse a variable name */
        rc = parse_name(var, ctx, p, end);
        if (rc < 0)
            goto error_return;
        if (rc > 0) {
            if (!tokenbuf_append(&name, p, rc)) {
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
            p += rc;
        }

        /* parse an (embedded) variable */
        rc = parse_variable(var, ctx, p, end, &tmp);
        if (rc < 0)
            goto error_return;
        if (rc > 0) {
            if (!tokenbuf_merge(&name, &tmp)) {
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
            p += rc;
        }
        tokenbuf_free(&tmp);          /* KES 11/9/2003 */
    } while (rc > 0);

    /* we must have the complete expanded variable name now,
       so make sure we really do. */
    if (name.begin == name.end) {
        if (ctx->force_expand) {
            rc = VAR_ERR_INCOMPLETE_VARIABLE_SPEC;
            goto error_return;
        }
        else {
            /* If no force_expand is requested, we have to back-off.
               We're not sure whether our approach here is 100% correct,
               because it _could_ have side-effects according to Peter
               Simons, but as far as we know and tried it, it is
               correct. But be warned -- RSE */
            tokenbuf_set(result, begin - 1, p, 0);
            goto goahead;
        }
    }

    /* parse an optional index specification */
    if (   var->syntax.index_open != EOS
        && *p == var->syntax.index_open) {
        p++;
        rc = parse_numexp(var, ctx, p, end, &idx, &failed);
        if (rc < 0)
            goto error_return;
        if (rc == 0) {
            rc = VAR_ERR_INCOMPLETE_INDEX_SPEC;
            goto error_return;
        }
        p += rc;
        if (p == end) {
            rc = VAR_ERR_INCOMPLETE_INDEX_SPEC;
            goto error_return;
        }
        if (*p != var->syntax.index_close) {
            rc = VAR_ERR_INVALID_CHAR_IN_INDEX_SPEC;
            goto error_return;
        }
        p++;
    }

    /* parse end of variable construct or start of post-operations */
    if (p == end || (*p != var->syntax.delim_close && *p != ':' && *p != '+')) {
        rc = VAR_ERR_INCOMPLETE_VARIABLE_SPEC;
        goto error_return;
    }
    if ((inc = (*p++ == '+'))) {
       p++;                           /* skip the + */
    }

    /* lookup the variable value now */
    if (failed) {
        tokenbuf_set(result, begin - 1, p, 0);
    } else {
        rc = lookup_value(var, ctx,
                          name.begin, name.end-name.begin, inc, idx,
                          &data, &len, &buffer_size);
        if (rc == VAR_ERR_UNDEFINED_VARIABLE) {
            tokenbuf_init(result); /* delayed handling of undefined variable */
        } else if (rc < 0) {
            goto error_return;
        } else {
            /* the preliminary result is the raw value of the variable.
               This may be modified by the operations that may follow. */
            tokenbuf_set(result, data, data + len, buffer_size);
        }
    }

    /* parse optional post-operations */
goahead:
    if (p[-1] == ':') {
        tokenbuf_free(&tmp);
        tokenbuf_init(&tmp);
        p--;
        while (p != end && *p == ':') {
            p++;
            if (!failed)
                rc = parse_operation(var, ctx, p, end, result);
            else
                rc = parse_operation(var, ctx, p, end, &tmp);
            if (rc < 0)
                goto error_return;
            p += rc;
            if (failed)
                result->end += rc;
        }
        if (p == end || *p != var->syntax.delim_close) {
            rc = VAR_ERR_INCOMPLETE_VARIABLE_SPEC;
            goto error_return;
        }
        p++;
        if (failed)
            result->end++;
    } else if (p[-1] == '+') {
       p++;
    }

    /* lazy handling of undefined variable */
    if (!failed && tokenbuf_isundef(result)) {
        if (ctx->force_expand) {
            rc = VAR_ERR_UNDEFINED_VARIABLE;
            goto error_return;
        } else {
            tokenbuf_set(result, begin - 1, p, 0);
        }
    }

    /* return successfully */
    tokenbuf_free(&name);
    tokenbuf_free(&tmp);
    return (p - begin);

    /* return with an error */
error_return:
    tokenbuf_free(&name);
    tokenbuf_free(&tmp);
    tokenbuf_free(result);
    return rc;
}

/* parse variable construct ("$name" or "${name...}") */
static int
parse_variable(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    tokenbuf_t *result)
{
    const char *p;
    const char *data;
    int len, buffer_size;
    int rc, rc2;
    int inc;

    /* initialization */
    p = begin;
    tokenbuf_init(result);

    /* parse init delimiter */
    if (p == end || *p != var->syntax.delim_init)
        return 0;
    p++;
    if (p == end)
        return VAR_ERR_INCOMPLETE_VARIABLE_SPEC;

    /* parse a simple variable name.
       (if this fails, we're try to parse a complex variable construct) */
    rc = parse_name(var, ctx, p, end);
    if (rc < 0)
        return rc;
    if (rc > 0) {
        inc = (p[rc] == '+');
        rc2 = lookup_value(var, ctx, p, rc, inc, 0, &data, &len, &buffer_size);
        if (rc2 == VAR_ERR_UNDEFINED_VARIABLE && !ctx->force_expand) {
            tokenbuf_set(result, begin, begin + 1 + rc, 0);
            return (1 + rc);
        }
        if (rc2 < 0)
            return rc2;
        tokenbuf_set(result, data, data + len, buffer_size);
        return (1 + rc);
    }

    /* parse a complex variable construct (else case) */
    rc = parse_variable_complex(var, ctx, p, end, result);
    if (rc > 0)
        rc++;
    return rc;
}

/* parse loop construct limits ("[...]{b,s,e}") */
static var_rc_t
parse_looplimits(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    int *start, int *step, int *stop, int *open_stop)
{
    const char *p;
    int rc;
    int failed;

    /* initialization */
    p = begin;

    /* we are happy if nothing is to left to parse */
    if (p == end)
        return VAR_OK;

    /* parse start delimiter */
    if (*p != var->syntax.delim_open)
        return VAR_OK;
    p++;

    /* parse loop start value */
    failed = 0;
    rc = parse_numexp(var, ctx, p, end, start, &failed);
    if (rc == VAR_ERR_INVALID_CHAR_IN_INDEX_SPEC)
        *start = 0; /* use default */
    else if (rc < 0)
        return (var_rc_t)rc;
    else
        p += rc;
    if (failed)
        return VAR_ERR_UNDEFINED_VARIABLE;

    /* parse separator */
    if (*p != ',')
        return VAR_ERR_INVALID_CHAR_IN_LOOP_LIMITS;
    p++;

    /* parse loop step value */
    failed = 0;
    rc = parse_numexp(var, ctx, p, end, step, &failed);
    if (rc == VAR_ERR_INVALID_CHAR_IN_INDEX_SPEC)
        *step = 1; /* use default */
    else if (rc < 0)
        return (var_rc_t)rc;
    else
        p += rc;
    if (failed)
        return VAR_ERR_UNDEFINED_VARIABLE;

    /* parse separator */
    if (*p != ',') {
        /* if not found, parse end delimiter */
        if (*p != var->syntax.delim_close)
            return VAR_ERR_INVALID_CHAR_IN_LOOP_LIMITS;
        p++;

        /* shift step value to stop value */
        *stop = *step;
        *step = 1;

        /* determine whether loop end is open */
        if (rc > 0)
            *open_stop = 0;
        else
            *open_stop = 1;
        return (var_rc_t)(p - begin);
    }
    p++;

    /* parse loop stop value */
    failed = 0;
    rc = parse_numexp(var, ctx, p, end, stop, &failed);
    if (rc == VAR_ERR_INVALID_CHAR_IN_INDEX_SPEC) {
        *stop = 0; /* use default */
        *open_stop = 1;
    }
    else if (rc < 0)
        return (var_rc_t)rc;
    else {
        *open_stop = 0;
        p += rc;
    }
    if (failed)
        return VAR_ERR_UNDEFINED_VARIABLE;

    /* parse end delimiter */
    if (*p != var->syntax.delim_close)
        return VAR_ERR_INVALID_CHAR_IN_LOOP_LIMITS;
    p++;

    /* return amount of parsed input */
    return (var_rc_t)(p - begin);
}

/* parse plain text */
static int
parse_text(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end)
{
    const char *p;

    /* parse until delim_init (variable construct)
       or index_open (loop construct) is found */
    for (p = begin; p != end; p++) {
        if (*p == var->syntax.escape) {
            p++; /* skip next character */
            if (p == end)
                return VAR_ERR_INCOMPLETE_QUOTED_PAIR;
        }
        else if (*p == var->syntax.delim_init)
            break;
        else if (   var->syntax.index_open != EOS
                 && (   *p == var->syntax.index_open
                     || *p == var->syntax.index_close))
            break;
    }
    return (p - begin);
}

/* expand input in general */
static var_rc_t
parse_input(
    var_t *var, var_parse_t *ctx,
    const char *begin, const char *end,
    tokenbuf_t *output, int recursion_level)
{
    const char *p;
    int rc, rc2;
    tokenbuf_t result;
    int start, step, stop, open_stop;
    int i;
    int output_backup;
    int rel_lookup_cnt;
    int loop_limit_length;
    var_parse_t myctx;

    /* initialization */
    p = begin;

    do {
        /* try to parse a loop construct */
        if (   p != end
            && var->syntax.index_open != EOS
            && *p == var->syntax.index_open) {
            p++;

            /* loop preparation */
            loop_limit_length = -1;
            rel_lookup_cnt = ctx->rel_lookup_cnt;
            open_stop = 1;
            rc = 0;
            start = 0;
            step  = 1;
            stop  = 0;
            output_backup = 0;

            /* iterate over loop construct, either as long as there is
               (still) nothing known about the limit, or there is an open
               (=unknown) limit stop and there are still defined variables
               or there is a stop limit known and it is still not reached */
            re_loop:
            for (i = start;
                 (   (   open_stop
                      && (   loop_limit_length < 0
                          || rel_lookup_cnt > ctx->rel_lookup_cnt))
                  || (   !open_stop
                      && i <= stop)                                );
                 i += step) {

                /* remember current output end for restoring */
                output_backup = (output->end - output->begin);

                /* open temporary context for recursion */
                ctx = var_parse_push(ctx, &myctx);
                ctx->force_expand    = 1;
                ctx->rel_lookup_flag = 1;
                ctx->index_this      = i;

                /* recursive parse input through ourself */
                rc = parse_input(var, ctx, p, end,
                                 output, recursion_level+1);

                /* retrieve info and close temporary context */
                rel_lookup_cnt = ctx->rel_lookup_cnt;
                ctx = var_parse_pop(ctx);

                /* error handling */
                if (rc < 0)
                    goto error_return;

                /* make sure the loop construct is closed */
                if (p[rc] != var->syntax.index_close) {
                    rc = VAR_ERR_UNTERMINATED_LOOP_CONSTRUCT;
                    goto error_return;
                }

                /* try to parse loop construct limit specification */
                if (loop_limit_length < 0) {
                    rc2 = parse_looplimits(var, ctx, p+rc+1, end,
                                           &start, &step, &stop, &open_stop);
                    if (rc2 < 0)
                        goto error_return;
                    else if (rc2 == 0)
                        loop_limit_length = 0;
                    else if (rc2 > 0) {
                        loop_limit_length = rc2;
                        /* restart loop from scratch */
                        output->end = (output->begin + output_backup);
                        goto re_loop;
                    }
                }
            }

            /* if stop value is open, restore to the output end
               because the last iteration was just to determine the loop
               termination and its result has to be discarded */
            if (open_stop)
                output->end = (output->begin + output_backup);

            /* skip parsed loop construct */
            p += rc;
            p++;
            p += loop_limit_length;

            continue;
        }

        /* try to parse plain text */
        rc = parse_text(var, ctx, p, end);
        if (rc > 0) {
            if (!tokenbuf_append(output, p, rc)) {
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
            p += rc;
            continue;
        } else if (rc < 0)
            goto error_return;

        /* try to parse a variable construct */
        tokenbuf_init(&result);
        rc = parse_variable(var, ctx, p, end, &result);
        if (rc > 0) {
            if (!tokenbuf_merge(output, &result)) {
                tokenbuf_free(&result);
                rc = VAR_ERR_OUT_OF_MEMORY;
                goto error_return;
            }
            tokenbuf_free(&result);
            p += rc;
            continue;
        }
        tokenbuf_free(&result);
        if (rc < 0)
            goto error_return;

    } while (p != end && rc > 0);

    /* We do not know whether this really could happen, but because we
       are paranoid, report an error at the outer most parsing level if
       there is still any input. Because this would mean that we are no
       longer able to parse the remaining input as a loop construct, a
       text or a variable construct. This would be very strange, but
       could perhaps happen in case of configuration errors!?... */
    if (recursion_level == 0 && p != end) {
        rc = VAR_ERR_INPUT_ISNT_TEXT_NOR_VARIABLE;
        goto error_return;
    }

    /* return amount of parsed text */
    return (var_rc_t)(p - begin);

    /* return with an error where as a special case the output begin is
       set to the input begin and the output end to the last input parsing
       position. */
    error_return:
    tokenbuf_free(output);
    tokenbuf_set(output, begin, p, 0);
    return (var_rc_t)rc;
}

/*
**
**  ==== APPLICATION PROGRAMMING INTERFACE (API) ====
**
*/

/* create variable expansion context */
var_rc_t
var_create(
    var_t **pvar)
{
    var_t *var;

    if (pvar == NULL)
        return VAR_RC(VAR_ERR_INVALID_ARGUMENT);
    if ((var = (var_t *)malloc(sizeof(var_t))) == NULL)
        return VAR_RC(VAR_ERR_OUT_OF_MEMORY);
    memset(var, 0, sizeof(var_t));
    var_config(var, VAR_CONFIG_SYNTAX, &var_syntax_default);
    *pvar = var;
    return VAR_OK;
}

/* destroy variable expansion context */
var_rc_t
var_destroy(
    var_t *var)
{
    if (var == NULL)
        return VAR_RC(VAR_ERR_INVALID_ARGUMENT);
    free(var);
    return VAR_OK;
}

/* configure variable expansion context */
var_rc_t
var_config(
    var_t *var,
    var_config_t mode,
    ...)
{
    va_list ap;
    var_rc_t rc = VAR_OK;

    if (var == NULL) {
        return VAR_RC(VAR_ERR_INVALID_ARGUMENT);
    }

    va_start(ap, mode);
    switch (mode) {
        case VAR_CONFIG_SYNTAX: {
            var_syntax_t *s;
            s = (var_syntax_t *)va_arg(ap, void *);
            if (s == NULL) {
                rc = VAR_ERR_INVALID_ARGUMENT;
                goto bail_out;
            }
            var->syntax.escape      = s->escape;
            var->syntax.delim_init  = s->delim_init;
            var->syntax.delim_open  = s->delim_open;
            var->syntax.delim_close = s->delim_close;
            var->syntax.index_open  = s->index_open;
            var->syntax.index_close = s->index_close;
            var->syntax.index_mark  = s->index_mark;
            var->syntax.name_chars  = NULL; /* unused internally */
            if ((rc = expand_character_class(s->name_chars, var->syntax_nameclass)) != VAR_OK) {
               goto bail_out;
            }
            if (var->syntax_nameclass[(int)var->syntax.delim_init] ||
                var->syntax_nameclass[(int)var->syntax.delim_open] ||
                var->syntax_nameclass[(int)var->syntax.delim_close] ||
                var->syntax_nameclass[(int)var->syntax.escape]) {
                rc = VAR_ERR_INVALID_CONFIGURATION;
                goto bail_out;
            }
            break;
        }
        case VAR_CONFIG_CB_VALUE: {
            var_cb_value_t fct;
            void *ctx;

            fct = (var_cb_value_t)va_arg(ap, void *);
            ctx = (void *)va_arg(ap, void *);
            var->cb_value_fct = fct;
            var->cb_value_ctx = ctx;
            break;
        }
        case VAR_CONFIG_CB_OPERATION: {
            var_cb_operation_t fct;
            void *ctx;

            fct = (var_cb_operation_t)va_arg(ap, void *);
            ctx = (void *)va_arg(ap, void *);
            var->cb_operation_fct = fct;
            var->cb_operation_ctx = ctx;
            break;
        }
        default:
            rc = VAR_ERR_INVALID_ARGUMENT;
            goto bail_out;
    }

bail_out:
    va_end(ap);
    return VAR_RC(rc);
}

/* perform unescape operation on a buffer */
var_rc_t
var_unescape(
    var_t *var,
    const char *src, int srclen,
    char *dst, int dstlen,
    int all)
{
    const char *end;
    var_rc_t rc;

    if (var == NULL || src == NULL || dst == NULL)
        return VAR_RC(VAR_ERR_INVALID_ARGUMENT);
    end = src + srclen;
    while (src < end) {
        if (*src == '\\') {
            if (++src == end)
                return VAR_RC(VAR_ERR_INCOMPLETE_NAMED_CHARACTER);
            switch (*src) {
                case '\\':
                    if (!all) {
                        *dst++ = '\\';
                    }
                    *dst++ = '\\';
                    break;
                case 'n':
                    *dst++ = '\n';
                    break;
                case 't':
                    *dst++ = '\t';
                    break;
                case 'r':
                    *dst++ = '\r';
                    break;
                case 'x':
                    ++src;
                    if ((rc = expand_hex(&src, &dst, end)) != VAR_OK)
                        return VAR_RC(rc);
                    break;
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    if (   end - src >= 3
                        && isdigit((int)src[1])
                        && isdigit((int)src[2])) {
                        if ((rc = expand_octal(&src, &dst, end)) != 0)
                            return VAR_RC(rc);
                        break;
                    }
                default:
                    if (!all) {
                        *dst++ = '\\';
                    }
                    *dst++ = *src;
            }
            ++src;
        } else
            *dst++ = *src++;
    }
    *dst = EOS;
    return VAR_OK;
}

/* perform expand operation on a buffer */
var_rc_t
var_expand(
    var_t *var,
    const char *src_ptr, int src_len,
    char **dst_ptr, int *dst_len,
    int force_expand)
{
    var_parse_t ctx;
    tokenbuf_t output;
    var_rc_t rc;

    /* argument sanity checks */
    if (var == NULL || src_ptr == NULL || src_len == 0 || dst_ptr == NULL)
        return VAR_RC(VAR_ERR_INVALID_ARGUMENT);

    /* prepare internal expansion context */
    ctx.lower           = NULL;
    ctx.force_expand    = force_expand;
    ctx.rel_lookup_flag = 0;
    ctx.rel_lookup_cnt  = 0;
    ctx.index_this      = 0;

    /* start the parsing */
    tokenbuf_init(&output);
    rc = parse_input(var, &ctx, src_ptr, src_ptr+src_len, &output, 0);

    /* post-processing */
    if (rc >= 0) {
        /* always EOS-terminate output for convinience reasons
           but do not count the EOS-terminator in the length */
        if (!tokenbuf_append(&output, "\0", 1)) {
            tokenbuf_free(&output);
            return VAR_RC(VAR_ERR_OUT_OF_MEMORY);
        }
        output.end--;

        /* provide result */
        *dst_ptr = (char *)output.begin;
        if (dst_len != NULL)
            *dst_len = (output.end - output.begin);
        rc = VAR_OK;
    }
    else {
        /* provide result */
        if (dst_len != NULL)
            *dst_len = (output.end - output.begin);
    }

    return VAR_RC(rc);
}

/* format and expand a string */
var_rc_t
var_formatv(
    var_t *var,
    char **dst_ptr, int force_expand,
    const char *fmt, va_list ap)
{
    var_rc_t rc;
    char *cpBuf;
    int nBuf = 5000;

    /* argument sanity checks */
    if (var == NULL || dst_ptr == NULL || fmt == NULL)
        return VAR_RC(VAR_ERR_INVALID_ARGUMENT);

    /* perform formatting */
    if ((cpBuf = (char *)malloc(nBuf+1)) == NULL)
        return VAR_RC(VAR_ERR_OUT_OF_MEMORY);
    nBuf = var_mvsnprintf(cpBuf, nBuf+1, fmt, ap);
    if (nBuf == -1) {
        free(cpBuf);
        return VAR_RC(VAR_ERR_FORMATTING_FAILURE);
    }

    /* perform expansion */
    if ((rc = var_expand(var, cpBuf, nBuf, dst_ptr, NULL, force_expand)) != VAR_OK) {
        free(cpBuf);
        return VAR_RC(rc);
    }

    /* cleanup */
    free(cpBuf);

    return VAR_OK;
}

/* format and expand a string */
var_rc_t
var_format(
    var_t *var,
    char **dst_ptr, int force_expand,
    const char *fmt, ...)
{
    var_rc_t rc;
    va_list ap;

    /* argument sanity checks */
    if (var == NULL || dst_ptr == NULL || fmt == NULL)
        return VAR_RC(VAR_ERR_INVALID_ARGUMENT);

    va_start(ap, fmt);
    rc = var_formatv(var, dst_ptr, force_expand, fmt, ap);
    va_end(ap);

    return VAR_RC(rc);
}

/* var_rc_t to string mapping table */
static const char *var_errors[] = {
    _("everything ok"),                                           /* VAR_OK = 0 */
    _("incomplete named character"),                              /* VAR_ERR_INCOMPLETE_NAMED_CHARACTER */
    _("incomplete hexadecimal value"),                            /* VAR_ERR_INCOMPLETE_HEX */
    _("invalid hexadecimal value"),                               /* VAR_ERR_INVALID_HEX */
    _("octal value too large"),                                   /* VAR_ERR_OCTAL_TOO_LARGE */
    _("invalid octal value"),                                     /* VAR_ERR_INVALID_OCTAL */
    _("incomplete octal value"),                                  /* VAR_ERR_INCOMPLETE_OCTAL */
    _("incomplete grouped hexadecimal value"),                    /* VAR_ERR_INCOMPLETE_GROUPED_HEX */
    _("incorrect character class specification"),                 /* VAR_ERR_INCORRECT_CLASS_SPEC */
    _("invalid expansion configuration"),                         /* VAR_ERR_INVALID_CONFIGURATION */
    _("out of memory"),                                           /* VAR_ERR_OUT_OF_MEMORY */
    _("incomplete variable specification"),                       /* VAR_ERR_INCOMPLETE_VARIABLE_SPEC */
    _("undefined variable"),                                      /* VAR_ERR_UNDEFINED_VARIABLE */
    _("input is neither text nor variable"),                      /* VAR_ERR_INPUT_ISNT_TEXT_NOR_VARIABLE */
    _("unknown command character in variable"),                   /* VAR_ERR_UNKNOWN_COMMAND_CHAR */
    _("malformatted search and replace operation"),               /* VAR_ERR_MALFORMATTED_REPLACE */
    _("unknown flag in search and replace operation"),            /* VAR_ERR_UNKNOWN_REPLACE_FLAG */
    _("invalid regex in search and replace operation"),           /* VAR_ERR_INVALID_REGEX_IN_REPLACE */
    _("missing parameter in command"),                            /* VAR_ERR_MISSING_PARAMETER_IN_COMMAND */
    _("empty search string in search and replace operation"),     /* VAR_ERR_EMPTY_SEARCH_STRING */
    _("start offset missing in cut operation"),                   /* VAR_ERR_MISSING_START_OFFSET */
    _("offsets in cut operation delimited by unknown character"), /* VAR_ERR_INVALID_OFFSET_DELIMITER */
    _("range out of bounds in cut operation"),                    /* VAR_ERR_RANGE_OUT_OF_BOUNDS */
    _("offset out of bounds in cut operation"),                   /* VAR_ERR_OFFSET_OUT_OF_BOUNDS */
    _("logic error in cut operation"),                            /* VAR_ERR_OFFSET_LOGIC */
    _("malformatted transpose operation"),                        /* VAR_ERR_MALFORMATTED_TRANSPOSE */
    _("source and target class mismatch in transpose operation"), /* VAR_ERR_TRANSPOSE_CLASSES_MISMATCH */
    _("empty character class in transpose operation"),            /* VAR_ERR_EMPTY_TRANSPOSE_CLASS */
    _("incorrect character class in transpose operation"),        /* VAR_ERR_INCORRECT_TRANSPOSE_CLASS_SPEC */
    _("malformatted padding operation"),                          /* VAR_ERR_MALFORMATTED_PADDING */
    _("width parameter missing in padding operation"),            /* VAR_ERR_MISSING_PADDING_WIDTH */
    _("fill string missing in padding operation"),                /* VAR_ERR_EMPTY_PADDING_FILL_STRING */
    _("unknown quoted pair in search and replace operation"),     /* VAR_ERR_UNKNOWN_QUOTED_PAIR_IN_REPLACE */
    _("sub-matching reference out of range"),                     /* VAR_ERR_SUBMATCH_OUT_OF_RANGE */
    _("invalid argument"),                                        /* VAR_ERR_INVALID_ARGUMENT */
    _("incomplete quoted pair"),                                  /* VAR_ERR_INCOMPLETE_QUOTED_PAIR */
    _("lookup function does not support variable arrays"),        /* VAR_ERR_ARRAY_LOOKUPS_ARE_UNSUPPORTED */
    _("index of array variable contains an invalid character"),   /* VAR_ERR_INVALID_CHAR_IN_INDEX_SPEC */
    _("index of array variable is incomplete"),                   /* VAR_ERR_INCOMPLETE_INDEX_SPEC */
    _("bracket expression in array variable's index not closed"), /* VAR_ERR_UNCLOSED_BRACKET_IN_INDEX */
    _("division by zero error in index specification"),           /* VAR_ERR_DIVISION_BY_ZERO_IN_INDEX */
    _("unterminated loop construct"),                             /* VAR_ERR_UNTERMINATED_LOOP_CONSTRUCT */
    _("invalid character in loop limits"),                        /* VAR_ERR_INVALID_CHAR_IN_LOOP_LIMITS */
    _("malformed operation argument list"),                       /* VAR_ERR_MALFORMED_OPERATION_ARGUMENTS */
    _("undefined operation"),                                     /* VAR_ERR_UNDEFINED_OPERATION */
    _("formatting failure")                                       /* VAR_ERR_FORMATTING_FAILURE */
};

/* translate a return code into its corresponding descriptive text */
const char *var_strerror(var_t *var, var_rc_t rc)
{
    const char *str;
    rc = (var_rc_t)(0 - rc);
    if (rc < 0 || rc >= (int)sizeof(var_errors) / (int)sizeof(char *)) {
        str = _("unknown error");
    } else {
        str = (char *)var_errors[rc];
    }
    return str;
}
