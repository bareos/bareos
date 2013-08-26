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
#include "dropletp.h"

dpl_sbuf_t *
dpl_sbuf_new(int size)
{
  dpl_sbuf_t *sb = NULL;

  sb = malloc(sizeof(dpl_sbuf_t));
  if (NULL == sb)
    {
      return NULL;
    }

  sb->buf=malloc(size);
  if (NULL == sb->buf)
    {
      free(sb);
      return NULL;
    }
  
  sb->len=0;
  sb->allocated=size;

  return sb;
}

dpl_sbuf_t *
dpl_sbuf_new_from_str(const char *str)
{
  dpl_sbuf_t *sb = NULL;

  sb = malloc(sizeof(dpl_sbuf_t));
  if (NULL == sb)
    {
      return NULL;
    }

  sb->allocated = strlen(str) + 1;
  sb->buf = malloc(sb->allocated);
  if (NULL == sb->buf)
    {
      free(sb);
      return NULL;
    }
  memcpy(sb->buf, str, sb->allocated);
  
  sb->len = sb->allocated - 1;

  return sb;
}

dpl_status_t
dpl_sbuf_add(dpl_sbuf_t *sb, const char *buf, int len)
{
  if (sb->len+len+1 > sb->allocated)
    {
      char *tmp = NULL;

      tmp = realloc(sb->buf, sb->len+len+1);
      if (NULL == tmp)
	{
	  return DPL_FAILURE;
	}

      sb->buf = tmp;
      sb->allocated = sb->len+len+1;
    }

  memcpy(&sb->buf[sb->len], buf, len);
  sb->buf[sb->len+len] = '\0';
  sb->len = sb->len+len;

  return DPL_SUCCESS;
}

dpl_status_t dpl_sbuf_add_str(dpl_sbuf_t *sb, const char *str)
{
  return dpl_sbuf_add(sb, str, strlen(str));
}

dpl_sbuf_t *dpl_sbuf_dup(const dpl_sbuf_t *src)
{
  dpl_sbuf_t *dst;

  dst = dpl_sbuf_new(src->allocated);
  if (NULL == dst)
    return NULL;

  (void)dpl_sbuf_add(dst, src->buf, src->len);

  return dst;
}

char *dpl_sbuf_get_str(dpl_sbuf_t *sbuf)
{
  return sbuf->buf;
}

void 
dpl_sbuf_free(dpl_sbuf_t *sb)
{
  free(sb->buf);
  free(sb);

  return;
}

void
dpl_sbuf_print(FILE *f, dpl_sbuf_t *sb)
{
  fprintf(f, "%.*s", sb->len, sb->buf);
}
