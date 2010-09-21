/*
 * Droplet, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplet
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
      lrand48_r(pdrbuffer, &tmp);
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
