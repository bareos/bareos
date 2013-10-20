/*
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
#ifndef __DROPLET_DBUF_H__
#define __DROPLET_DBUF_H__ 1

typedef struct
{
  unsigned char *data;
  size_t data_max;
  size_t real_size;
  size_t offset;
} dpl_dbuf_t;

dpl_dbuf_t *dpl_dbuf_new(void);
void dpl_dbuf_free(dpl_dbuf_t *nbuf);
int dpl_dbuf_add(dpl_dbuf_t *nbuf, const void *buf, int len);
// int dpl_dbuf_add_buffer(dpl_dbuf_t *nbuf, dpl_dbuf_t *nbuf2);
// int dpl_dbuf_add_printf(dpl_dbuf_t *nbuf, const char *fmt, ...);
// int dpl_dbuf_write(dpl_dbuf_t *nbuf, int fd);
// int dpl_dbuf_read(dpl_dbuf_t *nbuf, int fd, int size);
int dpl_dbuf_length(dpl_dbuf_t *nbuf);
// int dpl_dbuf_consume(dpl_dbuf_t *nbuf, void *buf, int size);
#endif
