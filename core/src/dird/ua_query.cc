/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2006 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, December MMI
 */
/**
 * @file
 * BAREOS Director -- User Agent Database Query Commands
 */

#include "bareos.h"
#include "dird.h"
#include "dird/ua_db.h"
#include "dird/ua_input.h"
#include "dird/ua_select.h"

extern DirectorResource *director;

static POOLMEM *substitute_prompts(UaContext *ua,
                       POOLMEM *query, char **prompt, int nprompt);

/**
 * Read a file containing SQL queries and prompt
 *  the user to select which one.
 *
 *   File format:
 *   #  => comment
 *   :prompt for query
 *   *prompt for subst %1
 *   *prompt for subst %2
 *   ...
 *   SQL statement possibly terminated by ;
 *   :next query prompt
 */
bool query_cmd(UaContext *ua, const char *cmd)
{
   FILE *fd = NULL;
   POOLMEM *query = get_pool_memory(PM_MESSAGE);
   char line[1000];
   int i, item, len;
   char *prompt[9];
   int nprompt = 0;
   char *query_file = me->query_file;

   if (!open_client_db(ua, true)) {
      goto bail_out;
   }
   if ((fd=fopen(query_file, "rb")) == NULL) {
      berrno be;
      ua->error_msg(_("Could not open %s: ERR=%s\n"), query_file,
         be.bstrerror());
      goto bail_out;
   }

   start_prompt(ua, _("Available queries:\n"));
   while (fgets(line, sizeof(line), fd) != NULL) {
      if (line[0] == ':') {
         strip_trailing_junk(line);
         add_prompt(ua, line+1);
      }
   }
   if ((item=do_prompt(ua, "", _("Choose a query"), NULL, 0)) < 0) {
      goto bail_out;
   }
   rewind(fd);
   i = -1;
   while (fgets(line, sizeof(line), fd) != NULL) {
      if (line[0] == ':') {
         i++;
      }
      if (i == item) {
         break;
      }
   }
   if (i != item) {
      ua->error_msg(_("Could not find query.\n"));
      goto bail_out;
   }
   query[0] = 0;
   for (i=0; i<9; i++) {
      prompt[i] = NULL;
   }
   while (fgets(line, sizeof(line), fd) != NULL) {
      if (line[0] == '#') {
         continue;
      }
      if (line[0] == ':') {
         break;
      }
      strip_trailing_junk(line);
      len = strlen(line);
      if (line[0] == '*') {            /* prompt */
         if (nprompt >= 9) {
            ua->error_msg(_("Too many prompts in query, max is 9.\n"));
         } else {
            line[len++] = ' ';
            line[len] = 0;
            prompt[nprompt++] = bstrdup(line+1);
            continue;
         }
      }
      if (*query != 0) {
         pm_strcat(query, " ");
      }
      pm_strcat(query, line);
      if (line[len-1] != ';') {
         continue;
      }
      line[len-1] = 0;             /* zap ; */
      if (query[0] != 0) {
         query = substitute_prompts(ua, query, prompt, nprompt);
         Dmsg1(100, "Query2=%s\n", query);
         if (query[0] == '!') {
            ua->db->list_sql_query(ua->jcr, query + 1, ua->send, VERT_LIST, false);
         } else if (!ua->db->list_sql_query(ua->jcr, query, ua->send, HORZ_LIST, true)) {
            ua->send_msg("%s\n", query);
         }
         query[0] = 0;
      }
   } /* end while */

   if (query[0] != 0) {
      query = substitute_prompts(ua, query, prompt, nprompt);
      Dmsg1(100, "Query2=%s\n", query);
         if (query[0] == '!') {
            ua->db->list_sql_query(ua->jcr, query + 1, ua->send, VERT_LIST, false);
         } else if (!ua->db->list_sql_query(ua->jcr, query, ua->send, HORZ_LIST, true)) {
            ua->error_msg("%s\n", query);
         }
   }

bail_out:
   if (fd) {
      fclose(fd);
   }
   free_pool_memory(query);
   for (i=0; i<nprompt; i++) {
      free(prompt[i]);
   }
   return true;
}

static POOLMEM *substitute_prompts(UaContext *ua, POOLMEM *query, char **prompt, int nprompt)
{
   char *p, *q, *o;
   POOLMEM *new_query;
   int i, n, len, olen;
   char *subst[9];

   if (nprompt == 0) {
      return query;
   }

   for (i = 0; i < 9; i++) {
      subst[i] = NULL;
   }

   new_query = get_pool_memory(PM_FNAME);
   o = new_query;
   for (q=query; (p=strchr(q, '%')); ) {
      if (p) {
        olen = o - new_query;
        new_query = check_pool_memory_size(new_query, olen + p - q + 10);
        o = new_query + olen;
         while (q < p) {              /* copy up to % */
            *o++ = *q++;
         }
         p++;
         switch (*p) {
         case '1':
         case '2':
         case '3':
         case '4':
         case '5':
         case '6':
         case '7':
         case '8':
         case '9':
            n = (int)(*p) - (int)'1';
            if (prompt[n]) {
               if (!subst[n]) {
                  if (!get_cmd(ua, prompt[n])) {
                     q += 2;
                     break;
                  }
               }
               len = strlen(ua->cmd);
               p = (char *)malloc(len * 2 + 1);
               ua->db->escape_string(ua->jcr, p, ua->cmd, len);
               subst[n] = p;
               olen = o - new_query;
               new_query = check_pool_memory_size(new_query, olen + strlen(p) + 10);
               o = new_query + olen;
               while (*p) {
                  *o++ = *p++;
               }
            } else {
               ua->error_msg(_("Warning prompt %d missing.\n"), n+1);
            }
            q += 2;
            break;
         case '%':
            *o++ = '%';
            q += 2;
            break;
         default:
            *o++ = '%';
            q++;
            break;
         }
      }
   }
   olen = o - new_query;
   new_query = check_pool_memory_size(new_query, olen + strlen(q) + 10);
   o = new_query + olen;
   while (*q) {
      *o++ = *q++;
   }
   *o = 0;
   for (i=0; i<9; i++) {
      if (subst[i]) {
         free(subst[i]);
      }
   }
   free_pool_memory(query);
   return new_query;
}

/**
 * Get general SQL query for Catalog
 */
bool sqlquery_cmd(UaContext *ua, const char *cmd)
{
   PoolMem query(PM_MESSAGE);
   int len;
   const char *msg;

   if (!open_client_db(ua, true)) {
      return true;
   }
   *query.c_str() = 0;

   ua->send_msg(_("Entering SQL query mode.\n"
"Terminate each query with a semicolon.\n"
"Terminate query mode with a blank line.\n"));
   msg = _("Enter SQL query: ");
   while (get_cmd(ua, msg)) {
      len = strlen(ua->cmd);
      Dmsg2(400, "len=%d cmd=%s:\n", len, ua->cmd);
      if (len == 0) {
         break;
      }
      if (*query.c_str() != 0) {
         pm_strcat(query, " ");
      }
      pm_strcat(query, ua->cmd);
      if (ua->cmd[len-1] == ';') {
         ua->cmd[len-1] = 0;          /* zap ; */
         /*
          * Submit query
          */
         ua->db->list_sql_query(ua->jcr, query.c_str(), ua->send, HORZ_LIST, true);
         *query.c_str() = 0;         /* start new query */
         msg = _("Enter SQL query: ");
      } else {
         msg = _("Add to SQL query: ");
      }
   }
   ua->send_msg(_("End query mode.\n"));
   return true;
}
