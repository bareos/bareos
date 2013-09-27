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
 * This file handles everything related to resources.
 *
 * Kern Sibbald, January 2000
 * Split from parse_conf.c April 2005
 * More stuff moved from parse_conf.c February 2013
 */

#include "bareos.h"
#include "generic_res.h"

static int res_locked = 0;            /* resource chain lock count -- for debug */

/*
 * Each daemon has a slightly different set of resources, so it will define the
 * following global values.
 */

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

/* Forward referenced subroutines */
static void scan_types(LEX *lc, MSGSRES *msg, int dest, char *where, char *cmd);

/* Common Resource definitions */

/* #define TRACE_RES */

void CONFIG::b_LockRes(const char *file, int line)
{
   int errstat;
#ifdef TRACE_RES
   Pmsg4(000, "LockRes  locked=%d w_active=%d at %s:%d\n",
         res_locked, m_res_lock.w_active, file, line);
    if (res_locked) {
       Pmsg2(000, "LockRes writerid=%d myid=%d\n",
             m_res_lock.writer_id, pthread_self());
     }
#endif
   if ((errstat=rwl_writelock(&m_res_lock)) != 0) {
      Emsg3(M_ABORT, 0, _("rwl_writelock failure at %s:%d:  ERR=%s\n"),
           file, line, strerror(errstat));
   }
   res_locked++;
}

void CONFIG::b_UnlockRes(const char *file, int line)
{
   int errstat;
   if ((errstat=rwl_writeunlock(&m_res_lock)) != 0) {
      Emsg3(M_ABORT, 0, _("rwl_writeunlock failure at %s:%d:. ERR=%s\n"),
           file, line, strerror(errstat));
   }
   res_locked--;
#ifdef TRACE_RES
   Pmsg4(000, "UnLockRes locked=%d wactive=%d at %s:%d\n",
         res_locked, m_res_lock.w_active, file, line);
#endif
}

/*
 * Compare two resource names
 */
static int res_compare(void *item1, void *item2)
{
   RES *res1 = (RES *)item1;
   RES *res2 = (RES *)item2;

   return strcmp(res1->name, res2->name);
}

/*
 * Return resource of type rcode that matches name
 */
RES *CONFIG::GetResWithName(int rcode, const char *name)
{
   RES_CONTAINER *res_container;
   int rindex = rcode - m_r_first;
   RES item, *res;

   LockRes(this);
   res_container = m_res_containers[rindex];
   item.name = (char *)name;
   res = (RES *)res_container->list->search(&item, res_compare);
   UnlockRes(this);

   return res;
}

/*
 * Return next resource of type rcode. On first
 * call second arg (res) is NULL, on subsequent
 * calls, it is called with previous value.
 */
RES *CONFIG::GetNextRes(int rcode, RES *res)
{
   RES *nres;
   int rindex = rcode - m_r_first;

   if (res == NULL) {
      nres = (RES *)m_res_containers[rindex]->head;
   } else {
      nres = res->res_next;
   }
   return nres;
}

/*
 * Message resource directives
 * name config_data_type value code flags default_value
 */
RES_ITEM msgs_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_msgs.hdr.name), 0, 0, NULL },
   { "description", CFG_TYPE_STR, ITEM(res_msgs.hdr.desc), 0, 0, NULL },
   { "mailcommand", CFG_TYPE_STR, ITEM(res_msgs.mail_cmd), 0, 0, NULL },
   { "operatorcommand", CFG_TYPE_STR, ITEM(res_msgs.operator_cmd), 0, 0, NULL },
   { "syslog", CFG_TYPE_MSGS, ITEM(res_msgs), MD_SYSLOG, 0, NULL },
   { "mail", CFG_TYPE_MSGS, ITEM(res_msgs), MD_MAIL, 0, NULL },
   { "mailonerror", CFG_TYPE_MSGS, ITEM(res_msgs), MD_MAIL_ON_ERROR, 0, NULL },
   { "mailonsuccess", CFG_TYPE_MSGS, ITEM(res_msgs), MD_MAIL_ON_SUCCESS, 0, NULL },
   { "file", CFG_TYPE_MSGS, ITEM(res_msgs), MD_FILE, 0, NULL },
   { "append", CFG_TYPE_MSGS, ITEM(res_msgs), MD_APPEND, 0, NULL },
   { "stdout", CFG_TYPE_MSGS, ITEM(res_msgs), MD_STDOUT, 0, NULL },
   { "stderr", CFG_TYPE_MSGS, ITEM(res_msgs), MD_STDERR, 0, NULL },
   { "director", CFG_TYPE_MSGS, ITEM(res_msgs), MD_DIRECTOR, 0, NULL },
   { "console", CFG_TYPE_MSGS, ITEM(res_msgs), MD_CONSOLE, 0, NULL },
   { "operator", CFG_TYPE_MSGS, ITEM(res_msgs), MD_OPERATOR, 0, NULL },
   { "catalog", CFG_TYPE_MSGS, ITEM(res_msgs), MD_CATALOG, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Store Messages Destination information
 */
static void store_msgs(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;
   char *cmd;
   POOLMEM *dest;
   int dest_len;

   Dmsg2(900, "store_msgs pass=%d code=%d\n", pass, item->code);
   if (pass == 1) {
      switch (item->code) {
      case MD_STDOUT:
      case MD_STDERR:
      case MD_SYSLOG:              /* syslog */
      case MD_CONSOLE:
      case MD_CATALOG:
         scan_types(lc, (MSGSRES *)(item->value), item->code, NULL, NULL);
         break;
      case MD_OPERATOR:            /* send to operator */
      case MD_DIRECTOR:            /* send to Director */
      case MD_MAIL:                /* mail */
      case MD_MAIL_ON_ERROR:       /* mail if Job errors */
      case MD_MAIL_ON_SUCCESS:     /* mail if Job succeeds */
         if (item->code == MD_OPERATOR) {
            cmd = res_all.res_msgs.operator_cmd;
         } else {
            cmd = res_all.res_msgs.mail_cmd;
         }
         dest = get_pool_memory(PM_MESSAGE);
         dest[0] = 0;
         dest_len = 0;
         /*
          * Pick up comma separated list of destinations
          */
         for ( ;; ) {
            token = lex_get_token(lc, T_NAME);   /* scan destination */
            dest = check_pool_memory_size(dest, dest_len + lc->str_len + 2);
            if (dest[0] != 0) {
               pm_strcat(dest, " ");  /* separate multiple destinations with space */
               dest_len++;
            }
            pm_strcat(dest, lc->str);
            dest_len += lc->str_len;
            Dmsg2(900, "store_msgs newdest=%s: dest=%s:\n", lc->str, NPRT(dest));
            token = lex_get_token(lc, T_SKIP_EOL);
            if (token == T_COMMA) {
               continue;           /* get another destination */
            }
            if (token != T_EQUALS) {
               scan_err1(lc, _("expected an =, got: %s"), lc->str);
               return;
            }
            break;
         }
         Dmsg1(900, "mail_cmd=%s\n", NPRT(cmd));
         scan_types(lc, (MSGSRES *)(item->value), item->code, dest, cmd);
         free_pool_memory(dest);
         Dmsg0(900, "done with dest codes\n");
         break;
      case MD_FILE:                /* file */
      case MD_APPEND:              /* append */
         dest = get_pool_memory(PM_MESSAGE);
         /*
          * Pick up a single destination
          */
         token = lex_get_token(lc, T_NAME);   /* scan destination */
         pm_strcpy(dest, lc->str);
         dest_len = lc->str_len;
         token = lex_get_token(lc, T_SKIP_EOL);
         Dmsg1(900, "store_msgs dest=%s:\n", NPRT(dest));
         if (token != T_EQUALS) {
            scan_err1(lc, _("expected an =, got: %s"), lc->str);
            return;
         }
         scan_types(lc, (MSGSRES *)(item->value), item->code, dest, NULL);
         free_pool_memory(dest);
         Dmsg0(900, "done with dest codes\n");
         break;
      default:
         scan_err1(lc, _("Unknown item code: %d\n"), item->code);
         return;
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   Dmsg0(900, "Done store_msgs\n");
}

/*
 * Scan for message types and add them to the message
 * destination. The basic job here is to connect message types
 *  (WARNING, ERROR, FATAL, INFO, ...) with an appropriate
 *  destination (MAIL, FILE, OPERATOR, ...)
 */
static void scan_types(LEX *lc, MSGSRES *msg, int dest_code, char *where, char *cmd)
{
   int i;
   bool found, is_not;
   int msg_type = 0;
   char *str;

   for ( ;; ) {
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

      if (msg_type == M_MAX+1) {         /* all? */
         for (i = 1; i <= M_MAX; i++) {      /* yes set all types */
            add_msg_dest(msg, dest_code, i, where, cmd);
         }
      } else if (is_not) {
         rem_msg_dest(msg, dest_code, msg_type, where);
      } else {
         add_msg_dest(msg, dest_code, msg_type, where, cmd);
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
 * This routine is ONLY for resource names
 *  Store a name at specified address.
 */
static void store_name(LEX *lc, RES_ITEM *item, int index, int pass)
{
   POOLMEM *msg = get_pool_memory(PM_EMSG);
   lex_get_token(lc, T_NAME);
   if (!is_name_valid(lc->str, &msg)) {
      scan_err1(lc, "%s\n", msg);
      return;
   }
   free_pool_memory(msg);
   /*
    * Store the name both pass 1 and pass 2
    */
   if (*(item->value)) {
      scan_err2(lc, _("Attempt to redefine name \"%s\" to \"%s\"."),
         *(item->value), lc->str);
      return;
   }
   *(item->value) = bstrdup(lc->str);
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a name string at specified address
 * A name string is limited to MAX_RES_NAME_LENGTH
 */
static void store_strname(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);
   /*
    * Store the name
    */
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
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a string at specified address
 */
static void store_str(LEX *lc, RES_ITEM *item, int index, int pass)
{
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
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a directory name at specified address. Note, we do
 *   shell expansion except if the string begins with a vertical
 *   bar (i.e. it will likely be passed to the shell later).
 */
static void store_dir(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_STRING);
   if (pass == 1) {
      /*
       * If a default was set free it first.
       */
      if (*(item->value)) {
         free(*(item->value));
      }
      if (lc->str[0] != '|') {
         do_shell_expansion(lc->str, sizeof(lc->str));
      }
      *(item->value) = bstrdup(lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a password at specified address in MD5 coding
 */
static void store_md5password(LEX *lc, RES_ITEM *item, int index, int pass)
{
   s_password *pwd;

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
         struct MD5Context md5c;
         unsigned char digest[CRYPTO_DIGEST_MD5_SIZE];
         char sig[100];

         MD5Init(&md5c);
         MD5Update(&md5c, (unsigned char *) (lc->str), lc->str_len);
         MD5Final(digest, &md5c);
         for (i = j = 0; i < sizeof(digest); i++) {
            sprintf(&sig[j], "%02x", digest[i]);
            j += 2;
         }
         pwd->encoding = p_encoding_md5;
         pwd->value = bstrdup(sig);
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a password at specified address in MD5 coding
 */
static void store_clearpassword(LEX *lc, RES_ITEM *item, int index, int pass)
{
   s_password *pwd;

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
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a resource at specified address.
 * If we are in pass 2, do a lookup of the
 * resource.
 */
static void store_res(CONFIG *config, LEX *lc, RES_ITEM *item, int index, int pass)
{
   RES *res;

   lex_get_token(lc, T_NAME);
   if (pass == 2) {
      res = config->GetResWithName(item->code, lc->str);
      if (res == NULL) {
         scan_err3(lc, _("Could not find config Resource %s referenced on line %d : %s\n"),
                   lc->str, lc->line_no, lc->line);
         return;
      }
      if (*(item->resvalue)) {
         scan_err3(lc, _("Attempt to redefine resource \"%s\" referenced on line %d : %s\n"),
                   item->name, lc->line_no, lc->line);
         return;
      }
      *(item->resvalue) = res;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a resource pointer in an alist. default_value indicates how many
 * times this routine can be called -- i.e. how many alists there are.
 *
 * If we are in pass 2, do a lookup of the resource.
 */
static void store_alist_res(CONFIG *config, LEX *lc, RES_ITEM *item, int index, int pass)
{
   RES *res;
   int count;
   int i = 0;
   alist *list;

   if (pass == 2) {
      count = str_to_int32(item->default_value);
      if (count == 0) {               /* always store in item->value */
         i = 0;
         if ((item->value)[i] == NULL) {
            list = New(alist(10, not_owned_by_alist));
         } else {
            list = (alist *)(item->value)[i];
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
         list = New(alist(10, not_owned_by_alist));
      }

      for (;;) {
         lex_get_token(lc, T_NAME);   /* scan next item */
         res = config->GetResWithName(item->code, lc->str);
         if (res == NULL) {
            scan_err3(lc, _("Could not find config Resource \"%s\" referenced on line %d : %s\n"),
               item->name, lc->line_no, lc->line);
            return;
         }
         Dmsg5(900, "Append %p to alist %p size=%d i=%d %s\n",
               res, list, list->size(), i, item->name);
         list->append(res);
         (item->value)[i] = (char *)list;
         if (lc->ch != ',') {         /* if no other item follows */
            break;                    /* get out */
         }
         lex_get_token(lc, T_ALL);    /* eat comma */
      }
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a string in an alist.
 */
static void store_alist_str(LEX *lc, RES_ITEM *item, int index, int pass)
{
   alist *list;

   if (pass == 2) {
      if (*(item->value) == NULL) {
         list = New(alist(10, owned_by_alist));
      } else {
         list = *(item->alistvalue);
      }

      lex_get_token(lc, T_STRING);   /* scan next item */
      Dmsg4(900, "Append %s to alist %p size=%d %s\n",
         lc->str, list, list->size(), item->name);
      list->append(bstrdup(lc->str));
      *(item->value) = (char *)list;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
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

   if (pass == 2) {
      if (*(item->value) == NULL) {
         list = New(alist(10, owned_by_alist));
      } else {
         list = (alist *)(*(item->value));
      }

      lex_get_token(lc, T_STRING);   /* scan next item */
      Dmsg4(900, "Append %s to alist %p size=%d %s\n",
            lc->str, list, list->size(), item->name);
      if (lc->str[0] != '|') {
         do_shell_expansion(lc->str, sizeof(lc->str));
      }
      list->append(bstrdup(lc->str));
      *(item->value) = (char *)list;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
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
static void store_defs(CONFIG *config, LEX *lc, RES_ITEM *item, int index, int pass)
{
   RES *res;

   lex_get_token(lc, T_NAME);
   if (pass == 2) {
     Dmsg2(900, "Code=%d name=%s\n", item->code, lc->str);
     res = config->GetResWithName(item->code, lc->str);
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
static void store_int32(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_INT32);
   *(item->i32value) = lc->int32_val;
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a positive integer at specified address
 */
static void store_pint32(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_PINT32);
   *(item->ui32value) = lc->pint32_val;
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store an 64 bit integer at specified address
 */
static void store_int64(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_INT64);
   *(item->i64value) = lc->int64_val;
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
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
         *(item->ui64value) = uvalue;
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
   set_bit(index, res_all.hdr.item_present);
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
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a yes/no in a bit field
 */
static void store_bit(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);
   if (bstrcasecmp(lc->str, "yes") || bstrcasecmp(lc->str, "true")) {
      *(item->ui32value) |= item->code;
   } else if (bstrcasecmp(lc->str, "no") || bstrcasecmp(lc->str, "false")) {
      *(item->ui32value) &= ~(item->code);
   } else {
      scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE", lc->str); /* YES and NO must not be translated */
      return;
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a bool in a bit field
 */
static void store_bool(LEX *lc, RES_ITEM *item, int index, int pass)
{
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
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store Tape Label Type (BAREOS, ANSI, IBM)
 */
static void store_label(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

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
   set_bit(index, res_all.hdr.item_present);
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
 (     }
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
   int port;

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

      port = str_to_int32(item->default_value);
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
   int port;

   port = str_to_int32(item->default_value);
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
   int port;

   port = str_to_int32(item->default_value);
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
bool CONFIG::store_resource(int type, LEX *lc, RES_ITEM *item, int index, int pass)
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
      store_res(this, lc, item, index, pass);
      break;
   case CFG_TYPE_ALIST_RES:
      store_alist_res(this, lc, item, index, pass);
      break;
   case CFG_TYPE_ALIST_STR:
      store_alist_str(lc, item, index, pass);
      break;
   case CFG_TYPE_ALIST_DIR:
      store_alist_dir(lc, item, index, pass);
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
      store_defs(this, lc, item, index, pass);
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
   default:
      return false;
   }

   return true;
}

/*
 * Initialize the static structure to zeros, then apply all the default values.
 */
void CONFIG::initialize_resource(int type, RES_ITEM *items, int pass)
{
   memset(m_res_all, 0, m_res_all_size);

   res_all.hdr.rcode = type;
   res_all.hdr.refcnt = 1;

   /*
    * Set defaults in each item. We only set defaults in pass 1.
    */
   if (pass == 1) {
      int i;

      for (i = 0; items[i].name; i++) {
         Dmsg3(900, "Item=%s def=%s defval=%s\n", items[i].name,
               (items[i].flags & CFG_ITEM_DEFAULT) ? "yes" : "no",
               (items[i].default_value) ? items[i].default_value : "None");

         /*
          * Sanity check.
          *
          * Items with a default value but without the CFG_ITEM_DEFAULT flag set
          * are most of the time an indication of a programmers error.
          */
         if (items[i].default_value != NULL && !(items[i].flags & CFG_ITEM_DEFAULT)) {
            Pmsg1(000, _("Found config item %s which has default value but no CFG_ITEM_DEFAULT flag set\n"),
                  items[i].name);
            items[i].flags |= CFG_ITEM_DEFAULT;
         }

         if (items[i].flags & CFG_ITEM_DEFAULT && items[i].default_value != NULL) {
            /*
             * First try to handle the generic types.
             */
            switch (items[i].type) {
            case CFG_TYPE_BIT:
               if (bstrcasecmp(items[i].default_value, "on")) {
                  *(items[i].ui32value) |= items[i].code;
               } else if (bstrcasecmp(items[i].default_value, "off")) {
                  *(items[i].ui32value) &= ~(items[i].code);
               }
               break;
            case CFG_TYPE_BOOL:
               if (bstrcasecmp(items[i].default_value, "yes") ||
                   bstrcasecmp(items[i].default_value, "true")) {
                  *(items[i].boolvalue) = true;
               } else if (bstrcasecmp(items[i].default_value, "no") ||
                          bstrcasecmp(items[i].default_value, "false")) {
                  *(items[i].boolvalue) = false;
               }
               break;
            case CFG_TYPE_PINT32:
            case CFG_TYPE_INT32:
            case CFG_TYPE_SIZE32:
               *(items[i].ui32value) = str_to_int32(items[i].default_value);
               break;
            case CFG_TYPE_INT64:
               *(items[i].i64value) = str_to_int64(items[i].default_value);
               break;
            case CFG_TYPE_SIZE64:
               *(items[i].ui64value) = str_to_uint64(items[i].default_value);
               break;
            case CFG_TYPE_SPEED:
               *(items[i].ui64value) = str_to_uint64(items[i].default_value);
               break;
            case CFG_TYPE_TIME:
               *(items[i].utimevalue) = str_to_int64(items[i].default_value);
               break;
            case CFG_TYPE_STRNAME:
            case CFG_TYPE_STR:
               *items[i].value = bstrdup(items[i].default_value);
               break;
            case CFG_TYPE_DIR: {
               POOL_MEM pathname(PM_FNAME);

               pm_strcpy(pathname, items[i].default_value);
               if (*pathname.c_str() != '|') {
                  int size;

                  /*
                   * Make sure we have enough room
                   */
                  size = pathname.size() + 1024;
                  pathname.check_size(size);
                  do_shell_expansion(pathname.c_str(), pathname.size());
               }
               *items[i].value = bstrdup(pathname.c_str());
               break;
            }
            case CFG_TYPE_ADDRESSES:
               init_default_addresses(items[i].dlistvalue, items[i].default_value);
               break;
            default:
               /*
                * None of the generic types fired if there is a registered callback call that now.
                */
               if (m_init_res) {
                  m_init_res(&items[i]);
               }
               break;
            }
         }

         /*
          * If this triggers, take a look at lib/parse_conf.h
          */
         if (i >= MAX_RES_ITEMS) {
            Emsg1(M_ERROR_TERM, 0, _("Too many items in %s resource\n"), m_resources[type - m_r_first]);
         }
      }
   }
}

/*
 * Recursively dump all resource of a certain type
 */
void CONFIG::dump_all_resources(int type, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   RES *res = NULL;

   if (type < 0) {
      type = -type;
   }

   foreach_res(this, res, type) {
      dump_resource(this, -type, res, sendit, sock);
   }
}

/*
 * Insert a resource specified in res into the correct resource container
 * which is a set of resources of a certain type. Most types only allow one
 * resource with a certain name and any duplicate makes the config engine
 * blow up, but if you want to continue and just ignore the duplicate you
 * can set the remove_duplicate flag to true and it will just remove the
 * duplicate when it encounters it and continue.
 */
void CONFIG::insert_resource(int rindex, RES *res, bool remove_duplicate)
{
   RES *item;

   if (m_res_containers[rindex]->list->empty()) {
      m_res_containers[rindex]->list->insert(res, res_compare);
      m_res_containers[rindex]->head = res;
      m_res_containers[rindex]->tail = res;
   } else {
      item = (RES *)m_res_containers[rindex]->list->insert(res, res_compare);

      if (item != res) {
         /*
          * RES not inserted e.g. already in the list
          */
         if (remove_duplicate) {
            free(((URES *)res)->hdr.name);
            free(res);
            return;
         } else {
            Emsg2(M_ERROR_TERM, 0,
                  _("Attempt to define second %s resource named \"%s\" is not permitted.\n"),
                  m_resources[rindex].name, ((URES *)res)->hdr.name);
         }
      }
      m_res_containers[rindex]->tail->res_next = res;
      m_res_containers[rindex]->tail = res;
   }
   Dmsg2(900, _("Inserted res: %s index=%d\n"), ((URES *)res)->hdr.name, rindex);
}

/*
 * Insert the resource in res_all into the correct resource container.
 */
void CONFIG::insert_resource(int rindex, int size)
{
   RES *res;

   res = (RES *)malloc(size);
   memcpy(res, &res_all, size);

   insert_resource(rindex, res, false);
}

/*
 * Insert an empty resource with name lc->str into the correct resource container.
 */
void CONFIG::insert_resource(int rindex, int size, LEX *lc)
{
   RES *res;

   res = (RES *)malloc(size);
   memset(res, 0, size);
   ((URES *)res)->hdr.name = bstrdup(lc->str);
   ((URES *)res)->hdr.rcode = m_r_first + rindex;

   insert_resource(rindex, res, true);
}

/*
 * Create a new resource container set.
 */
void CONFIG::new_res_containers()
{
   int i;
   int num = m_r_last - m_r_first + 1;
   RES *res = NULL;

   m_res_containers = (RES_CONTAINER **)malloc(num * sizeof(RES_CONTAINER));

   for (i = 0; i < num; i++) {
      m_res_containers[i] = (RES_CONTAINER *)malloc(sizeof(RES_CONTAINER));
      m_res_containers[i]->list = New(rblist(res, &res->link));
      m_res_containers[i]->head = NULL;
      m_res_containers[i]->tail = NULL;
   }
}

/*
 * Free configuration resources
 */
void CONFIG::free_all_resources()
{
   int i;
   RES *next, *res;

   if (m_res_containers == NULL) {
      return;
   }

   /*
    * Walk down chain of res_containers
    */
   for (i = m_r_first; i <= m_r_last; i++) {
      int index = i - m_r_first;

      if (m_res_containers[index]) {

         /*
          * Walk down resource chain freeing them
          */
         next = m_res_containers[index]->head;
         while (next) {
            res = next;
            next = res->res_next;
            free_resource(res, i);
         }

         free(m_res_containers[index]->list);
         free(m_res_containers[index]);

         m_res_containers[index] = NULL;
      }
   }

   free(m_res_containers);
}

const char *CONFIG::res_to_str(int rcode)
{
   if (rcode < m_r_first || rcode > m_r_last) {
      return _("***UNKNOWN***");
   } else {
      return m_resources[rcode - m_r_first].name;
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
