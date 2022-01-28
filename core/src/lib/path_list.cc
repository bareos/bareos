/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2022 Bareos GmbH & Co. KG

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

// Kern Sibbald, September MMVII

#include "include/bareos.h"
#include "lib/path_list.h"
#include "lib/htable.h"

#define debuglevel 50

// Initialize the path hash table
PathList* path_list_init()
{
  CurDir* elt = NULL;
  return new PathList(elt, &elt->link, 10000);
}

/*
 * Add a path to the hash when we create a directory with the replace=NEVER
 * option
 */
bool PathListAdd(PathList* path_list, uint32_t len, const char* fname)
{
  CurDir* item;

  if (!path_list) { return false; }

  // We store CurDir, fname in the same chunk
  item = (CurDir*)path_list->hash_malloc(sizeof(CurDir) + len + 1);

  memset(item, 0, sizeof(CurDir));
  memcpy(item->fname, fname, len + 1);

  path_list->insert(item->fname, item);

  Dmsg1(debuglevel, "add fname=<%s>\n", fname);

  return true;
}

bool PathListLookup(PathList* path_list, const char* fname)
{
  int len;
  bool found = false;
  POOLMEM* filename;

  if (!path_list) { return false; }

  filename = GetPoolMemory(PM_FNAME);
  PmStrcpy(filename, fname);

  // Strip trailing /
  len = strlen(filename);
  if (len == 0) {
    FreePoolMemory(filename);
    return false;
  }
  len--;

  if (filename[len] == '/') { /* strip any trailing slash */
    filename[len] = 0;
  }

  CurDir* temp = (CurDir*)path_list->lookup(filename);
  if (temp) { found = true; }

  Dmsg2(debuglevel, "lookup <%s> %s\n", filename, found ? "ok" : "not ok");

  return found;
}

void FreePathList(PathList* path_list) { delete path_list; }
