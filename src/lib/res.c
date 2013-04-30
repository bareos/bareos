/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *  This file handles locking and seaching resources
 *
 *     Kern Sibbald, January MM
 *       Split from parse_conf.c April MMV
 *
 */

#include "bacula.h"

/* Each daemon has a slightly different set of
 * resources, so it will define the following
 * global values.
 */
extern int32_t r_first;
extern int32_t r_last;
extern RES_TABLE resources[];
extern RES **res_head;

brwlock_t res_lock;                   /* resource lock */
static int res_locked = 0;            /* resource chain lock count -- for debug */


/* #define TRACE_RES */

void b_LockRes(const char *file, int line)
{
   int errstat;
#ifdef TRACE_RES
   Pmsg4(000, "LockRes  locked=%d w_active=%d at %s:%d\n", 
         res_locked, res_lock.w_active, file, line);
    if (res_locked) {
       Pmsg2(000, "LockRes writerid=%d myid=%d\n", res_lock.writer_id,
          pthread_self());
     }
#endif
   if ((errstat=rwl_writelock(&res_lock)) != 0) {
      Emsg3(M_ABORT, 0, _("rwl_writelock failure at %s:%d:  ERR=%s\n"),
           file, line, strerror(errstat));
   }
   res_locked++;
}

void b_UnlockRes(const char *file, int line)
{
   int errstat;
   if ((errstat=rwl_writeunlock(&res_lock)) != 0) {
      Emsg3(M_ABORT, 0, _("rwl_writeunlock failure at %s:%d:. ERR=%s\n"),
           file, line, strerror(errstat));
   }
   res_locked--;
#ifdef TRACE_RES
   Pmsg4(000, "UnLockRes locked=%d wactive=%d at %s:%d\n", 
         res_locked, res_lock.w_active, file, line);
#endif
}

/*
 * Return resource of type rcode that matches name
 */
RES *
GetResWithName(int rcode, const char *name)
{
   RES *res;
   int rindex = rcode - r_first;

   LockRes();
   res = res_head[rindex];
   while (res) {
      if (strcmp(res->name, name) == 0) {
         break;
      }
      res = res->next;
   }
   UnlockRes();
   return res;

}

/*
 * Return next resource of type rcode. On first
 * call second arg (res) is NULL, on subsequent
 * calls, it is called with previous value.
 */
RES *
GetNextRes(int rcode, RES *res)
{
   RES *nres;
   int rindex = rcode - r_first;

   if (res == NULL) {
      nres = res_head[rindex];
   } else {
      nres = res->next;
   }
   return nres;
}


/* Parser state */
enum parse_state {
   p_none,
   p_resource
};
