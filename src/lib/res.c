/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

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
 * This file handles most things related to generic resources.
 *
 * Kern Sibbald, January MM
 * Split from parse_conf.c April MMV
 */

#define NEED_JANSSON_NAMESPACE 1
#include "bareos.h"
#include "generic_res.h"

/* Forward referenced subroutines */

extern CONFIG *my_config;             /* Our Global config */

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

static int res_locked = 0;            /* resource chain lock count -- for debug */

/* #define TRACE_RES */

void b_LockRes(const char *file, int line)
{
   int errstat;

#ifdef TRACE_RES
   Pmsg4(000, "LockRes  locked=%d w_active=%d at %s:%d\n",
         res_locked, my_config->m_res_lock.w_active, file, line);

    if (res_locked) {
       Pmsg2(000, "LockRes writerid=%d myid=%d\n",
             my_config->m_res_lock.writer_id, pthread_self());
     }
#endif
   if ((errstat = rwl_writelock(&my_config->m_res_lock)) != 0) {
      Emsg3(M_ABORT, 0, _("rwl_writelock failure at %s:%d:  ERR=%s\n"),
            file, line, strerror(errstat));
   }
   res_locked++;
}

void b_UnlockRes(const char *file, int line)
{
   int errstat;

   if ((errstat = rwl_writeunlock(&my_config->m_res_lock)) != 0) {
      Emsg3(M_ABORT, 0, _("rwl_writeunlock failure at %s:%d:. ERR=%s\n"),
            file, line, strerror(errstat));
   }
   res_locked--;
#ifdef TRACE_RES
   Pmsg4(000, "UnLockRes locked=%d wactive=%d at %s:%d\n",
         res_locked, my_config->m_res_lock.w_active, file, line);
#endif
}

/*
 * Return resource of type rcode that matches name
 */
RES *GetResWithName(int rcode, const char *name, bool lock)
{
   RES *res;
   int rindex = rcode - my_config->m_r_first;

   if (lock) {
      LockRes();
   }

   res = my_config->m_res_head[rindex];
   while (res) {
      if (bstrcmp(res->name, name)) {
         break;
      }
      res = res->next;
   }

   if (lock) {
      UnlockRes();
   }

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
   int rindex = rcode - my_config->m_r_first;

   if (res == NULL) {
      nres = my_config->m_res_head[rindex];
   } else {
      nres = res->next;
   }

   return nres;
}

const char *res_to_str(int rcode)
{
   if (rcode < my_config->m_r_first || rcode > my_config->m_r_last) {
      return _("***UNKNOWN***");
   } else {
      return my_config->m_resources[rcode - my_config->m_r_first].name;
   }
}

/*
 * Scan for message types and add them to the message
 * destination. The basic job here is to connect message types
 * (WARNING, ERROR, FATAL, INFO, ...) with an appropriate
 * destination (MAIL, FILE, OPERATOR, ...)
 */
static void scan_types(LEX *lc, MSGSRES *msg, int dest_code,
                       char *where, char *cmd, char *timestamp_format)
{
   int i;
   bool found, is_not;
   int msg_type = 0;
   char *str;

   for (;;) {
      lex_get_token(lc, T_NAME);            /* expect at least one type */
      found = false;
      if (lc->str[0] == '!') {
         is_not = true;
         str = &lc->str[1];
      } else {
         is_not = false;
         str = &lc->str[0];
      }
      for (i = 0; msg_types[i].name; i++) {
         if (bstrcasecmp(str, msg_types[i].name)) {
            msg_type = msg_types[i].token;
            found = true;
            break;
         }
      }
      if (!found) {
         scan_err1(lc, _("message type: %s not found"), str);
         return;
      }

      if (msg_type == M_MAX + 1) {       /* all? */
         for (i = 1; i <= M_MAX; i++) {      /* yes set all types */
            add_msg_dest(msg, dest_code, i, where, cmd, timestamp_format);
         }
      } else if (is_not) {
         rem_msg_dest(msg, dest_code, msg_type, where);
      } else {
         add_msg_dest(msg, dest_code, msg_type, where, cmd, timestamp_format);
      }
      if (lc->ch != ',') {
         break;
      }
      Dmsg0(900, "call lex_get_token() to eat comma\n");
      lex_get_token(lc, T_ALL);          /* eat comma */
   }
   Dmsg0(900, "Done scan_types()\n");
}

/*
 * Store Messages Destination information
 */
static void store_msgs(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;
   char *cmd = NULL,
        *tsf = NULL;
   POOLMEM *dest;
   int dest_len;
   URES *res_all = (URES *)my_config->m_res_all;

   Dmsg2(900, "store_msgs pass=%d code=%d\n", pass, item->code);

   tsf = res_all->res_msgs.timestamp_format;
   if (pass == 1) {
      switch (item->code) {
      case MD_STDOUT:
      case MD_STDERR:
      case MD_CONSOLE:
      case MD_CATALOG:
         scan_types(lc, (MSGSRES *)(item->value), item->code, NULL, NULL, tsf);
         break;
      case MD_SYSLOG: {            /* syslog */
         char *p;
         int cnt = 0;
         bool done = false;

         /*
          * See if this is an old style syslog definition.
          * We count the number of = signs in the current config line.
          */
         p = lc->line;
         while (!done && *p) {
            switch (*p) {
            case '=':
               cnt++;
               break;
            case ',':
            case ';':
               /*
                * No need to continue scanning when we encounter a ',' or ';'
                */
               done = true;
               break;
            default:
               break;
            }
            p++;
         }

         /*
          * If there is more then one = its the new format e.g.
          * syslog = facility = filter
          */
         if (cnt > 1) {
            dest = get_pool_memory(PM_MESSAGE);
            /*
             * Pick up a single facility.
             */
            token = lex_get_token(lc, T_NAME);   /* Scan destination */
            pm_strcpy(dest, lc->str);
            dest_len = lc->str_len;
            token = lex_get_token(lc, T_SKIP_EOL);

            scan_types(lc, (MSGSRES *)(item->value), item->code, dest, NULL, NULL);
            free_pool_memory(dest);
            Dmsg0(900, "done with dest codes\n");
         } else {
            scan_types(lc, (MSGSRES *)(item->value), item->code, NULL, NULL, NULL);
         }
         break;
      }
      case MD_OPERATOR:            /* Send to operator */
      case MD_DIRECTOR:            /* Send to Director */
      case MD_MAIL:                /* Mail */
      case MD_MAIL_ON_ERROR:       /* Mail if Job errors */
      case MD_MAIL_ON_SUCCESS:     /* Mail if Job succeeds */
         if (item->code == MD_OPERATOR) {
            cmd = res_all->res_msgs.operator_cmd;
         } else {
            cmd = res_all->res_msgs.mail_cmd;
         }
         dest = get_pool_memory(PM_MESSAGE);
         dest[0] = 0;
         dest_len = 0;

         /*
          * Pick up comma separated list of destinations.
          */
         for (;;) {
            token = lex_get_token(lc, T_NAME);   /* Scan destination */
            dest = check_pool_memory_size(dest, dest_len + lc->str_len + 2);
            if (dest[0] != 0) {
               pm_strcat(dest, " ");  /* Separate multiple destinations with space */
               dest_len++;
            }
            pm_strcat(dest, lc->str);
            dest_len += lc->str_len;
            Dmsg2(900, "store_msgs newdest=%s: dest=%s:\n", lc->str, NPRT(dest));
            token = lex_get_token(lc, T_SKIP_EOL);
            if (token == T_COMMA) {
               continue;           /* Get another destination */
            }
            if (token != T_EQUALS) {
               scan_err1(lc, _("expected an =, got: %s"), lc->str);
               return;
            }
            break;
         }
         Dmsg1(900, "mail_cmd=%s\n", NPRT(cmd));
         scan_types(lc, (MSGSRES *)(item->value), item->code, dest, cmd, tsf);
         free_pool_memory(dest);
         Dmsg0(900, "done with dest codes\n");
         break;
      case MD_FILE:                /* File */
      case MD_APPEND:              /* Append */
         dest = get_pool_memory(PM_MESSAGE);

         /*
          * Pick up a single destination.
          */
         token = lex_get_token(lc, T_NAME);   /* Scan destination */
         pm_strcpy(dest, lc->str);
         dest_len = lc->str_len;
         token = lex_get_token(lc, T_SKIP_EOL);
         Dmsg1(900, "store_msgs dest=%s:\n", NPRT(dest));
         if (token != T_EQUALS) {
            scan_err1(lc, _("expected an =, got: %s"), lc->str);
            return;
         }
         scan_types(lc, (MSGSRES *)(item->value), item->code, dest, NULL, tsf);
         free_pool_memory(dest);
         Dmsg0(900, "done with dest codes\n");
         break;
      default:
         scan_err1(lc, _("Unknown item code: %d\n"), item->code);
         return;
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
   Dmsg0(900, "Done store_msgs\n");
}

/*
 * This routine is ONLY for resource names
 * Store a name at specified address.
 */
static void store_name(LEX *lc, RES_ITEM *item, int index, int pass)
{
   POOLMEM *msg = get_pool_memory(PM_EMSG);
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_NAME);
   if (!is_name_valid(lc->str, msg)) {
      scan_err1(lc, "%s\n", msg);
      return;
   }
   free_pool_memory(msg);
   /*
    * Store the name both in pass 1 and pass 2
    */
   if (*(item->value)) {
      scan_err2(lc, _("Attempt to redefine name \"%s\" to \"%s\"."),
                *(item->value), lc->str);
      return;
   }
   *(item->value) = bstrdup(lc->str);
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a name string at specified address
 * A name string is limited to MAX_RES_NAME_LENGTH
 */
static void store_strname(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   /*
    * Store the name
    */
   lex_get_token(lc, T_NAME);
   if (pass == 1) {
      /*
       * If a default was set free it first.
       */
      if (*(item->value)) {
         free(*(item->value));
      }
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a string at specified address
 */
static void store_str(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      /*
       * If a default was set free it first.
       */
      if (*(item->value)) {
         free(*(item->value));
      }
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a directory name at specified address. Note, we do
 * shell expansion except if the string begins with a vertical
 * bar (i.e. it will likely be passed to the shell later).
 */
static void store_dir(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      /*
       * If a default was set free it first.
       */
      if (*(item->value)) {
         free(*(item->value));
      }
      if (lc->str[0] != '|') {
         do_shell_expansion(lc->str, sizeof_pool_memory(lc->str));
      }
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a password at specified address in MD5 coding
 */
static void store_md5password(LEX *lc, RES_ITEM *item, int index, int pass)
{
   s_password *pwd;
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      pwd = item->pwdvalue;

      if (pwd->value) {
         free(pwd->value);
      }

      /*
       * See if we are parsing an MD5 encoded password already.
       */
      if (bstrncmp(lc->str, "[md5]", 5)) {
         pwd->encoding = p_encoding_md5;
         pwd->value = bstrdup(lc->str + 5);
      } else {
         unsigned int i, j;
         MD5_CTX md5c;
         unsigned char digest[CRYPTO_DIGEST_MD5_SIZE];
         char sig[100];

         MD5_Init(&md5c);
         MD5_Update(&md5c, (unsigned char *) (lc->str), lc->str_len);
         MD5_Final(digest, &md5c);
         for (i = j = 0; i < sizeof(digest); i++) {
            sprintf(&sig[j], "%02x", digest[i]);
            j += 2;
         }
         pwd->encoding = p_encoding_md5;
         pwd->value = bstrdup(sig);
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a password at specified address in MD5 coding
 */
static void store_clearpassword(LEX *lc, RES_ITEM *item, int index, int pass)
{
   s_password *pwd;
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      pwd = item->pwdvalue;

      if (pwd->value) {
         free(pwd->value);
      }

      pwd->encoding = p_encoding_clear;
      pwd->value = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a resource at specified address.
 * If we are in pass 2, do a lookup of the
 * resource.
 */
static void store_res(LEX *lc, RES_ITEM *item, int index, int pass)
{
   RES *res;
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_NAME);
   if (pass == 2) {
      res = GetResWithName(item->code, lc->str);
      if (res == NULL) {
         scan_err3(lc, _("Could not find config resource \"%s\" referenced on line %d: %s"),
                   lc->str, lc->line_no, lc->line);
         return;
      }
      if (*(item->resvalue)) {
         scan_err3(lc, _("Attempt to redefine resource \"%s\" referenced on line %d: %s"),
                   item->name, lc->line_no, lc->line);
         return;
      }
      *(item->resvalue) = res;
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a resource pointer in an alist. default_value indicates how many
 * times this routine can be called -- i.e. how many alists there are.
 *
 * If we are in pass 2, do a lookup of the resource.
 */
static void store_alist_res(LEX *lc, RES_ITEM *item, int index, int pass)
{
   RES *res;
   int i = 0;
   alist *list;
   URES *res_all = (URES *)my_config->m_res_all;
   int count = str_to_int32(item->default_value);

   if (pass == 2) {
      if (count == 0) {               /* always store in item->value */
         i = 0;
         if (!item->alistvalue[i]) {
            item->alistvalue[i] = New(alist(10, not_owned_by_alist));
         }
      } else {
         /*
          * Find empty place to store this directive
          */
         while ((item->value)[i] != NULL && i++ < count) { }
         if (i >= count) {
            scan_err4(lc, _("Too many %s directives. Max. is %d. line %d: %s\n"),
                      lc->str, count, lc->line_no, lc->line);
            return;
         }
         item->alistvalue[i] = New(alist(10, not_owned_by_alist));
      }
      list = item->alistvalue[i];

      for (;;) {
         lex_get_token(lc, T_NAME);   /* scan next item */
         res = GetResWithName(item->code, lc->str);
         if (res == NULL) {
            scan_err3(lc, _("Could not find config Resource \"%s\" referenced on line %d : %s\n"),
                      item->name, lc->line_no, lc->line);
            return;
         }
         Dmsg5(900, "Append %p to alist %p size=%d i=%d %s\n", res, list, list->size(), i, item->name);
         list->append(res);
         if (lc->ch != ',') {         /* if no other item follows */
            break;                    /* get out */
         }
         lex_get_token(lc, T_ALL);    /* eat comma */
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a string in an alist.
 */
static void store_alist_str(LEX *lc, RES_ITEM *item, int index, int pass)
{
   alist *list;
   URES *res_all = (URES *)my_config->m_res_all;

   if (pass == 2) {
      if (!*(item->value)) {
         *(item->alistvalue) = New(alist(10, owned_by_alist));
      }
      list = *(item->alistvalue);

      lex_get_token(lc, T_STRING);   /* scan next item */
      Dmsg4(900, "Append %s to alist %p size=%d %s\n",
            lc->str, list, list->size(), item->name);

      /*
       * See if we need to drop the default value from the alist.
       *
       * We first check to see if the config item has the CFG_ITEM_DEFAULT
       * flag set and currently has exactly one entry.
       */
      if ((item->flags & CFG_ITEM_DEFAULT) && list->size() == 1) {
         char *entry;

         entry = (char *)list->first();
         if (bstrcmp(entry, item->default_value)) {
            list->destroy();
            list->init(10, owned_by_alist);
         }
      }

      list->append(bstrdup(lc->str));
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a directory name at specified address in an alist.
 * Note, we do shell expansion except if the string begins
 * with a vertical bar (i.e. it will likely be passed to the
 * shell later).
 */
static void store_alist_dir(LEX *lc, RES_ITEM *item, int index, int pass)
{
   alist *list;
   URES *res_all = (URES *)my_config->m_res_all;

   if (pass == 2) {
      if (!*(item->alistvalue)) {
         *(item->alistvalue) = New(alist(10, owned_by_alist));
      }
      list = *(item->alistvalue);

      lex_get_token(lc, T_STRING);   /* scan next item */
      Dmsg4(900, "Append %s to alist %p size=%d %s\n",
            lc->str, list, list->size(), item->name);

      if (lc->str[0] != '|') {
         do_shell_expansion(lc->str, sizeof_pool_memory(lc->str));
      }

      /*
       * See if we need to drop the default value from the alist.
       *
       * We first check to see if the config item has the CFG_ITEM_DEFAULT
       * flag set and currently has exactly one entry.
       */
      if ((item->flags & CFG_ITEM_DEFAULT) && list->size() == 1) {
         char *entry;

         entry = (char *)list->first();
         if (bstrcmp(entry, item->default_value)) {
            list->destroy();
            list->init(10, owned_by_alist);
         }
      }

      list->append(bstrdup(lc->str));
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a list of plugin names to load by the daemon on startup.
 */
static void store_plugin_names(LEX *lc, RES_ITEM *item, int index, int pass)
{
   alist *list;
   char *p, *plugin_name, *plugin_names;
   URES *res_all = (URES *)my_config->m_res_all;

   if (pass == 2) {
      lex_get_token(lc, T_STRING);   /* scan next item */

      if (!*(item->alistvalue)) {
         *(item->alistvalue) = New(alist(10, owned_by_alist));
      }
      list = *(item->alistvalue);

      plugin_names = bstrdup(lc->str);
      plugin_name = plugin_names;
      while (plugin_name) {
         /*
          * Split the plugin_names string on ':'
          */
         if ((p = strchr(plugin_name, ':'))) {
            *p++ = '\0';
         }

         list->append(bstrdup(plugin_name));
         plugin_name = p;
      }
      free(plugin_names);
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store default values for Resource from xxxDefs
 * If we are in pass 2, do a lookup of the
 * resource and store everything not explicitly set
 * in main resource.
 *
 * Note, here item points to the main resource (e.g. Job, not
 *  the jobdefs, which we look up).
 */
static void store_defs(LEX *lc, RES_ITEM *item, int index, int pass)
{
   RES *res;

   lex_get_token(lc, T_NAME);
   if (pass == 2) {
      Dmsg2(900, "Code=%d name=%s\n", item->code, lc->str);
      res = GetResWithName(item->code, lc->str);
      if (res == NULL) {
         scan_err3(lc, _("Missing config Resource \"%s\" referenced on line %d : %s\n"),
                   lc->str, lc->line_no, lc->line);
         return;
      }
   }
   scan_to_eol(lc);
}

/*
 * Store an integer at specified address
 */
static void store_int16(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_INT16);
   *(item->i16value) = lc->u.int16_val;
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

static void store_int32(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_INT32);
   *(item->i32value) = lc->u.int32_val;
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a positive integer at specified address
 */
static void store_pint16(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_PINT16);
   *(item->ui16value) = lc->u.pint16_val;
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

static void store_pint32(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_PINT32);
   *(item->ui32value) = lc->u.pint32_val;
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store an 64 bit integer at specified address
 */
static void store_int64(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_INT64);
   *(item->i64value) = lc->u.int64_val;
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Unit types
 */
enum unit_type {
   STORE_SIZE,
   STORE_SPEED
};

/*
 * Store a size in bytes
 */
static void store_int_unit(LEX *lc, RES_ITEM *item, int index, int pass,
                           bool size32, enum unit_type type)
{
   int token;
   uint64_t uvalue;
   char bsize[500];
   URES *res_all = (URES *)my_config->m_res_all;

   Dmsg0(900, "Enter store_unit\n");
   token = lex_get_token(lc, T_SKIP_EOL);
   errno = 0;
   switch (token) {
   case T_NUMBER:
   case T_IDENTIFIER:
   case T_UNQUOTED_STRING:
      bstrncpy(bsize, lc->str, sizeof(bsize));  /* save first part */
      /*
       * If terminated by space, scan and get modifier
       */
      while (lc->ch == ' ') {
         token = lex_get_token(lc, T_ALL);
         switch (token) {
         case T_NUMBER:
         case T_IDENTIFIER:
         case T_UNQUOTED_STRING:
            bstrncat(bsize, lc->str, sizeof(bsize));
            break;
         }
      }

      switch (type) {
      case STORE_SIZE:
         if (!size_to_uint64(bsize, &uvalue)) {
            scan_err1(lc, _("expected a size number, got: %s"), lc->str);
            return;
         }
         break;
      case STORE_SPEED:
         if (!speed_to_uint64(bsize, &uvalue)) {
            scan_err1(lc, _("expected a speed number, got: %s"), lc->str);
            return;
         }
         break;
      default:
         scan_err0(lc, _("unknown unit type encountered"));
         return;
      }

      if (size32) {
         *(item->ui32value) = (uint32_t)uvalue;
      } else {
         switch (type) {
         case STORE_SIZE:
            *(item->i64value) = uvalue;
            break;
         case STORE_SPEED:
            *(item->ui64value) = uvalue;
            break;
         }
      }
      break;
   default:
      scan_err2(lc, _("expected a %s, got: %s"),
                (type == STORE_SIZE)?_("size"):_("speed"), lc->str);
      return;
   }
   if (token != T_EOL) {
      scan_to_eol(lc);
   }
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
   Dmsg0(900, "Leave store_unit\n");
}

/*
 * Store a size in bytes
 */
static void store_size32(LEX *lc, RES_ITEM *item, int index, int pass)
{
   store_int_unit(lc, item, index, pass, true /* 32 bit */, STORE_SIZE);
}

/*
 * Store a size in bytes
 */
static void store_size64(LEX *lc, RES_ITEM *item, int index, int pass)
{
   store_int_unit(lc, item, index, pass, false /* not 32 bit */, STORE_SIZE);
}

/*
 * Store a speed in bytes/s
 */
static void store_speed(LEX *lc, RES_ITEM *item, int index, int pass)
{
   store_int_unit(lc, item, index, pass, false /* 64 bit */, STORE_SPEED);
}

/*
 * Store a time period in seconds
 */
static void store_time(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;
   utime_t utime;
   char period[500];
   URES *res_all = (URES *)my_config->m_res_all;

   token = lex_get_token(lc, T_SKIP_EOL);
   errno = 0;
   switch (token) {
   case T_NUMBER:
   case T_IDENTIFIER:
   case T_UNQUOTED_STRING:
      bstrncpy(period, lc->str, sizeof(period));  /* get first part */
      /*
       * If terminated by space, scan and get modifier
       */
      while (lc->ch == ' ') {
         token = lex_get_token(lc, T_ALL);
         switch (token) {
         case T_NUMBER:
         case T_IDENTIFIER:
         case T_UNQUOTED_STRING:
            bstrncat(period, lc->str, sizeof(period));
            break;
         }
      }
      if (!duration_to_utime(period, &utime)) {
         scan_err1(lc, _("expected a time period, got: %s"), period);
         return;
      }
      *(item->utimevalue) = utime;
      break;
   default:
      scan_err1(lc, _("expected a time period, got: %s"), lc->str);
      return;
   }
   if (token != T_EOL) {
      scan_to_eol(lc);
   }
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a yes/no in a bit field
 */
static void store_bit(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_NAME);
   if (bstrcasecmp(lc->str, "yes") || bstrcasecmp(lc->str, "true")) {
      set_bit(item->code, item->bitvalue);
   } else if (bstrcasecmp(lc->str, "no") || bstrcasecmp(lc->str, "false")) {
      clear_bit(item->code, item->bitvalue);
   } else {
      scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE", lc->str); /* YES and NO must not be translated */
      return;
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store a bool in a bit field
 */
static void store_bool(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_NAME);
   if (bstrcasecmp(lc->str, "yes") || bstrcasecmp(lc->str, "true")) {
      *item->boolvalue = true;
   } else if (bstrcasecmp(lc->str, "no") || bstrcasecmp(lc->str, "false")) {
      *(item->boolvalue) = false;
   } else {
      scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE", lc->str); /* YES and NO must not be translated */
      return;
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store Tape Label Type (BAREOS, ANSI, IBM)
 */
static void store_label(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;
   URES *res_all = (URES *)my_config->m_res_all;

   lex_get_token(lc, T_NAME);
   /*
    * Store the label pass 2 so that type is defined
    */
   for (i = 0; tapelabels[i].name; i++) {
      if (bstrcasecmp(lc->str, tapelabels[i].name)) {
         *(item->ui32value) = tapelabels[i].token;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Tape Label keyword, got: %s"), lc->str);
      return;
   }
   scan_to_eol(lc);
   set_bit(index, res_all->hdr.item_present);
   clear_bit(index, res_all->hdr.inherit_content);
}

/*
 * Store network addresses.
 *
 *   my tests
 *   positiv
 *   = { ip = { addr = 1.2.3.4; port = 1205; } ipv4 = { addr = 1.2.3.4; port = http; } }
 *   = { ip = {
 *         addr = 1.2.3.4; port = 1205; }
 *     ipv4 = {
 *         addr = 1.2.3.4; port = http; }
 *     ipv6 = {
 *       addr = 1.2.3.4;
 *       port = 1205;
 *     }
 *     ip = {
 *       addr = 1.2.3.4
 *       port = 1205
 *     }
 *     ip = {
 *       addr = 1.2.3.4
 *     }
 *     ip = {
 *       addr = 2001:220:222::2
 *     }
 *     ip = {
 *       addr = bluedot.thun.net
 *     }
 *   }
 *   negativ
 *   = { ip = { } }
 *   = { ipv4 { addr = doof.nowaytoheavenxyz.uhu; } }
 *   = { ipv4 { port = 4711 } }
 */
static void store_addresses(LEX * lc, RES_ITEM * item, int index, int pass)
{
   int token;
   int exist;
   int family = 0;
   char errmsg[1024];
   char port_str[128];
   char hostname_str[1024];
   enum {
      EMPTYLINE = 0x0,
      PORTLINE = 0x1,
      ADDRLINE = 0x2
   } next_line = EMPTYLINE;
   int port = str_to_int32(item->default_value);

   token = lex_get_token(lc, T_SKIP_EOL);
   if (token != T_BOB) {
      scan_err1(lc, _("Expected a block begin { , got: %s"), lc->str);
   }
   token = lex_get_token(lc, T_SKIP_EOL);
   if (token == T_EOB) {
      scan_err0(lc, _("Empty addr block is not allowed"));
   }
   do {
      if (!(token == T_UNQUOTED_STRING || token == T_IDENTIFIER)) {
         scan_err1(lc, _("Expected a string, got: %s"), lc->str);
      }
      if (bstrcasecmp("ip", lc->str) || bstrcasecmp("ipv4", lc->str)) {
         family = AF_INET;
#ifdef HAVE_IPV6
      } else if (bstrcasecmp("ipv6", lc->str)) {
         family = AF_INET6;
      } else {
         scan_err1(lc, _("Expected a string [ip|ipv4|ipv6], got: %s"), lc->str);
      }
#else
      } else {
         scan_err1(lc, _("Expected a string [ip|ipv4], got: %s"), lc->str);
      }
#endif
      token = lex_get_token(lc, T_SKIP_EOL);
      if (token != T_EQUALS) {
         scan_err1(lc, _("Expected a equal =, got: %s"), lc->str);
      }
      token = lex_get_token(lc, T_SKIP_EOL);
      if (token != T_BOB) {
         scan_err1(lc, _("Expected a block begin { , got: %s"), lc->str);
      }
      token = lex_get_token(lc, T_SKIP_EOL);
      exist = EMPTYLINE;
      port_str[0] = hostname_str[0] = '\0';
      do {
         if (token != T_IDENTIFIER) {
            scan_err1(lc, _("Expected a identifier [addr|port], got: %s"), lc->str);
         }
         if (bstrcasecmp("port", lc->str)) {
            next_line = PORTLINE;
            if (exist & PORTLINE) {
               scan_err0(lc, _("Only one port per address block"));
            }
            exist |= PORTLINE;
         } else if (bstrcasecmp("addr", lc->str)) {
            next_line = ADDRLINE;
            if (exist & ADDRLINE) {
               scan_err0(lc, _("Only one addr per address block"));
            }
            exist |= ADDRLINE;
         } else {
            scan_err1(lc, _("Expected a identifier [addr|port], got: %s"), lc->str);
         }
         token = lex_get_token(lc, T_SKIP_EOL);
         if (token != T_EQUALS) {
            scan_err1(lc, _("Expected a equal =, got: %s"), lc->str);
         }
         token = lex_get_token(lc, T_SKIP_EOL);
         switch (next_line) {
         case PORTLINE:
            if (!(token == T_UNQUOTED_STRING || token == T_NUMBER || token == T_IDENTIFIER)) {
               scan_err1(lc, _("Expected a number or a string, got: %s"), lc->str);
            }
            bstrncpy(port_str, lc->str, sizeof(port_str));
            break;
         case ADDRLINE:
            if (!(token == T_UNQUOTED_STRING || token == T_IDENTIFIER)) {
               scan_err1(lc, _("Expected an IP number or a hostname, got: %s"),
                         lc->str);
            }
            bstrncpy(hostname_str, lc->str, sizeof(hostname_str));
            break;
         case EMPTYLINE:
            scan_err0(lc, _("State machine missmatch"));
            break;
         }
         token = lex_get_token(lc, T_SKIP_EOL);
      } while (token == T_IDENTIFIER);
      if (token != T_EOB) {
         scan_err1(lc, _("Expected a end of block }, got: %s"), lc->str);
      }
      if (pass == 1 && !add_address(item->dlistvalue, IPADDR::R_MULTIPLE,
                                    htons(port), family, hostname_str, port_str,
                                    errmsg, sizeof(errmsg))) {
           scan_err3(lc, _("Can't add hostname(%s) and port(%s) to addrlist (%s)"),
                   hostname_str, port_str, errmsg);
      }
      token = scan_to_next_not_eol(lc);
   } while ((token == T_IDENTIFIER || token == T_UNQUOTED_STRING));
   if (token != T_EOB) {
      scan_err1(lc, _("Expected a end of block }, got: %s"), lc->str);
   }
}

static void store_addresses_address(LEX * lc, RES_ITEM * item, int index, int pass)
{
   int token;
   char errmsg[1024];
   int port = str_to_int32(item->default_value);

   token = lex_get_token(lc, T_SKIP_EOL);
   if (!(token == T_UNQUOTED_STRING || token == T_NUMBER || token == T_IDENTIFIER)) {
      scan_err1(lc, _("Expected an IP number or a hostname, got: %s"), lc->str);
   }
   if (pass == 1 && !add_address(item->dlistvalue, IPADDR::R_SINGLE_ADDR,
                    htons(port), AF_INET, lc->str, 0,
                    errmsg, sizeof(errmsg))) {
      scan_err2(lc, _("can't add port (%s) to (%s)"), lc->str, errmsg);
   }
}

static void store_addresses_port(LEX * lc, RES_ITEM * item, int index, int pass)
{
   int token;
   char errmsg[1024];
   int port = str_to_int32(item->default_value);

   token = lex_get_token(lc, T_SKIP_EOL);
   if (!(token == T_UNQUOTED_STRING || token == T_NUMBER || token == T_IDENTIFIER)) {
      scan_err1(lc, _("Expected a port number or string, got: %s"), lc->str);
   }
   if (pass == 1 && !add_address(item->dlistvalue, IPADDR::R_SINGLE_PORT,
                    htons(port), AF_INET, 0, lc->str,
                    errmsg, sizeof(errmsg))) {
      scan_err2(lc, _("can't add port (%s) to (%s)"), lc->str, errmsg);
   }
}

/*
 * Generic store resource dispatcher.
 */
bool store_resource(int type, LEX *lc, RES_ITEM *item, int index, int pass)
{
   switch (type) {
   case CFG_TYPE_STR:
      store_str(lc, item, index, pass);
      break;
   case CFG_TYPE_DIR:
      store_dir(lc, item, index, pass);
      break;
   case CFG_TYPE_MD5PASSWORD:
      store_md5password(lc, item, index, pass);
      break;
   case CFG_TYPE_CLEARPASSWORD:
      store_clearpassword(lc, item, index, pass);
      break;
   case CFG_TYPE_NAME:
      store_name(lc, item, index, pass);
      break;
   case CFG_TYPE_STRNAME:
      store_strname(lc, item, index, pass);
      break;
   case CFG_TYPE_RES:
      store_res(lc, item, index, pass);
      break;
   case CFG_TYPE_ALIST_RES:
      store_alist_res(lc, item, index, pass);
      break;
   case CFG_TYPE_ALIST_STR:
      store_alist_str(lc, item, index, pass);
      break;
   case CFG_TYPE_ALIST_DIR:
      store_alist_dir(lc, item, index, pass);
      break;
   case CFG_TYPE_INT16:
      store_int16(lc, item, index, pass);
      break;
   case CFG_TYPE_PINT16:
      store_pint16(lc, item, index, pass);
      break;
   case CFG_TYPE_INT32:
      store_int32(lc, item, index, pass);
      break;
   case CFG_TYPE_PINT32:
      store_pint32(lc, item, index, pass);
      break;
   case CFG_TYPE_MSGS:
      store_msgs(lc, item, index, pass);
      break;
   case CFG_TYPE_INT64:
      store_int64(lc, item, index, pass);
      break;
   case CFG_TYPE_BIT:
      store_bit(lc, item, index, pass);
      break;
   case CFG_TYPE_BOOL:
      store_bool(lc, item, index, pass);
      break;
   case CFG_TYPE_TIME:
      store_time(lc, item, index, pass);
      break;
   case CFG_TYPE_SIZE64:
      store_size64(lc, item, index, pass);
      break;
   case CFG_TYPE_SIZE32:
      store_size32(lc, item, index, pass);
      break;
   case CFG_TYPE_SPEED:
      store_speed(lc, item, index, pass);
      break;
   case CFG_TYPE_DEFS:
      store_defs(lc, item, index, pass);
      break;
   case CFG_TYPE_LABEL:
      store_label(lc, item, index, pass);
      break;
   case CFG_TYPE_ADDRESSES:
      store_addresses(lc, item, index, pass);
      break;
   case CFG_TYPE_ADDRESSES_ADDRESS:
      store_addresses_address(lc, item, index, pass);
      break;
   case CFG_TYPE_ADDRESSES_PORT:
      store_addresses_port(lc, item, index, pass);
      break;
   case CFG_TYPE_PLUGIN_NAMES:
      store_plugin_names(lc, item, index, pass);
      break;
   default:
      return false;
   }

   return true;
}

void indent_config_item(POOL_MEM &cfg_str, int level, const char *config_item, bool inherited)
{
   for (int i = 0; i < level; i++) {
      pm_strcat(cfg_str, DEFAULT_INDENT_STRING);
   }
   if (inherited) {
      pm_strcat(cfg_str, "#");
      pm_strcat(cfg_str, DEFAULT_INDENT_STRING);
   }
   pm_strcat(cfg_str, config_item);
}

static inline void print_config_size(RES_ITEM *item, POOL_MEM &cfg_str, bool inherited)
{
   POOL_MEM temp;
   POOL_MEM volspec;   /* vol specification string*/
   int64_t bytes = *(item->ui64value);
   int factor;

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

   if (bytes == 0) {
      pm_strcat(volspec, "0");
   } else {
      for (int t=0; modifier[t]; t++) {
         Dmsg2(200, " %s bytes: %lld\n", item->name, bytes);
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
   indent_config_item(cfg_str, 1, temp.c_str(), inherited);
}

static inline void print_config_time(RES_ITEM *item, POOL_MEM &cfg_str, bool inherited)
{
   POOL_MEM temp;
   POOL_MEM timespec;
   utime_t secs = *(item->utimevalue);
   int factor;

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
   indent_config_item(cfg_str, 1, temp.c_str(), inherited);
}

bool MSGSRES::print_config(POOL_MEM &buff, bool hide_sensitive_data, bool verbose)
{
   POOL_MEM cfg_str;    /* configuration as string  */
   POOL_MEM temp;
   MSGSRES *msgres;
   DEST *d;

   msgres = this;

   pm_strcat(cfg_str, "Messages {\n");
   Mmsg(temp, "   %s = \"%s\"\n", "Name", msgres->name());
   pm_strcat(cfg_str, temp.c_str());

   if (msgres->mail_cmd) {
      POOL_MEM esc;

      escape_string(esc, msgres->mail_cmd, strlen(msgres->mail_cmd));
      Mmsg(temp, "   MailCommand = \"%s\"\n", esc.c_str());
      pm_strcat(cfg_str, temp.c_str());
   }

   if (msgres->operator_cmd) {
      POOL_MEM esc;

      escape_string(esc, msgres->operator_cmd, strlen(msgres->operator_cmd));
      Mmsg(temp, "   OperatorCommand = \"%s\"\n", esc.c_str());
      pm_strcat(cfg_str, temp.c_str());
   }

   if (msgres->timestamp_format) {
      POOL_MEM esc;

      escape_string(esc, msgres->timestamp_format, strlen(msgres->timestamp_format));
      Mmsg(temp, "   TimestampFormat = \"%s\"\n", esc.c_str());
      pm_strcat(cfg_str, temp.c_str());
   }

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
            Mmsg(temp, ",%s" , msg_types[j].name);
            pm_strcat(t, temp.c_str());
         } else {
            Mmsg(temp, ",!%s" , msg_types[j].name);
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

static inline bool has_default_value(RES_ITEM *item)
{
   bool is_default = false;

   if (item->flags & CFG_ITEM_DEFAULT) {
            /*
             * Check for default values.
             */
            switch (item->type) {
            case CFG_TYPE_STR:
            case CFG_TYPE_DIR:
            case CFG_TYPE_NAME:
            case CFG_TYPE_STRNAME:
               is_default = bstrcmp(*(item->value), item->default_value);
               break;
            case CFG_TYPE_INT16:
               is_default = (*(item->i16value) == (int16_t)str_to_int32(item->default_value));
               break;
            case CFG_TYPE_PINT16:
               is_default = (*(item->ui16value) == (uint16_t)str_to_int32(item->default_value));
               break;
            case CFG_TYPE_INT32:
               is_default = (*(item->i32value) == str_to_int32(item->default_value));
               break;
            case CFG_TYPE_PINT32:
               is_default = (*(item->ui32value) == (uint32_t)str_to_int32(item->default_value));
               break;
            case CFG_TYPE_INT64:
               is_default = (*(item->i64value) == str_to_int64(item->default_value));
               break;
            case CFG_TYPE_SPEED:
               is_default = (*(item->ui64value) == (uint64_t)str_to_int64(item->default_value));
               break;
            case CFG_TYPE_SIZE64:
               is_default = (*(item->ui64value) == (uint64_t)str_to_int64(item->default_value));
               break;
            case CFG_TYPE_SIZE32:
               is_default = (*(item->ui32value) != (uint32_t)str_to_int32(item->default_value));
               break;
            case CFG_TYPE_TIME:
               is_default = (*(item->ui64value) == (uint64_t)str_to_int64(item->default_value));
               break;
            case CFG_TYPE_BOOL: {
               bool default_value = bstrcasecmp(item->default_value, "true") ||
                                    bstrcasecmp(item->default_value, "yes");

               is_default = (*item->boolvalue == default_value);
               break;
            }
            default:
               break;
            }
   } else {
            switch (item->type) {
            case CFG_TYPE_STR:
            case CFG_TYPE_DIR:
            case CFG_TYPE_NAME:
            case CFG_TYPE_STRNAME:
               is_default = (*(item->value) == NULL);
               break;
            case CFG_TYPE_INT16:
               is_default = (*(item->i16value) == 0);
               break;
            case CFG_TYPE_PINT16:
               is_default = (*(item->ui16value) == 0);
               break;
            case CFG_TYPE_INT32:
               is_default = (*(item->i32value) == 0);
               break;
            case CFG_TYPE_PINT32:
               is_default = (*(item->ui32value) == 0);
               break;
            case CFG_TYPE_INT64:
               is_default = (*(item->i64value) == 0);
               break;
            case CFG_TYPE_SPEED:
               is_default = (*(item->ui64value) == 0);
               break;
            case CFG_TYPE_SIZE64:
               is_default = (*(item->ui64value) == 0);
               break;
            case CFG_TYPE_SIZE32:
               is_default = (*(item->ui32value) == 0);
               break;
            case CFG_TYPE_TIME:
               is_default = (*(item->ui64value) == 0);
               break;
            case CFG_TYPE_BOOL:
               is_default = (*item->boolvalue == false);
               break;
            default:
               break;
            }
   }

   return is_default;
}

bool BRSRES::print_config(POOL_MEM &buff, bool hide_sensitive_data, bool verbose)
{
   POOL_MEM cfg_str;
   POOL_MEM temp;
   RES_ITEM *items;
   int i = 0;
   int rindex;
   bool inherited = false;

   /*
    * If entry is not used, then there is nothing to print.
    */
   if (this->hdr.rcode < (uint32_t)my_config->m_r_first ||
       this->hdr.refcnt <= 0) {
      return true;
   }
   rindex = this->hdr.rcode - my_config->m_r_first;

   /*
    * Make sure the resource class has any items.
    */
   if (!my_config->m_resources[rindex].items) {
      return true;
   }

   memcpy(my_config->m_res_all, this, my_config->m_resources[rindex].size);

   pm_strcat(cfg_str, res_to_str(this->hdr.rcode));
   pm_strcat(cfg_str, " {\n");

   items = my_config->m_resources[rindex].items;

   for (i = 0; items[i].name; i++) {
      bool print_item = false;
      inherited = bit_is_set(i, this->hdr.inherit_content);

      /*
       * If this is an alias for another config keyword suppress it.
       */
      if ((items[i].flags & CFG_ITEM_ALIAS)) {
         continue;
      }

      if ((!verbose) && inherited) {
         /*
          * If not in verbose mode, skip inherited directives.
          */
         continue;
      }

      if ((items[i].flags & CFG_ITEM_REQUIRED) || !my_config->m_omit_defaults) {
         /*
          * Always print required items or if my_config->m_omit_defaults is false
          */
         print_item = true;
      }

      if (!has_default_value(&items[i])) {
            print_item = true;
      } else {
         if ((items[i].flags & CFG_ITEM_DEFAULT) && verbose) {
            /*
             * If value has a expliciet default value and verbose mode is on,
             * display directive as inherited.
             */
            print_item = true;
            inherited = true;
         }
      }

      switch (items[i].type) {
      case CFG_TYPE_STR:
      case CFG_TYPE_DIR:
      case CFG_TYPE_NAME:
      case CFG_TYPE_STRNAME:
         if (print_item && *(items[i].value) != NULL) {
            Dmsg2(200, "%s = \"%s\"\n", items[i].name, *(items[i].value));
            Mmsg(temp, "%s = \"%s\"\n", items[i].name, *(items[i].value));
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         }
         break;
      case CFG_TYPE_MD5PASSWORD:
      case CFG_TYPE_CLEARPASSWORD:
      case CFG_TYPE_AUTOPASSWORD:
         if (print_item) {
            s_password *password;

            password = items[i].pwdvalue;
            if (password && password->value != NULL) {
               if (hide_sensitive_data) {
                  Dmsg1(200, "%s = \"****************\"\n", items[i].name);
                  Mmsg(temp, "%s = \"****************\"\n", items[i].name);
               } else {
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
               }
               indent_config_item(cfg_str, 1, temp.c_str(), inherited);
            }
         }
         break;
      case CFG_TYPE_LABEL:
         for (int j = 0; tapelabels[j].name; j++) {
            if (*(items[i].ui32value) == tapelabels[j].token) {
               /*
                * Supress printing default value.
                */
               if (items[i].flags & CFG_ITEM_DEFAULT) {
                  if (bstrcasecmp(items[i].default_value, tapelabels[j].name)) {
                     break;
                  }
               }

               Mmsg(temp, "%s = \"%s\"\n", items[i].name, tapelabels[j].name);
               indent_config_item(cfg_str, 1, temp.c_str(), inherited);
               break;
            }
         }
         break;
      case CFG_TYPE_INT16:
         if (print_item) {
            Mmsg(temp, "%s = %hd\n", items[i].name, *(items[i].i16value));
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         }
         break;
      case CFG_TYPE_PINT16:
         if (print_item) {
            Mmsg(temp, "%s = %hu\n", items[i].name, *(items[i].ui16value));
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         }
         break;
      case CFG_TYPE_INT32:
         if (print_item) {
            Mmsg(temp, "%s = %d\n", items[i].name, *(items[i].i32value));
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         }
         break;
      case CFG_TYPE_PINT32:
         if (print_item) {
            Mmsg(temp, "%s = %u\n", items[i].name, *(items[i].ui32value));
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         }
         break;
      case CFG_TYPE_INT64:
         if (print_item) {
            Mmsg(temp, "%s = %d\n", items[i].name, *(items[i].i64value));
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         }
         break;
      case CFG_TYPE_SPEED:
         if (print_item) {
            Mmsg(temp, "%s = %u\n", items[i].name, *(items[i].ui64value));
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         }
         break;
      case CFG_TYPE_SIZE64:
      case CFG_TYPE_SIZE32:
         if (print_item) {
            print_config_size(&items[i], cfg_str, inherited);
         }
         break;
      case CFG_TYPE_TIME:
         if (print_item) {
            print_config_time(&items[i], cfg_str, inherited);
         }
         break;
      case CFG_TYPE_BOOL:
         if (print_item) {
            if (*items[i].boolvalue) {
               Mmsg(temp, "%s = %s\n", items[i].name, NT_("yes"));
            } else {
               Mmsg(temp, "%s = %s\n", items[i].name, NT_("no"));
            }
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         }
         break;
      case CFG_TYPE_ALIST_STR:
      case CFG_TYPE_ALIST_DIR:
      case CFG_TYPE_PLUGIN_NAMES: {
        /*
         * One line for each member of the list
         */
         char *value;
         alist *list;
         list = *(items[i].alistvalue);

         if (list != NULL) {
            foreach_alist(value, list) {
               /*
                * If this is the default value skip it.
                */
               if (items[i].flags & CFG_ITEM_DEFAULT) {
                  if (bstrcmp(value, items[i].default_value)) {
                     continue;
                  }
               }
               Mmsg(temp, "%s = \"%s\"\n", items[i].name, value);
               indent_config_item(cfg_str, 1, temp.c_str(), inherited);
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
         alist *list;
         POOL_MEM res_names;

         list = *(items[i].alistvalue);
         if (list != NULL) {
            Mmsg(temp, "%s = ", items[i].name);
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);

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

         res = *(items[i].resvalue);
         if (res != NULL && res->name != NULL) {
            Mmsg(temp, "%s = \"%s\"\n", items[i].name, res->name);
            indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         }
         break;
      }
      case CFG_TYPE_BIT:
         if (bit_is_set(items[i].code, items[i].bitvalue)) {
            Mmsg(temp, "%s = %s\n", items[i].name, NT_("yes"));
         } else {
            Mmsg(temp, "%s = %s\n", items[i].name, NT_("no"));
         }
         indent_config_item(cfg_str, 1, temp.c_str(), inherited);
         break;
      case CFG_TYPE_MSGS:
         /*
          * We ignore these items as they are printed in a special way in MSGSRES::print_config()
          */
         break;
      case CFG_TYPE_ADDRESSES: {
         dlist *addrs = *items[i].dlistvalue;
         IPADDR *adr;

         Mmsg(temp, "%s = {\n", items[i].name);
         indent_config_item(cfg_str, 1, temp.c_str(), inherited);
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
         /*
          * Is stored in CFG_TYPE_ADDRESSES and printed there.
          */
         break;
      case CFG_TYPE_ADDRESSES_ADDRESS:
         /*
          * Is stored in CFG_TYPE_ADDRESSES and printed there.
          */
         break;
      default:
         /*
          * This is a non-generic type call back to the daemon to get things printed.
          */
         if (my_config->m_print_res) {
            my_config->m_print_res(items, i, cfg_str, hide_sensitive_data, inherited);
         }
         break;
      }
   }

   pm_strcat(cfg_str, "}\n\n");
   pm_strcat(buff, cfg_str.c_str());

   return true;
}

#ifdef HAVE_JANSSON
/*
 * resource item schema description in JSON format.
 * Example output:
 *
 *   "filesetacl": {
 *     "datatype": "BOOLEAN",
 *     "datatype_number": 51,
 *     "code": int
 *     [ "defaultvalue": "xyz", ]
 *     [ "required": true, ]
 *     [ "alias": true, ]
 *     [ "deprecated": true, ]
 *     [ "equals": true, ]
 *     ...
 *     "type": "RES_ITEM"
 *   }
 */
json_t *json_item(RES_ITEM *item)
{
   json_t *json = json_object();

   json_object_set_new(json, "datatype", json_string(datatype_to_str(item->type)));
   json_object_set_new(json, "code", json_integer(item->code));

   if (item->flags & CFG_ITEM_ALIAS) {
      json_object_set_new(json, "alias", json_true());
   }
   if (item->flags & CFG_ITEM_DEFAULT) {
      /* FIXME? would it be better to convert it to the right type before returning? */
      json_object_set_new(json, "default_value", json_string(item->default_value));
   }
   if (item->flags & CFG_ITEM_PLATFORM_SPECIFIC) {
      json_object_set_new(json, "platform_specific", json_true());
   }
   if (item->flags & CFG_ITEM_DEPRECATED) {
      json_object_set_new(json, "deprecated", json_true());
   }
   if (item->flags & CFG_ITEM_NO_EQUALS) {
      json_object_set_new(json, "equals", json_false());
   } else {
      json_object_set_new(json, "equals", json_true());
   }
   if (item->flags & CFG_ITEM_REQUIRED) {
      json_object_set_new(json, "required", json_true());
   }
   if (item->versions) {
      json_object_set_new(json, "versions", json_string(item->versions));
   }
   if (item->description) {
      json_object_set_new(json, "description", json_string(item->description));
   }

   return json;
}

json_t *json_item(s_kw *item)
{
   json_t *json = json_object();

   json_object_set_new(json, "token", json_integer(item->token));

   return json;
}

json_t *json_items(RES_ITEM items[])
{
   json_t *json = json_object();

   if (items) {
      for (int i = 0; items[i].name; i++) {
         json_object_set_new(json, items[i].name, json_item(&items[i]));
      }
   }

   return json;
}
#endif

static DATATYPE_NAME datatype_names[] = {
   /*
    * Standard resource types. handlers in res.c
    */
   { CFG_TYPE_STR, "STRING", "String" },
   { CFG_TYPE_DIR, "DIRECTORY", "directory" },
   { CFG_TYPE_MD5PASSWORD, "MD5PASSWORD", "Password in MD5 format" },
   { CFG_TYPE_CLEARPASSWORD, "CLEARPASSWORD", "Password as cleartext" },
   { CFG_TYPE_AUTOPASSWORD, "AUTOPASSWORD", "Password stored in clear when needed otherwise hashed" },
   { CFG_TYPE_NAME, "NAME", "Name" },
   { CFG_TYPE_STRNAME, "STRNAME", "String name" },
   { CFG_TYPE_RES, "RES", "Resource" },
   { CFG_TYPE_ALIST_RES, "RESOURCE_LIST", "Resource list" },
   { CFG_TYPE_ALIST_STR, "STRING_LIST", "string list" },
   { CFG_TYPE_ALIST_DIR, "DIRECTORY_LIST", "directory list" },
   { CFG_TYPE_INT16, "INT16", "Integer 16 bits" },
   { CFG_TYPE_PINT16, "PINT16", "Positive 16 bits Integer (unsigned)" },
   { CFG_TYPE_INT32, "INT32", "Integer 32 bits" },
   { CFG_TYPE_PINT32, "PINT32", "Positive 32 bits Integer (unsigned)" },
   { CFG_TYPE_MSGS, "MESSAGES", "Message resource" },
   { CFG_TYPE_INT64, "INT64", "Integer 64 bits" },
   { CFG_TYPE_BIT, "BIT", "Bitfield" },
   { CFG_TYPE_BOOL, "BOOLEAN", "boolean" },
   { CFG_TYPE_TIME, "TIME", "time" },
   { CFG_TYPE_SIZE64, "SIZE64", "64 bits file size" },
   { CFG_TYPE_SIZE32, "SIZE32", "32 bits file size" },
   { CFG_TYPE_SPEED, "SPEED", "speed" },
   { CFG_TYPE_DEFS, "DEFS", "definition" },
   { CFG_TYPE_LABEL, "LABEL", "label" },
   { CFG_TYPE_ADDRESSES, "ADDRESSES", "ip addresses list" },
   { CFG_TYPE_ADDRESSES_ADDRESS, "ADDRESS", "ip address" },
   { CFG_TYPE_ADDRESSES_PORT, "PORT", "network port" },
   { CFG_TYPE_PLUGIN_NAMES, "PLUGIN_NAMES", "Plugin Name(s)" },

   /*
    * Director resource types. handlers in dird_conf.
    */
   { CFG_TYPE_ACL, "ACL", "User Access Control List" },
   { CFG_TYPE_AUDIT, "AUDIT_COMMAND_LIST", "Auditing Command List" },
   { CFG_TYPE_AUTHPROTOCOLTYPE, "AUTH_PROTOCOL_TYPE", "Authentication Protocol" },
   { CFG_TYPE_AUTHTYPE, "AUTH_TYPE", "Authentication Type" },
   { CFG_TYPE_DEVICE, "DEVICE", "Device resource" },
   { CFG_TYPE_JOBTYPE, "JOB_TYPE", "Type of Job" },
   { CFG_TYPE_PROTOCOLTYPE, "PROTOCOL_TYPE", "Protocol" },
   { CFG_TYPE_LEVEL, "BACKUP_LEVEL", "Backup Level" },
   { CFG_TYPE_REPLACE, "REPLACE_OPTION", "Replace option" },
   { CFG_TYPE_SHRTRUNSCRIPT, "RUNSCRIPT_SHORT", "Short Runscript definition" },
   { CFG_TYPE_RUNSCRIPT, "RUNSCRIPT", "Runscript" },
   { CFG_TYPE_RUNSCRIPT_CMD, "RUNSCRIPT_COMMAND", "Runscript Command" },
   { CFG_TYPE_RUNSCRIPT_TARGET, "RUNSCRIPT_TARGET", "Runscript Target (Host)" },
   { CFG_TYPE_RUNSCRIPT_BOOL, "RUNSCRIPT_BOOLEAN", "Runscript Boolean" },
   { CFG_TYPE_RUNSCRIPT_WHEN, "RUNSCRIPT_WHEN", "Runscript When expression" },
   { CFG_TYPE_MIGTYPE, "MIGRATION_TYPE", "Migration Type" },
   { CFG_TYPE_INCEXC, "INCLUDE_EXCLUDE_ITEM", "Include/Exclude item" },
   { CFG_TYPE_RUN, "SCHEDULE_RUN_COMMAND", "Schedule Run Command" },
   { CFG_TYPE_ACTIONONPURGE, "ACTION_ON_PURGE", "Action to perform on Purge" },
   { CFG_TYPE_POOLTYPE, "POOLTYPE", "Pool Type" },

   /*
    * Director fileset options. handlers in dird_conf.
    */
   { CFG_TYPE_FNAME, "FILENAME", "Filename" },
   { CFG_TYPE_PLUGINNAME, "PLUGIN_NAME", "Pluginname" },
   { CFG_TYPE_EXCLUDEDIR, "EXCLUDE_DIRECTORY", "Exclude directory" },
   { CFG_TYPE_OPTIONS, "OPTIONS", "Options block" },
   { CFG_TYPE_OPTION, "OPTION", "Option of Options block" },
   { CFG_TYPE_REGEX, "REGEX", "Regular Expression" },
   { CFG_TYPE_BASE, "BASEJOB", "Basejob Expression" },
   { CFG_TYPE_WILD, "WILDCARD", "Wildcard Expression" },
   { CFG_TYPE_PLUGIN, "PLUGIN", "Plugin definition" },
   { CFG_TYPE_FSTYPE, "FILESYSTEM_TYPE", "FileSytem match criterium (UNIX)" },
   { CFG_TYPE_DRIVETYPE, "DRIVE_TYPE", "DriveType match criterium (Windows)" },
   { CFG_TYPE_META, "META_TAG", "Meta tag" },

   /*
    * Storage daemon resource types
    */
   { CFG_TYPE_DEVTYPE, "DEVICE_TYPE", "Device Type" },
   { CFG_TYPE_MAXBLOCKSIZE, "MAX_BLOCKSIZE", "Maximum Blocksize" },
   { CFG_TYPE_IODIRECTION, "IO_DIRECTION", "IO Direction" },
   { CFG_TYPE_CMPRSALGO, "COMPRESSION_ALGORITHM", "Compression Algorithm" },

   /*
    * File daemon resource types
    */
   { CFG_TYPE_CIPHER, "ENCRYPTION_CIPHER", "Encryption Cipher" },

   { 0, NULL, NULL }
};

DATATYPE_NAME *get_datatype(int number)
{
   int size = sizeof(datatype_names) / sizeof(datatype_names[0]);

   if (number >= size) {
      /*
       * Last entry of array is a dummy entry
       */
      number=size - 1;
   }

   return &(datatype_names[number]);
}

const char *datatype_to_str(int type)
{
   for (int i = 0; datatype_names[i].name; i++) {
      if (datatype_names[i].number == type) {
         return datatype_names[i].name;
      }
   }

   return "unknown";
}

const char *datatype_to_description(int type)
{
   for (int i = 0; datatype_names[i].name; i++) {
      if (datatype_names[i].number == type) {
         return datatype_names[i].description;
      }
   }

   return NULL;
}
