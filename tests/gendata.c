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
#include <droplet.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/types.h>

void gen_data(char *id, char *buf, int len)
{
  char *p;
  char *e;
  int i, id_len;

  id_len = strlen(id);

  p = id;
  e = buf + (len/id_len)*id_len;
  for (;buf < e; buf += id_len) {
    memcpy(buf, p, id_len);
  }
  for (i=0;i < len;i++) {
    buf[i] = p[i%id_len];
  }
}

int check_data(char *id, char *buf, int len)
{
  char *p;
  int i, id_len;

  id_len = strlen(id);

  p = id;
  for (i = 0;i < len;i++)
    {
      if (buf[i] != p[i%id_len])
        {
          fprintf(stderr,
                  "byte %d differ: %d != %d\n", i, buf[i], p[i%id_len]);
          return -1;
        }
    }

  return 0;
}

uint64_t
get_oid(int oflag, struct drand48_data *pdrbuffer)
{
  static uint64_t static_oid = 0;
  static pthread_mutex_t oid_lock = PTHREAD_MUTEX_INITIALIZER;
  uint64_t oid;

  if (1 == oflag)
    {
      long int tmp;
      pthread_mutex_lock(&oid_lock);
      #if defined(__APPLE__) && defined(__MACH__) || defined(__ellcc__ )
      tmp = lrand48();
      #else
      lrand48_r(pdrbuffer, &tmp);
      #endif
      oid = tmp;
      pthread_mutex_unlock(&oid_lock);
      return oid;
    }
  else
    {
      pthread_mutex_lock(&oid_lock);
      oid = static_oid++;
      pthread_mutex_unlock(&oid_lock);

      return oid;
    }
}
