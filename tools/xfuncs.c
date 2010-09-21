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

#include "dplsh.h"

char *
xstrdup(char *str)
{
  char *nstr;

  nstr = strdup(str);
  if (NULL == nstr)
    {
      perror("strdup");
      exit(1);
    }

  return nstr;
}

void *
xmalloc(size_t size)
{
  void *ptr;

  ptr = malloc(size);
  if (NULL == ptr)
    xperror("malloc");

  return ptr;
}

void 
xperror(char *str)
{
  perror(str);
  exit(1);
}
