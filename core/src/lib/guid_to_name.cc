/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Kern Sibbald
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
/*
 * Written by Kern Sibbald, July 2007 to replace idcache.c
 *
 * Program to convert uid and gid into names, and cache the results
 * for performance reasons.
 */

#include <pwd.h>
#include <grp.h>
#include "include/bareos.h"
#include "lib/guid_to_name.h"
#include "lib/edit.h"
#include <algorithm>
#include <vector>

#ifndef WIN32
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

struct guitem {
  char* name;
  union {
    uid_t uid;
    gid_t gid;
  };
};


guid_list* new_guid_list()
{
  guid_list* list;
  list = (guid_list*)malloc(sizeof(guid_list));
  list->uid_list = new std::vector<guitem*>();
  list->gid_list = new std::vector<guitem*>();
  return list;
}

void FreeGuidList(guid_list* list)
{
  for (auto* item : *list->uid_list) {
    free(item->name);
    free(item);
  }
  for (auto* item : *list->gid_list) {
    free(item->name);
    free(item);
  }
  delete list->uid_list;
  delete list->gid_list;
  free(list);
}

static int UidCompare(guitem* item1, guitem* item2)
{
  guitem* i1 = item1;
  guitem* i2 = item2;
  if (i1->uid < i2->uid) {
    return -1;
  } else if (i1->uid > i2->uid) {
    return 1;
  } else {
    return 0;
  }
}

static int GidCompare(guitem* item1, guitem* item2)
{
  guitem* i1 = item1;
  guitem* i2 = item2;
  if (i1->gid < i2->gid) {
    return -1;
  } else if (i1->gid > i2->gid) {
    return 1;
  } else {
    return 0;
  }
}

#ifdef HAVE_WIN32
static void GetUidname(uid_t, guitem*) {}
static void GetGidname(gid_t, guitem*) {}
#else
static void GetUidname(uid_t uid, guitem* item)
{
  struct passwd* pwbuf;
  lock_mutex(mutex);
  pwbuf = getpwuid(uid);
  if (pwbuf != NULL && !bstrcmp(pwbuf->pw_name, "????????")) {
    item->name = strdup(pwbuf->pw_name);
  }
  unlock_mutex(mutex);
}

static void GetGidname(gid_t gid, guitem* item)
{
  struct group* grbuf;
  lock_mutex(mutex);
  grbuf = getgrgid(gid);
  if (grbuf != NULL && !bstrcmp(grbuf->gr_name, "????????")) {
    item->name = strdup(grbuf->gr_name);
  }
  unlock_mutex(mutex);
}
#endif


char* guid_list::uid_to_name(uid_t uid, char* name, int maxlen)
{
  guitem sitem, *item, *fitem;
  sitem.uid = uid;
  char buf[50];

  {
    auto it = std::lower_bound(uid_list->begin(), uid_list->end(), &sitem,
        [](guitem* a, guitem* b) { return UidCompare(a, b) < 0; });
    item = (it != uid_list->end() && UidCompare(*it, &sitem) == 0) ? *it : nullptr;
  }
  Dmsg2(900, "uid=%d item=%p\n", uid, item);
  if (!item) {
    item = (guitem*)malloc(sizeof(guitem));
    item->uid = uid;
    item->name = NULL;
    GetUidname(uid, item);
    if (!item->name) {
      item->name = strdup(edit_int64(uid, buf));
      Dmsg2(900, "set uid=%d name=%s\n", uid, item->name);
    }
    auto it = std::lower_bound(uid_list->begin(), uid_list->end(), item,
        [](guitem* a, guitem* b) { return UidCompare(a, b) < 0; });
    if (it != uid_list->end() && UidCompare(*it, item) == 0) {
      fitem = *it;
    } else {
      uid_list->insert(it, item);
      fitem = item;
    }
    if (fitem != item) { /* item already there this shouldn't happen */
      free(item->name);
      free(item);
      item = fitem;
    }
  }
  bstrncpy(name, item->name, maxlen);
  return name;
}

char* guid_list::gid_to_name(gid_t gid, char* name, int maxlen)
{
  guitem sitem, *item, *fitem;
  sitem.gid = gid;
  char buf[50];

  {
    auto it = std::lower_bound(gid_list->begin(), gid_list->end(), &sitem,
        [](guitem* a, guitem* b) { return GidCompare(a, b) < 0; });
    item = (it != gid_list->end() && GidCompare(*it, &sitem) == 0) ? *it : nullptr;
  }
  if (!item) {
    item = (guitem*)malloc(sizeof(guitem));
    item->gid = gid;
    item->name = NULL;
    GetGidname(gid, item);
    if (!item->name) { item->name = strdup(edit_int64(gid, buf)); }
    auto it = std::lower_bound(gid_list->begin(), gid_list->end(), item,
        [](guitem* a, guitem* b) { return GidCompare(a, b) < 0; });
    if (it != gid_list->end() && GidCompare(*it, item) == 0) {
      fitem = *it;
    } else {
      gid_list->insert(it, item);
      fitem = item;
    }
    if (fitem != item) { /* item already there this shouldn't happen */
      free(item->name);
      free(item);
      item = fitem;
    }
  }

  bstrncpy(name, item->name, maxlen);
  return name;
}
