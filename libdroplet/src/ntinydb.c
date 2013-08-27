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

/** @file */

/** 
 * 
 * 
 * @param blob 
 * @param key 
 * @param data 
 * @param datalen 
 * 
 * @return 
 */
dpl_status_t
dpl_ntinydb_set(dpl_sbuf_t *blob,
                const char *key, 
                char *data,
                int datalen)
{
  char flag;
  int keylen, ret;
  uint32_t len;

  flag = 0;
  ret = dpl_sbuf_add(blob, &flag, 1);
  if (-1 == ret)
    return DPL_FAILURE;

  keylen = strlen(key);
  len = htonl(keylen);

  ret = dpl_sbuf_add(blob, (char *)&len, sizeof (len));
  if (-1 == ret)
    return DPL_FAILURE;

  ret = dpl_sbuf_add(blob, key, keylen);
  if (-1 == ret)
    return DPL_FAILURE;

  len = htonl(datalen);
  ret = dpl_sbuf_add(blob, (char *)&len, sizeof (len));
  if (-1 == ret)
    return DPL_FAILURE;

  ret = dpl_sbuf_add(blob, data, datalen);
  if (-1 == ret)
    return DPL_FAILURE;

  //printf("ntinydb_set '%s' '%.*s'\n", key, datalen, data);

  dpl_sbuf_print(stdout, blob);

  return DPL_SUCCESS;
}

dpl_status_t
dpl_ntinydb_get(const char *blob_buf,
                int blob_len,
                const char *key,
                const char **data_returned,
                int *datalen_returned)
{
  int i, keylenref, keylen, datalen, match;
  __attribute__((unused)) char flag;
  uint32_t len;

  keylenref = strlen(key);

  match = 0;
  i = 0;
  while (1)
    {
      //fprintf(stderr, "%d\n", blob_buf[i]);

      if ((i + 1) < blob_len)
        flag = blob_buf[i];
      else
        break ;

      i++;

      if ((i + sizeof (len)) < blob_len)
        memcpy(&len, blob_buf + i, sizeof (len));
      else
        break ;

      i += sizeof (len);

      keylen = ntohl(len);

      if (keylen == keylenref)
        {
          //fprintf(stderr, "%.*s\n", keylen, blob_buf + i);

          if (!memcmp(key, blob_buf + i, keylenref))
            match = 1;
        }

      i += keylen;

      if ((i + sizeof (len)) < blob_len)
        memcpy(&len, blob_buf + i, sizeof (len));
      else
        break ;

      i += sizeof (len);

      datalen = ntohl(len);

      if (1 == match)
        {
          //fprintf(stderr, "match datalen=%d\n", datalen);

          *data_returned = blob_buf + i;
          *datalen_returned = datalen;

          return DPL_SUCCESS;
        }

      i += datalen;
    }

  return DPL_FAILURE;
}

dpl_status_t
dpl_ntinydb_list(const char *blob_buf,
                 int blob_len,
                 dpl_ntinydb_func_t cb_func,
                 void *cb_arg)
{
  int i, keylen, datalen;
  __attribute__((unused)) int match;
  __attribute__((unused)) char flag;
  uint32_t len;

  match = 0;
  i = 0;
  while (1)
    {
      //fprintf(stderr, "%d\n", blob_buf[i]);

      if ((i + 1) < blob_len)
        flag = blob_buf[i];
      else
        break ;

      i++;

      if ((i + sizeof (len)) < blob_len)
        memcpy(&len, blob_buf + i, sizeof (len));
      else
        break ;

      i += sizeof (len);

      keylen = ntohl(len);

      if (NULL != cb_func)
        {
          int ret;

          ret = cb_func(blob_buf + i, keylen, cb_arg);
          if (0 != ret)
            return DPL_FAILURE;
        }

      i += keylen;

      if ((i + sizeof (len)) < blob_len)
        memcpy(&len, blob_buf + i, sizeof (len));
      else
        break ;

      i += sizeof (len);

      datalen = ntohl(len);

      i += datalen;
    }

  return DPL_SUCCESS;
}
