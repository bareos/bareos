/*
 * Copyright (C) 2020-2021 Bareos GmbH & Co. KG
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_SBUF_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_SBUF_H_

typedef struct {
  char* buf;
  size_t len;
  size_t allocated;
} dpl_sbuf_t;

/* PROTO sbuf.c */
dpl_sbuf_t* dpl_sbuf_new(size_t size);
dpl_sbuf_t* dpl_sbuf_new_from_str(const char* str);
dpl_status_t dpl_sbuf_add(dpl_sbuf_t* sb, const char* buf, size_t len);
dpl_status_t dpl_sbuf_add_str(dpl_sbuf_t* sb, const char* str);
dpl_status_t __attribute__((format(printf, 2, 3)))
dpl_sbuf_add_str_fmt(dpl_sbuf_t* sb, const char* format, ...);
dpl_sbuf_t* dpl_sbuf_dup(const dpl_sbuf_t* src);
char* dpl_sbuf_get_str(dpl_sbuf_t* sbuf);
void dpl_sbuf_free(dpl_sbuf_t* sb);
void dpl_sbuf_print(FILE* f, dpl_sbuf_t* sb);
dpl_status_t dpl_sbuf_url_encode(dpl_sbuf_t* sb);

#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_SBUF_H_
