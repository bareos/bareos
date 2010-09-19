/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
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

#include "dpl_sh.h"

/** 
 * ask for confirmation
 * 
 * @param str 
 * 
 * @return 0 if 'Yes', else 1
 */
int 
ask_for_confirmation(char *str)
{
  char buf[256];
  ssize_t cc;
  char *p;

  fprintf(stderr, "%s (please type 'Yes' to confirm) ", str);
  fflush(stderr);
  cc = read(0, buf, sizeof(buf));
  if (-1 == cc)
    {
      perror("read");
      exit(1);
    }
  
  buf[sizeof (buf) - 1] = 0;

  if ((p = index(buf, '\n')))
    *p = 0;
  
  if (!strcmp(buf, "Yes"))
    return 0;

  return 1;
}

int
write_all(int fd,
          char *buf,
          int len)
{
  ssize_t cc;
  int remain;

  remain = len;
  while (1)
    {
    again:
      cc = write(fd, buf, remain);
      if (-1 == cc)
        {
          if (EINTR == errno)
            goto again;

          return -1;
        }
      
      remain -= cc;
      buf += cc;

      if (0 == remain)
        return 0;
    }
}

int
read_all(int fd,
         char *buf,
         int len)
{
  ssize_t cc;
  int remain;
  
  remain = len;
  while (1)
    {
    again:
      cc = read(fd, buf, remain);
      if (-1 == cc)
        {
          if (EINTR == errno)
            goto again;

          return -1;
        }
      
      if (0 == cc && 0 != len)
        {
          return -2;
        }

      remain -= cc;
      buf += cc;

      if (0 == remain)
        {
          return 0;
        }
    }
}

int
read_fd(int fd,
        char **data_bufp,
        u_int *data_lenp)
{
  char buf[8192];
  ssize_t cc;
  char *data_buf = NULL;
  u_int data_len = 0;

  while (1)
    {
      cc = read(fd, buf, sizeof (buf));
      if (-1 == cc)
        {
          return -1;
        }
      
      if (0 == cc)
        {
          break ;
        }
      
      if (NULL == data_buf)
        {
          data_buf = malloc(cc);
          if (NULL == data_buf)
            return -1;
        }
      else
        {
          data_buf = realloc(data_buf, data_len + cc);
          if (NULL == data_buf)
            return -1;
        }

      memcpy(data_buf + data_len, buf, cc);
      data_len += cc;
    }

  if (NULL != data_bufp)
    *data_bufp = data_buf;
  
  if (NULL != data_lenp)
    *data_lenp = data_len;

  return 0;
}

