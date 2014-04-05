/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
 * This file handles locking and seaching resources
 *
 * Kern Sibbald, January MM
 * Split from parse_conf.c April MMV
 */

#include "bareos.h"
#include "generic_res.h"

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
      if (bstrcmp(res->name, name)) {
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

bool MSGSRES::print_config(POOL_MEM &buff)
{
   int len;
   POOLMEM *cmdbuf;
   POOL_MEM cfg_str;    /* configuration as string  */
   POOL_MEM temp;
   MSGSRES* msgres;
   DEST *d;
   msgres = this;//(MSGSRES*) *items[i].value;

   pm_strcat(cfg_str, "Messages {\n");
   Mmsg(temp, "   %s = \"%s\"\n", "Name", msgres->name());
   pm_strcat(cfg_str, temp.c_str());

   cmdbuf = get_pool_memory(PM_NAME);
   if (msgres->mail_cmd) {
      len = strlen(msgres->mail_cmd);
      cmdbuf = check_pool_memory_size(cmdbuf, len * 2);
      escape_string(cmdbuf, msgres->mail_cmd, len);
      Mmsg(temp, "   mailcommand = \"%s\"\n", cmdbuf);
      pm_strcat(cfg_str, temp.c_str());
   }

   if (msgres->operator_cmd) {
      len = strlen(msgres->operator_cmd);
      cmdbuf = check_pool_memory_size(cmdbuf, len * 2);
      escape_string(cmdbuf, msgres->operator_cmd, len);
      Mmsg(temp, "   operatorcommand = \"%s\"\n", cmdbuf);
      pm_strcat(cfg_str, temp.c_str());
   }
   free_pool_memory(cmdbuf);

   for (d = msgres->dest_chain; d; d = d->next) {
      int nr_set = 0;
      int nr_unset = 0;
      POOL_MEM t; /* number of set   types */
      POOL_MEM u; /* number of unset types */

      for (int i = 0; msg_destinations[i].code; i++) {
         if (msg_destinations[i].code == d->dest_code) {
            if (msg_destinations[i].where) {
               Mmsg(temp, "   %s = %s = ", msg_destinations[i].destination, d->where);
            } else {
               Mmsg(temp, "   %s = ", msg_destinations[i].destination);
            }
            pm_strcat(cfg_str, temp.c_str());
            break;
         }
      }

      for (int j = 0; j < M_MAX - 1; j++) {
         if bit_is_set(msg_types[j].token, d->msg_types) {
            nr_set ++;
            Mmsg(temp, ",%s" , msg_types[j].name );
            pm_strcat(t, temp.c_str());
         } else {
            Mmsg(temp, ",!%s" , msg_types[j].name );
            nr_unset++;
            pm_strcat(u, temp.c_str());
         }
      }

      if (nr_set > nr_unset) {               /* if more is set than is unset */
         pm_strcat(cfg_str,"all");           /* all, but not ... */
         pm_strcat(cfg_str, u.c_str());
      } else {                               /* only print set types */
         pm_strcat(cfg_str, t.c_str() + 1);  /* skip first comma */
      }
      pm_strcat(cfg_str, "\n");
   }

   pm_strcat (cfg_str, "}\n\n");
   pm_strcat (buff, cfg_str.c_str());

   return true;
}
