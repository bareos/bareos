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
extern CONFIG *my_config;

/*
 * Set default indention e.g. 2 spaces.
 */
#define DEFAULT_INDENT_STRING "  "

/*
 * Define the Union of all the common resource structure definitions.
 */
union URES {
   MSGSRES  res_msgs;
   RES hdr;
};

#if defined(_MSC_VER)
// work around visual studio name mangling preventing external linkage since res_all
// is declared as a different type when instantiated.
extern "C" URES res_all;
#else
extern  URES res_all;
#endif

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
RES *GetResWithName(int rcode, const char *name)
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
RES *GetNextRes(int rcode, RES *res)
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

static void indent_config_item(POOL_MEM &cfg_str, int level, const char *config_item)
{
   int i;

   for (i = 0; i < level; i++) {
      pm_strcat(cfg_str, DEFAULT_INDENT_STRING);
   }
   pm_strcat(cfg_str, config_item);
}

static inline void print_config_size(RES_ITEM *item, POOL_MEM &cfg_str)
{
   POOL_MEM temp;
   bool print_item = false;

   /*
    * convert default value string to numeric value
    */
   static const char *modifier[] = {
      "g",
      "m",
      "k",
      "",
      NULL
   };
   const int64_t multiplier[] = {
      1073741824,    /* gibi */
      1048576,       /* mebi */
      1024,          /* kibi */
      1              /* byte */
   };

   if ((item->flags & CFG_ITEM_REQUIRED) || !my_config->m_omit_defaults) {
      /*
       * Always print required items or if my_config->m_omit_defaults is false
       */
      print_item = true;
   } else if (item->flags & CFG_ITEM_DEFAULT) {
      /*
       * We have a default value
       */
      if (*(item->i64value) != str_to_int64(item->default_value)) {
         /*
          * Print if value differs from default
          */
         print_item = true;
      }
   }

   if (print_item) {
      POOL_MEM volspec;   /* vol specification string*/
      int64_t bytes = *(item->i64value);
      int factor;

      if (bytes == 0) {
         pm_strcat(volspec, "0");
      } else {
         for (int t=0; modifier[t]; t++) {
            Dmsg2(200, " %s bytes: %d\n", item->name,  bytes);
            factor = bytes / multiplier[t];
            bytes  = bytes % multiplier[t];
            if (factor > 0) {
               Mmsg(temp, "%d %s ", factor, modifier[t]);
               pm_strcat(volspec, temp.c_str());
               Dmsg1(200, " volspec: %s\n", volspec.c_str());
            }
            if (bytes == 0) {
               break;
            }
         }
      }

      Mmsg(temp, "%s = %s\n", item->name, volspec.c_str());
      indent_config_item(cfg_str, 1, temp.c_str());
   }
}

static inline void print_config_time(RES_ITEM *item, POOL_MEM &cfg_str)
{
   POOL_MEM temp;
   bool print_item = false;

   /*
    * Reverse time formatting: 1 Month, 1 Week, etc.
    *
    * convert default value string to numeric value
    */
   static const char *modifier[] = {
      "years",
      "months",
      "weeks",
      "days",
      "hours",
      "minutes",
      "seconds",
      NULL
   };
   static const int32_t multiplier[] = {
      60 * 60 * 24 * 365,
      60 * 60 * 24 * 30,
      60 * 60 * 24 * 7,
      60 * 60 * 24,
      60 * 60,
      60,
      1,
      0
   };

   if ((item->flags & CFG_ITEM_REQUIRED) || !my_config->m_omit_defaults) {
      /*
       * Always print required items or if my_config->m_omit_defaults is false
       */
      print_item = true;
   } else if (item->flags & CFG_ITEM_DEFAULT) {
      /*
       * We have a default value
       */
      if (*(item->i64value) != str_to_int64(item->default_value)) {
         /*
          * Print if value differs from default
          */
         print_item = true;
      }
   }

   if (print_item) {
      POOL_MEM timespec;
      utime_t secs = *(item->utimevalue);
      int factor;
      if (secs == 0) {
         pm_strcat(timespec, "0");
      } else {
         for (int t=0; modifier[t]; t++) {
            factor = secs / multiplier[t];
            secs   = secs % multiplier[t];
            if (factor > 0) {
               Mmsg(temp, "%d %s ", factor, modifier[t]);
               pm_strcat(timespec, temp.c_str());
            }
            if (secs == 0) {
               break;
            }
         }
      }

      Mmsg(temp, "%s = %s\n", item->name, timespec.c_str());
      indent_config_item(cfg_str, 1, temp.c_str());
   }
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

bool BRSRES::print_config(POOL_MEM &buff)
{
   RES_ITEM *items;
   int rindex = this->hdr.rcode - r_first;
   int i = 0;
   POOL_MEM cfg_str;
   POOL_MEM temp;

   /*
    * Make sure the resource class has any items.
    */
   if (!resources[rindex].items) {
      return true;
   }

   memcpy(&res_all, this, resources[rindex].size);

   pm_strcat(cfg_str, res_to_str(this->hdr.rcode));
   pm_strcat(cfg_str, " {\n");

   items = resources[rindex].items;

   for (i = 0; items[i].name; i++) {
      bool print_item = false;

      switch (items[i].type) {
      case CFG_TYPE_STR:
      case CFG_TYPE_DIR:
      case CFG_TYPE_NAME:
      case CFG_TYPE_STRNAME:
         /*
          * String types
          */
         if ((items[i].flags & CFG_ITEM_REQUIRED) || !my_config->m_omit_defaults) {
            /*
             * Always print required items or if my_config->m_omit_defaults is false
             */
            print_item = true;
         } else if (items[i].flags & CFG_ITEM_DEFAULT) {
            /*
             * We have a default value
             */
            if (!bstrcmp(*(items[i].value), items[i].default_value)) {
               /*
                * Print if value differs from default
                */
               print_item = true;
            }
         }

         if (print_item && *(items[i].value) != NULL) {
            Dmsg2(200, "%s = \"%s\"\n", items[i].name, *(items[i].value));
            Mmsg(temp, "%s = \"%s\"\n", items[i].name, *(items[i].value));
            indent_config_item(cfg_str, 1, temp.c_str());
         }
         break;
      case CFG_TYPE_MD5PASSWORD:
      case CFG_TYPE_CLEARPASSWORD:
      case CFG_TYPE_AUTOPASSWORD: {
         s_password *password;

         password = items[i].pwdvalue;
         if ((items[i].flags & CFG_ITEM_REQUIRED) || !my_config->m_omit_defaults) {
            /*
             * Always print required items or if my_config->m_omit_defaults is false
             */
            print_item = true;
         } else if (items[i].flags & CFG_ITEM_DEFAULT) {
            /*
             * We have a default value
             */
            if (!bstrcmp(password->value, items[i].default_value)) {
               /*
                * Print if value differs from default
                */
               print_item = true;
            }
         }

         if (print_item && password && password->value != NULL) {
            switch (password->encoding) {
            case p_encoding_clear:
               Dmsg2(200, "%s = \"%s\"\n", items[i].name, password->value);
               Mmsg(temp, "%s = \"%s\"\n", items[i].name, password->value);
               break;
            case p_encoding_md5:
               Dmsg2(200, "%s = \"[md5]%s\"\n", items[i].name, password->value);
               Mmsg(temp, "%s = \"[md5]%s\"\n", items[i].name, password->value);
               break;
            default:
               break;
            }
            indent_config_item(cfg_str, 1, temp.c_str());
         }
         break;
      }
      case CFG_TYPE_LABEL: {
         for (int j = 0; tapelabels[j].name; j++) {
            if (*(items[i].ui32value) == tapelabels[j].token) {
               Mmsg(temp, "%s = \"%s\"\n", items[i].name, tapelabels[j].name);
               indent_config_item(cfg_str, 1, temp.c_str());
               break;
            }
         }
         break;
      }
      case CFG_TYPE_INT32:
      case CFG_TYPE_PINT32:
      case CFG_TYPE_INT64:
      case CFG_TYPE_SPEED:
         /*
          * Integer types
          */
         if ((items[i].flags & CFG_ITEM_REQUIRED) || !my_config->m_omit_defaults) {
            /*
             * Always print required items or if my_config->m_omit_defaults is false
             */
            print_item = true;
         } else if (items[i].flags & CFG_ITEM_DEFAULT) {
            /*
             * We have a default value
             */
            if (*(items[i].i64value) != str_to_int64(items[i].default_value)) {
               /*
                * Print if value differs from default
                */
               print_item = true;
            }
         }

         if (print_item) {
            Mmsg(temp, "%s = %d\n", items[i].name, *(items[i].value));
            indent_config_item(cfg_str, 1, temp.c_str());
         }
         break;
      case CFG_TYPE_SIZE64:
      case CFG_TYPE_SIZE32:
         print_config_size(&items[i], cfg_str);
         break;
      case CFG_TYPE_TIME:
         print_config_time(&items[i], cfg_str);
         break;
      case CFG_TYPE_BOOL:
         if ((items[i].flags & CFG_ITEM_REQUIRED) || !my_config->m_omit_defaults) {
            /*
             * Always print required items or if my_config->m_omit_defaults is false
             */
            print_item = true;
         } else if (items[i].flags & CFG_ITEM_DEFAULT) {
            /*
             * we have a default value
             */
            bool default_value = bstrcasecmp(items[i].default_value, "true") ||
                                 bstrcasecmp(items[i].default_value, "yes");

            /*
             * Print if value differs from default
             */
            if (*items[i].boolvalue != default_value) {
               print_item = true;
            }
         }

         if (print_item) {
            if (*items[i].boolvalue) {
               Mmsg(temp, "%s = %s\n", items[i].name, NT_("yes"));
            } else {
               Mmsg(temp, "%s = %s\n", items[i].name, NT_("no"));
            }
            indent_config_item(cfg_str, 1, temp.c_str());
         }
         break;
      case CFG_TYPE_ALIST_STR:
      case CFG_TYPE_ALIST_DIR: {
        /*
         * One line for each member of the list
         */
         char *value;
         alist* list;
         list = (alist *) *(items[i].value);

         if (list != NULL) {
            foreach_alist(value, list) {
               Mmsg(temp, "%s = %s\n", items[i].name, value);
               indent_config_item(cfg_str, 1, temp.c_str());
            }
         }
         break;
      }
      case CFG_TYPE_ALIST_RES: {
         /*
          * Each member of the list is comma-separated
          */
         int cnt = 0;
         RES *res;
         alist* list;
         POOL_MEM res_names;

         list = (alist *) *(items[i].value);
         if (list != NULL) {
            Mmsg(temp, "%s = ", items[i].name);
            indent_config_item(cfg_str, 1, temp.c_str());

            pm_strcpy(res_names, "");
            foreach_alist(res, list) {
               if (cnt) {
                  Mmsg(temp, ",\"%s\"", res->name);
               } else {
                  Mmsg(temp, "\"%s\"", res->name);
               }
               pm_strcat(res_names, temp.c_str());
               cnt++;
            }

            pm_strcat(cfg_str, res_names.c_str());
            pm_strcat(cfg_str, "\n");
         }
         break;
      }
      case CFG_TYPE_RES: {
         RES *res;

         res = (RES *)*(items[i].value);
         if (res != NULL && res->name != NULL) {
            Mmsg(temp, "%s = \"%s\"\n", items[i].name, res->name);
            indent_config_item(cfg_str, 1, temp.c_str());
         }
         break;
      }
      case CFG_TYPE_BIT: {
         if (*(items[i].ui32value) & items[i].code) {
            Mmsg(temp, "%s = %s\n", items[i].name, NT_("yes"));
         } else {
            Mmsg(temp, "%s = %s\n", items[i].name, NT_("no"));
         }
         indent_config_item(cfg_str, 1, temp.c_str());
         break;
      }
      case CFG_TYPE_MSGS:
         /*
          * We ignore these items as they are printed in a special way in MSGSRES::print_config()
          */
         break;
      case CFG_TYPE_ADDRESSES: {
         dlist *addrs = *items[i].dlistvalue;
         IPADDR *adr;

         Mmsg(temp, "%s = {\n", items[i].name);
         indent_config_item(cfg_str, 1, temp.c_str());
         foreach_dlist(adr, addrs) {
            char tmp[1024];

            adr->build_config_str(tmp, sizeof(tmp));
            pm_strcat(cfg_str, tmp);
            pm_strcat(cfg_str, "\n");
         }

         indent_config_item(cfg_str, 1, "}\n");
         break;
      }
      case CFG_TYPE_ADDRESSES_PORT:
         /* is stored in CFG_TYPE_ADDRESSES and printed there  */
         break;
      case CFG_TYPE_ADDRESSES_ADDRESS:
         /* is stored in CFG_TYPE_ADDRESSES and printed there  */
         break;
      default:
         /*
          * This is a non-generic type call back to the daemon to get things printed.
          */
         if (my_config->m_print_res) {
            my_config->m_print_res(items, i, cfg_str);
         }
         break;
      }
   }

   pm_strcat(cfg_str, "}\n\n");
   pm_strcat(buff, cfg_str.c_str());

   return true;
}
