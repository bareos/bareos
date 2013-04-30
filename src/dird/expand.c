/*
 *
 *   Bacula Director -- expand.c -- does variable expansion
 *    in particular for the LabelFormat specification.
 *
 *     Kern Sibbald, June MMIII
 *
 *   Version $Id$
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2006 Free Software Foundation Europe e.V.

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

#include "bacula.h"
#include "dird.h"



static int date_item(JCR *jcr, int code,
              const char **val_ptr, int *val_len, int *val_size)
{
   struct tm tm;
   time_t now = time(NULL);
   (void)localtime_r(&now, &tm);
   int val = 0;
   char buf[10];

   switch (code) {
   case 1:                            /* year */
      val = tm.tm_year + 1900;
      break;
   case 2:                            /* month */
      val = tm.tm_mon + 1;
      break;
   case 3:                            /* day */
      val = tm.tm_mday;
      break;
   case 4:                            /* hour */
      val = tm.tm_hour;
      break;
   case 5:                            /* minute */
      val = tm.tm_min;
      break;
   case 6:                            /* second */
      val = tm.tm_sec;
      break;
   case 7:                            /* Week day */
      val = tm.tm_wday;
      break;
   }
   bsnprintf(buf, sizeof(buf), "%d", val);
   *val_ptr = bstrdup(buf);
   *val_len = strlen(buf);
   *val_size = *val_len + 1;
   return 1;
}

static int job_item(JCR *jcr, int code,
              const char **val_ptr, int *val_len, int *val_size)
{
   const char *str = " ";
   char buf[20];

   switch (code) {
   case 1:                            /* Job */
      str = jcr->job->name();
      break;
   case 2:                            /* Director's name */
      str = my_name;
      break;
   case 3:                            /* level */
      str = job_level_to_str(jcr->getJobLevel());
      break;
   case 4:                            /* type */
      str = job_type_to_str(jcr->getJobType());
      break;
   case 5:                            /* JobId */
      bsnprintf(buf, sizeof(buf), "%d", jcr->JobId);
      str = buf;
      break;
   case 6:                            /* Client */
      str = jcr->client->name();
      if (!str) {
         str = " ";
      }
      break;
   case 7:                            /* NumVols */
      bsnprintf(buf, sizeof(buf), "%d", jcr->NumVols);
      str = buf;
      break;
   case 8:                            /* Pool */
      str = jcr->pool->name();
      break;
   case 9:                            /* Storage */
      if (jcr->wstore) {
         str = jcr->wstore->name();
      } else {
         str = jcr->rstore->name();
      }
      break;
   case 10:                           /* Catalog */
      str = jcr->catalog->name();
      break;
   case 11:                           /* MediaType */
      if (jcr->wstore) {
         str = jcr->wstore->media_type;
      } else {
         str = jcr->rstore->media_type;
      }
      break;
   case 12:                           /* JobName */
      str = jcr->Job;
      break;
   }
   *val_ptr = bstrdup(str);
   *val_len = strlen(str);
   *val_size = *val_len + 1;
   return 1;
}

struct s_built_in_vars {const char *var_name; int code; int (*func)(JCR *jcr, int code,
                         const char **val_ptr, int *val_len, int *val_size);};

/*
 * Table of build in variables
 */
static struct s_built_in_vars built_in_vars[] = {
   { NT_("Year"),       1, date_item},
   { NT_("Month"),      2, date_item},
   { NT_("Day"),        3, date_item},
   { NT_("Hour"),       4, date_item},
   { NT_("Minute"),     5, date_item},
   { NT_("Second"),     6, date_item},
   { NT_("WeekDay"),    7, date_item},

   { NT_("Job"),        1, job_item},
   { NT_("Dir"),        2, job_item},
   { NT_("Level"),      3, job_item},
   { NT_("Type"),       4, job_item},
   { NT_("JobId"),      5, job_item},
   { NT_("Client"),     6, job_item},
   { NT_("NumVols"),    7, job_item},
   { NT_("Pool"),       8, job_item},
   { NT_("Storage"),    9, job_item},
   { NT_("Catalog"),   10, job_item},
   { NT_("MediaType"), 11, job_item},
   { NT_("JobName"),   12, job_item},

   { NULL, 0, NULL}
};


/*
 * Search the table of built-in variables, and if found,
 *   call the appropriate subroutine to do the work.
 */
static var_rc_t lookup_built_in_var(var_t *ctx, void *my_ctx,
          const char *var_ptr, int var_len, int var_index,
          const char **val_ptr, int *val_len, int *val_size)
{
   JCR *jcr = (JCR *)my_ctx;
   int stat;

   for (int i=0; _(built_in_vars[i].var_name); i++) {
      if (strncmp(_(built_in_vars[i].var_name), var_ptr, var_len) == 0) {
         stat = (*built_in_vars[i].func)(jcr, built_in_vars[i].code,
            val_ptr, val_len, val_size);
         if (stat) {
            return VAR_OK;
         }
         break;
      }
   }
   return VAR_ERR_UNDEFINED_VARIABLE;
}


/*
 * Search counter variables
 */
static var_rc_t lookup_counter_var(var_t *ctx, void *my_ctx,
          const char *var_ptr, int var_len, int var_inc, int var_index,
          const char **val_ptr, int *val_len, int *val_size)
{
   char buf[MAXSTRING];
   var_rc_t stat = VAR_ERR_UNDEFINED_VARIABLE;

   if (var_len > (int)sizeof(buf) - 1) {
       return VAR_ERR_OUT_OF_MEMORY;
   }
   memcpy(buf, var_ptr, var_len);
   buf[var_len] = 0;
   LockRes();
   for (COUNTER *counter=NULL; (counter = (COUNTER *)GetNextRes(R_COUNTER, (RES *)counter)); ) {
      if (strcmp(counter->name(), buf) == 0) {
         Dmsg2(100, "Counter=%s val=%d\n", buf, counter->CurrentValue);
         /* -1 => return size of array */
        if (var_index == -1) {
            bsnprintf(buf, sizeof(buf), "%d", counter->CurrentValue);
            *val_len = bsnprintf(buf, sizeof(buf), "%d", strlen(buf));
            *val_ptr = buf;
            *val_size = 0;                  /* don't try to free val_ptr */
            return VAR_OK;
         } else {
            bsnprintf(buf, sizeof(buf), "%d", counter->CurrentValue);
            *val_ptr = bstrdup(buf);
            *val_len = strlen(buf);
            *val_size = *val_len + 1;
         }
         if (var_inc) {               /* increment the variable? */
            if (counter->CurrentValue == counter->MaxValue) {
               counter->CurrentValue = counter->MinValue;
            } else {
               counter->CurrentValue++;
            }
            if (counter->Catalog) {   /* update catalog if need be */
               COUNTER_DBR cr;
               JCR *jcr = (JCR *)my_ctx;
               memset(&cr, 0, sizeof(cr));
               bstrncpy(cr.Counter, counter->name(), sizeof(cr.Counter));
               cr.MinValue = counter->MinValue;
               cr.MaxValue = counter->MaxValue;
               cr.CurrentValue = counter->CurrentValue;
               Dmsg1(100, "New value=%d\n", cr.CurrentValue);
               if (counter->WrapCounter) {
                  bstrncpy(cr.WrapCounter, counter->WrapCounter->name(), sizeof(cr.WrapCounter));
               } else {
                  cr.WrapCounter[0] = 0;
               }
               if (!db_update_counter_record(jcr, jcr->db, &cr)) {
                  Jmsg(jcr, M_ERROR, 0, _("Count not update counter %s: ERR=%s\n"),
                     counter->name(), db_strerror(jcr->db));
               }
            }
         }
         stat = VAR_OK;
         break;
      }
   }
   UnlockRes();
   return stat;
}


/*
 * Called here from "core" expand code to look up a variable
 */
static var_rc_t lookup_var(var_t *ctx, void *my_ctx,
          const char *var_ptr, int var_len, int var_inc, int var_index,
          const char **val_ptr, int *val_len, int *val_size)
{
   char buf[MAXSTRING], *val, *p, *v;
   var_rc_t stat;
   int count;

   /* Note, if val_size > 0 and val_ptr!=NULL, the core code will free() it */
   if ((stat = lookup_built_in_var(ctx, my_ctx, var_ptr, var_len, var_index,
        val_ptr, val_len, val_size)) == VAR_OK) {
      return VAR_OK;
   }

   if ((stat = lookup_counter_var(ctx, my_ctx, var_ptr, var_len, var_inc, var_index,
        val_ptr, val_len, val_size)) == VAR_OK) {
      return VAR_OK;
   }

   /* Look in environment */
   if (var_len > (int)sizeof(buf) - 1) {
       return VAR_ERR_OUT_OF_MEMORY;
   }
   memcpy(buf, var_ptr, var_len + 1);
   buf[var_len] = 0;
   Dmsg1(100, "Var=%s\n", buf);

   if ((val = getenv(buf)) == NULL) {
       return VAR_ERR_UNDEFINED_VARIABLE;
   }
   /* He wants to index the "array" */
   count = 1;
   /* Find the size of the "array"
    *   each element is separated by a |
    */
   for (p = val; *p; p++) {
      if (*p == '|') {
         count++;
      }
   }
   Dmsg3(100, "For %s, reqest index=%d have=%d\n",
      buf, var_index, count);

   /* -1 => return size of array */
   if (var_index == -1) {
      int len;
      if (count == 1) {               /* if not array */
         len = strlen(val);           /* return length of string */
      } else {
         len = count;                 /* else return # array items */
      }
      *val_len = bsnprintf(buf, sizeof(buf), "%d", len);
      *val_ptr = buf;
      *val_size = 0;                  /* don't try to free val_ptr */
      return VAR_OK;
   }


   if (var_index < -1 || var_index > --count) {
//    return VAR_ERR_SUBMATCH_OUT_OF_RANGE;
      return VAR_ERR_UNDEFINED_VARIABLE;
   }
   /* Now find the particular item (var_index) he wants */
   count = 0;
   for (p=val; *p; ) {
      if (*p == '|') {
         if (count < var_index) {
            val = ++p;
            count++;
            continue;
         }
         break;
      }
      p++;
   }
   if (p-val > (int)sizeof(buf) - 1) {
       return VAR_ERR_OUT_OF_MEMORY;
   }
   Dmsg2(100, "val=%s len=%d\n", val, p-val);
   /* Make a copy of item, and pass it back */
   v = (char *)malloc(p-val+1);
   memcpy(v, val, p-val);
   v[p-val] = 0;
   *val_ptr = v;
   *val_len = p-val;
   *val_size = p-val+1;
   Dmsg1(100, "v=%s\n", v);
   return VAR_OK;
}

/*
 * Called here to do a special operation on a variable
 *   op_ptr  points to the special operation code (not EOS terminated)
 *   arg_ptr points to argument to special op code
 *   val_ptr points to the value string
 *   out_ptr points to string to be returned
 */
static var_rc_t operate_var(var_t *var, void *my_ctx,
          const char *op_ptr, int op_len,
          const char *arg_ptr, int arg_len,
          const char *val_ptr, int val_len,
          char **out_ptr, int *out_len, int *out_size)
{
   var_rc_t stat = VAR_ERR_UNDEFINED_OPERATION;
   Dmsg0(100, "Enter operate_var\n");
   if (!val_ptr) {
      *out_size = 0;
      return stat;
   }
   if (op_len == 3 && strncmp(op_ptr, "inc", 3) == 0) {
      char buf[MAXSTRING];
      if (val_len > (int)sizeof(buf) - 1) {
          return VAR_ERR_OUT_OF_MEMORY;
      }
      memcpy(buf, arg_ptr, arg_len);
      buf[arg_len] = 0;
      Dmsg1(100, "Arg=%s\n", buf);
      memcpy(buf, val_ptr, val_len);
      buf[val_len] = 0;
      Dmsg1(100, "Val=%s\n", buf);
      LockRes();
      for (COUNTER *counter=NULL; (counter = (COUNTER *)GetNextRes(R_COUNTER, (RES *)counter)); ) {
         if (strcmp(counter->name(), buf) == 0) {
            Dmsg2(100, "counter=%s val=%s\n", counter->name(), buf);
            break;
         }
      }
      UnlockRes();
      return stat;
   }
   *out_size = 0;
   return stat;
}


/*
 * Expand an input line and return it.
 *
 *  Returns: 0 on failure
 *           1 on success and exp has expanded input
 */
int variable_expansion(JCR *jcr, char *inp, POOLMEM **exp)
{
   var_t *var_ctx;
   var_rc_t stat;
   char *outp;
   int in_len, out_len;
   int rtn_stat = 0;

   in_len = strlen(inp);
   outp = NULL;
   out_len = 0;

   /* create context */
   if ((stat = var_create(&var_ctx)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot create var context: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }
   /* define callback */
   if ((stat = var_config(var_ctx, VAR_CONFIG_CB_VALUE, lookup_var, (void *)jcr)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot set var callback: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }

   /* define special operations */
   if ((stat = var_config(var_ctx, VAR_CONFIG_CB_OPERATION, operate_var, (void *)jcr)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot set var operate: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }

   /* unescape in place */
   if ((stat = var_unescape(var_ctx, inp, in_len, inp, in_len+1, 0)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot unescape string: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }

   in_len = strlen(inp);

   /* expand variables */
   if ((stat = var_expand(var_ctx, inp, in_len, &outp, &out_len, 0)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot expand expression \"%s\": ERR=%s\n"),
          inp, var_strerror(var_ctx, stat));
       goto bail_out;
   }

   /* unescape once more in place */
   if ((stat = var_unescape(var_ctx, outp, out_len, outp, out_len+1, 1)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot unescape string: ERR=%s\n"), var_strerror(var_ctx, stat));
       goto bail_out;
   }

   pm_strcpy(exp, outp);

   rtn_stat = 1;

bail_out:
   /* destroy expansion context */
   if ((stat = var_destroy(var_ctx)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot destroy var context: ERR=%s\n"), var_strerror(var_ctx, stat));
   }
   if (outp) {
      free(outp);
   }
   return rtn_stat;
}
