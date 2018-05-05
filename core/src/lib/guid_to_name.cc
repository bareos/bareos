/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Kern Sibbald

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * for preformance reasons.
 */

#include "include/bareos.h"
#include "lib/edit.h"

#ifndef WIN32
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

struct guitem {
   dlink link;
   char *name;
   union {
      uid_t uid;
      gid_t gid;
   };
};


guid_list *new_guid_list()
{
   guid_list *list;
   guitem *item = NULL;
   list = (guid_list *)malloc(sizeof(guid_list));
   list->uid_list = New(dlist(item, &item->link));
   list->gid_list = New(dlist(item, &item->link));
   return list;
}

void FreeGuidList(guid_list *list)
{
   guitem *item;
   foreach_dlist(item, list->uid_list) {
      free(item->name);
   }
   foreach_dlist(item, list->gid_list) {
      free(item->name);
   }
   delete list->uid_list;
   delete list->gid_list;
   free(list);
}

static int UidCompare(void *item1, void *item2)
{
   guitem *i1 = (guitem *)item1;
   guitem *i2 = (guitem *)item2;
   if (i1->uid < i2->uid) {
      return -1;
   } else if (i1->uid > i2->uid) {
      return 1;
   } else {
      return 0;
   }
}

static int GidCompare(void *item1, void *item2)
{
   guitem *i1 = (guitem *)item1;
   guitem *i2 = (guitem *)item2;
   if (i1->gid < i2->gid) {
      return -1;
   } else if (i1->gid > i2->gid) {
      return 1;
   } else {
      return 0;
   }
}


static void GetUidname(uid_t uid, guitem *item)
{
#ifndef HAVE_WIN32
   struct passwd *pwbuf;
   P(mutex);
   pwbuf = getpwuid(uid);
   if (pwbuf != NULL && !bstrcmp(pwbuf->pw_name, "????????")) {
      item->name = bstrdup(pwbuf->pw_name);
   }
   V(mutex);
#endif
}

static void GetGidname(gid_t gid, guitem *item)
{
#ifndef HAVE_WIN32
   struct group *grbuf;
   P(mutex);
   grbuf = getgrgid(gid);
   if (grbuf != NULL && !bstrcmp(grbuf->gr_name, "????????")) {
      item->name = bstrdup(grbuf->gr_name);
   }
   V(mutex);
#endif
}


char *guid_list::uid_to_name(uid_t uid, char *name, int maxlen)
{
   guitem sitem, *item, *fitem;
   sitem.uid = uid;
   char buf[50];

   item = (guitem *)uid_list->binary_search(&sitem, UidCompare);
   Dmsg2(900, "uid=%d item=%p\n", uid, item);
   if (!item) {
      item = (guitem *)malloc(sizeof(guitem));
      item->uid = uid;
      item->name = NULL;
      GetUidname(uid, item);
      if (!item->name) {
         item->name = bstrdup(edit_int64(uid, buf));
         Dmsg2(900, "set uid=%d name=%s\n", uid, item->name);
      }
      fitem = (guitem *)uid_list->binary_insert(item, UidCompare);
      if (fitem != item) {               /* item already there this shouldn't happen */
         free(item->name);
         free(item);
         item = fitem;
      }
   }
   bstrncpy(name, item->name, maxlen);
   return name;
}

char *guid_list::gid_to_name(gid_t gid, char *name, int maxlen)
{
   guitem sitem, *item, *fitem;
   sitem.gid = gid;
   char buf[50];

   item = (guitem *)gid_list->binary_search(&sitem, GidCompare);
   if (!item) {
      item = (guitem *)malloc(sizeof(guitem));
      item->gid = gid;
      item->name = NULL;
      GetGidname(gid, item);
      if (!item->name) {
         item->name = bstrdup(edit_int64(gid, buf));
      }
      fitem = (guitem *)gid_list->binary_insert(item, GidCompare);
      if (fitem != item) {               /* item already there this shouldn't happen */
         free(item->name);
         free(item);
         item = fitem;
      }
   }

   bstrncpy(name, item->name, maxlen);
   return name;
}
