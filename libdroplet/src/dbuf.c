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

dpl_dbuf_t *
dpl_dbuf_new(void)
{
  return calloc(1, sizeof (dpl_dbuf_t));
}

void
dpl_dbuf_free(dpl_dbuf_t *nbuf)
{
  if (nbuf->data)
    free(nbuf->data);
  free(nbuf);
}

#if 0
static void
buf_drain(dpl_dbuf_t *nbuf, size_t len)
{
  if (nbuf->offset != 0) {
    memmove(nbuf->data, nbuf->data + nbuf->offset, nbuf->data_max-nbuf->offset);
    nbuf->data_max -= nbuf->offset;
    nbuf->offset    = 0;
  }

  if (nbuf->data_max >= len)
    nbuf->data_max = len;

  if (nbuf->data_max == 0) {
    free(nbuf->data);
    nbuf->data = NULL;
    nbuf->offset = 0;
    nbuf->real_size = 0;
  }
}
#endif

int
dpl_dbuf_length(dpl_dbuf_t *nbuf)
{
  return nbuf->data_max - nbuf->offset;
}

static void *
buf_recompute_length(dpl_dbuf_t *nbuf, size_t size)
{
  int	 real_size;
  unsigned char	*data;

  /* there is enough room without doing a shift */
  if (size <= nbuf->real_size - nbuf->data_max)
    return nbuf->data;

  /* there is enough room if we reshift */
  if (size <= nbuf->real_size - nbuf->data_max + nbuf->offset) {
    memmove(nbuf->data, nbuf->data + nbuf->offset, nbuf->data_max-nbuf->offset);
    nbuf->data_max -= nbuf->offset;
    nbuf->offset    = 0;
    return nbuf->data;
  }

  /* not enough room, we realloc AND shift */
  real_size = MAX(dpl_pow2_next(nbuf->real_size + size), 1024);
  data = realloc(nbuf->data, real_size);
  if (NULL == data)
    return NULL;
  nbuf->data = data;

  memmove(nbuf->data, nbuf->data + nbuf->offset, nbuf->data_max-nbuf->offset);
  nbuf->offset    = 0;
  nbuf->data_max -= nbuf->offset;
  nbuf->real_size = real_size;

  return nbuf->data;
}

int
dpl_dbuf_add(dpl_dbuf_t *nbuf, const void *buf, int size)
{
  unsigned char	*data;

  data = buf_recompute_length(nbuf, size);
  if (NULL == data)
    return 0;
  memcpy(data + nbuf->data_max, buf, size);

  nbuf->data  = data;
  nbuf->data_max += size;
  return 1;
}

#if 0
int
dpl_dbuf_add_buffer(dpl_dbuf_t *nbuf, dpl_dbuf_t *nbuf2)
{
  unsigned char *data;

  data = buf_recompute_length(nbuf, nbuf2->data_max - nbuf2->offset);
  if (NULL == data)
    return 0;
  memcpy(data + nbuf->data_max, nbuf2->data + nbuf2->offset,
	 nbuf2->data_max - nbuf2->offset);

  nbuf->data  = data;
  nbuf->data_max += (nbuf2->data_max - nbuf2->offset);

  return 1;
}

int
dpl_dbuf_add_printf(dpl_dbuf_t *nbuf, const char *fmt, ...)
{
  char		*str = NULL;
  unsigned char	*data = NULL;
  int		 len;
  int		 ret;
  va_list	 ap;

  va_start(ap, fmt);

  len = vasprintf(&str, fmt, ap);
  if (len == -1)
    {
      ret = 0;
      goto end;
    }

  data = buf_recompute_length(nbuf, len);
  if (NULL == data)
    {
      ret = 0;
      goto end;
    }
  memcpy(data + nbuf->data_max, str, len);

  nbuf->data  = data;
  nbuf->data_max += len;

  ret = 1;

 end:
  free (str);
  va_end(ap);
  return ret;
}

int
dpl_dbuf_write(dpl_dbuf_t *nbuf, int fd)
{
  int	ret;

  /* we're done with that buffer */
  if (nbuf->data_max == nbuf->offset)
    return 0;

 eintr:
  ret = write(fd, nbuf->data + nbuf->offset, nbuf->data_max - nbuf->offset);
  if (ret == -1 && errno == EINTR)
    goto eintr;

  if (ret == -1)
    return -1;

  nbuf->offset += ret;

  if (nbuf->offset == nbuf->data_max)
    buf_drain(nbuf, 0);

  return ret;
}

int
dpl_dbuf_read(dpl_dbuf_t *nbuf, int fd, int size)
{
  int	ret;
  unsigned char *data;

  data = buf_recompute_length(nbuf, size);
  if (NULL == data)
    return 0;

 eintr:
  ret = read(fd, data + nbuf->data_max, size);
  if (ret == -1 && errno == EINTR)
    goto eintr;

  if (ret == -1)
    return -1;

  nbuf->data  = data;
  nbuf->data_max += ret;

  return ret;
}

int
dpl_dbuf_consume(dpl_dbuf_t *nbuf, void *buf, int size)
{
  if (size > dpl_dbuf_length(nbuf))
    size = dpl_dbuf_length(nbuf);

  memcpy(buf, nbuf->data + nbuf->offset, size);
  nbuf->offset += size;
  return size;
}
#endif
