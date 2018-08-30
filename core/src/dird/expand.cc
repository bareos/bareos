/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2006 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, June MMIII
 */
/**
 * @file
 * does variable expansion
 * in particular for the LabelFormat specification.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"

namespace directordaemon {

static int date_item(JobControlRecord *jcr,
                     int code,
                     const char **val_ptr,
                     int *val_len,
                     int *val_size)
{
   int val = 0;
   char buf[10];
   struct tm tm;
   time_t now = time(NULL);

   Blocaltime(&now, &tm);

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
   Bsnprintf(buf, sizeof(buf), "%d", val);
   *val_ptr = bstrdup(buf);
   *val_len = strlen(buf);
   *val_size = *val_len + 1;

   return 1;
}

static int job_item(JobControlRecord *jcr,
                    int code,
                    const char **val_ptr,
                    int *val_len,
                    int *val_size)
{
   const char *str = " ";
   char buf[20];

   switch (code) {
   case 1:                            /* Job */
      str = jcr->res.job->name();
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
      Bsnprintf(buf, sizeof(buf), "%d", jcr->JobId);
      str = buf;
      break;
   case 6:                            /* Client */
      str = jcr->res.client->name();
      if (!str) {
         str = " ";
      }
      break;
   case 7:                            /* NumVols */
      Bsnprintf(buf, sizeof(buf), "%d", jcr->NumVols);
      str = buf;
      break;
   case 8:                            /* Pool */
      str = jcr->res.pool->name();
      break;
   case 9:                            /* Storage */
      if (jcr->res.wstore) {
         str = jcr->res.wstore->name();
      } else {
         str = jcr->res.rstore->name();
      }
      break;
   case 10:                           /* Catalog */
      str = jcr->res.catalog->name();
      break;
   case 11:                           /* MediaType */
      if (jcr->res.wstore) {
         str = jcr->res.wstore->media_type;
      } else {
         str = jcr->res.rstore->media_type;
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

struct s_built_in_vars {
   const char *var_name;
   int code;
   int (*func)(JobControlRecord *jcr, int code, const char **val_ptr, int *val_len, int *val_size);
};

/*
 * Table of build in variables
 */
static struct s_built_in_vars built_in_vars[] = {
   { NT_("Year"), 1, date_item },
   { NT_("Month"), 2, date_item },
   { NT_("Day"), 3, date_item },
   { NT_("Hour"), 4, date_item },
   { NT_("Minute"), 5, date_item },
   { NT_("Second"), 6, date_item },
   { NT_("WeekDay"), 7, date_item },
   { NT_("Job"), 1, job_item },
   { NT_("Dir"), 2, job_item },
   { NT_("Level"), 3, job_item },
   { NT_("Type"), 4, job_item },
   { NT_("JobId"), 5, job_item },
   { NT_("Client"), 6, job_item },
   { NT_("NumVols"), 7, job_item },
   { NT_("Pool"), 8, job_item },
   { NT_("Storage"), 9, job_item },
   { NT_("Catalog"), 10, job_item },
   { NT_("MediaType"), 11, job_item },
   { NT_("JobName"), 12, job_item },
   { NULL, 0, NULL }
};

/**
 * Search the table of built-in variables, and if found,
 * call the appropriate subroutine to do the work.
 */
static var_rc_t lookup_built_in_var(var_t *ctx,
                                    void *my_ctx,
                                    const char *var_ptr,
                                    int var_len,
                                    int var_index,
                                    const char **val_ptr,
                                    int *val_len,
                                    int *val_size)
{
   JobControlRecord *jcr = (JobControlRecord *)my_ctx;
   int status, i;

   for (i = 0; _(built_in_vars[i].var_name); i++) {
      if (bstrncmp(_(built_in_vars[i].var_name), var_ptr, var_len)) {
         status = (*built_in_vars[i].func)(jcr, built_in_vars[i].code,
                                           val_ptr, val_len, val_size);
         if (status) {
            return VAR_OK;
         }
         break;
      }
   }

   return VAR_ERR_UNDEFINED_VARIABLE;
}

/**
 * Search counter variables
 */
static var_rc_t lookup_counter_var(var_t *ctx,
                                   void *my_ctx,
                                   const char *var_ptr,
                                   int var_len,
                                   int var_inc,
                                   int var_index,
                                   const char **val_ptr,
                                   int *val_len,
                                   int *val_size)
{
   CounterResource *counter;
   PoolMem buf(PM_NAME);
   var_rc_t status = VAR_ERR_UNDEFINED_VARIABLE;

   buf.check_size(var_len + 1);
   PmMemcpy(buf, var_ptr, var_len);
   (buf.c_str())[var_len] = 0;

   LockRes();
   for (counter = NULL; (counter = (CounterResource *)my_config->GetNextRes(R_COUNTER, (CommonResourceHeader *)counter)); ) {
      if (bstrcmp(counter->name(), buf.c_str())) {
         Dmsg2(100, "Counter=%s val=%d\n", buf.c_str(), counter->CurrentValue);
         /*
          * -1 => return size of array
          */
         if (var_index == -1) {
            Mmsg(buf, "%d", counter->CurrentValue);
            *val_len = Mmsg(buf, "%d", strlen(buf.c_str()));
            *val_ptr = bstrdup(buf.c_str());
            *val_size = 0;                  /* don't try to free val_ptr */
            return VAR_OK;
         } else {
            Mmsg(buf, "%d", counter->CurrentValue);
            *val_ptr = bstrdup(buf.c_str());
            *val_len = strlen(buf.c_str());
            *val_size = *val_len + 1;
         }
         if (var_inc) {               /* increment the variable? */
            if (counter->CurrentValue == counter->MaxValue) {
               counter->CurrentValue = counter->MinValue;
            } else {
               counter->CurrentValue++;
            }
            if (counter->Catalog) {   /* update catalog if need be */
               CounterDbRecord cr;
               JobControlRecord *jcr = (JobControlRecord *)my_ctx;
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
               if (!jcr->db->UpdateCounterRecord(jcr, &cr)) {
                  Jmsg(jcr, M_ERROR, 0, _("Count not update counter %s: ERR=%s\n"),
                       counter->name(), jcr->db->strerror());
               }
            }
         }
         status = VAR_OK;
         break;
      }
   }
   UnlockRes();

   return status;
}

/**
 * Called here from "core" expand code to look up a variable
 */
static var_rc_t lookup_var(var_t *ctx,
                           void *my_ctx,
                           const char *var_ptr,
                           int var_len,
                           int var_inc,
                           int var_index,
                           const char **val_ptr,
                           int *val_len,
                           int *val_size)
{
   PoolMem buf(PM_NAME);
   char *val, *p, *v;
   var_rc_t status;
   int count;

   /*
    * Note, if val_size > 0 and val_ptr!=NULL, the core code will free() it
    */
   if ((status = lookup_built_in_var(ctx, my_ctx, var_ptr, var_len, var_index,
                                     val_ptr, val_len, val_size)) == VAR_OK) {
      return VAR_OK;
   }

   if ((status = lookup_counter_var(ctx, my_ctx, var_ptr, var_len, var_inc, var_index,
                                    val_ptr, val_len, val_size)) == VAR_OK) {
      return VAR_OK;
   }

   /*
    * Look in environment
    */
   buf.check_size(var_len + 1);
   PmMemcpy(buf, var_ptr, var_len);
   (buf.c_str())[var_len] = 0;
   Dmsg1(100, "Var=%s\n", buf.c_str());

   if ((val = getenv(buf.c_str())) == NULL) {
       return VAR_ERR_UNDEFINED_VARIABLE;
   }

   /*
    * He wants to index the "array"
    */
   count = 1;

   /*
    * Find the size of the "array" each element is separated by a |
    */
   for (p = val; *p; p++) {
      if (*p == '|') {
         count++;
      }
   }

   Dmsg3(100, "For %s, reqest index=%d have=%d\n", buf.c_str(), var_index, count);

   /*
    * -1 => return size of array
    */
   if (var_index == -1) {
      int len;
      if (count == 1) {               /* if not array */
         len = strlen(val);           /* return length of string */
      } else {
         len = count;                 /* else return # array items */
      }
      *val_len = Mmsg(buf, "%d", len);
      *val_ptr = bstrdup(buf.c_str());
      *val_size = 0;                  /* don't try to free val_ptr */
      return VAR_OK;
   }


   if (var_index < -1 || var_index > --count) {
//    return VAR_ERR_SUBMATCH_OUT_OF_RANGE;
      return VAR_ERR_UNDEFINED_VARIABLE;
   }

   /*
    * Now find the particular item (var_index) he wants
    */
   count = 0;
   for (p = val; *p; ) {
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

   buf.check_size(p - val);
   Dmsg2(100, "val=%s len=%d\n", val, p - val);

   /*
    * Make a copy of item, and pass it back
    */
   v = (char *)malloc(p-val+1);
   memcpy(v, val, p-val);
   v[p-val] = 0;
   *val_ptr = v;
   *val_len = p-val;
   *val_size = p-val+1;
   Dmsg1(100, "v=%s\n", v);

   return VAR_OK;
}

/**
 * Called here to do a special operation on a variable
 *   op_ptr  points to the special operation code (not EOS terminated)
 *   arg_ptr points to argument to special op code
 *   val_ptr points to the value string
 *   out_ptr points to string to be returned
 */
static var_rc_t operate_var(var_t *var,
                            void *my_ctx,
                            const char *op_ptr,
                            int op_len,
                            const char *arg_ptr,
                            int arg_len,
                            const char *val_ptr,
                            int val_len,
                            char **out_ptr,
                            int *out_len,
                            int *out_size)
{
   CounterResource *counter;
   PoolMem buf(PM_NAME);
   var_rc_t status = VAR_ERR_UNDEFINED_OPERATION;

   Dmsg0(100, "Enter operate_var\n");
   if (!val_ptr) {
      *out_size = 0;
      return status;
   }

   if (op_len == 3 && bstrncmp(op_ptr, "inc", 3)) {
      buf.check_size(val_len + 1);
      PmMemcpy(buf, arg_ptr, val_len);
      (buf.c_str())[val_len] = 0;
      Dmsg1(100, "Arg=%s\n", buf.c_str());

      PmMemcpy(buf, val_ptr, val_len);
      (buf.c_str())[val_len] = 0;
      Dmsg1(100, "Val=%s\n", buf.c_str());

      LockRes();
      for (counter = NULL; (counter = (CounterResource *)my_config->GetNextRes(R_COUNTER, (CommonResourceHeader *)counter)); ) {
         if (bstrcmp(counter->name(), buf.c_str())) {
            Dmsg2(100, "counter=%s val=%s\n", counter->name(), buf.c_str());
            break;
         }
      }
      UnlockRes();
      return status;
   }
   *out_size = 0;

   return status;
}

/**
 * Expand an input line and return it.
 *
 * Returns: 0 on failure
 *          1 on success and exp has expanded input
 */
int VariableExpansion(JobControlRecord *jcr, char *inp, POOLMEM *&exp)
{
   var_t *var_ctx;
   var_rc_t status;
   char *outp;
   int in_len, out_len;
   int rtn_stat = 0;

   in_len = strlen(inp);
   outp = NULL;
   out_len = 0;

   /*
    * Create context
    */
   if ((status = var_create(&var_ctx)) != VAR_OK) {
      Jmsg(jcr, M_ERROR, 0, _("Cannot create var context: ERR=%s\n"), var_strerror(var_ctx, status));
      goto bail_out;
   }

   /*
    * Define callback
    */
   if ((status = var_config(var_ctx, VAR_CONFIG_CB_VALUE, lookup_var, (void *)jcr)) != VAR_OK) {
      Jmsg(jcr, M_ERROR, 0, _("Cannot set var callback: ERR=%s\n"), var_strerror(var_ctx, status));
      goto bail_out;
   }

   /*
    * Define special operations
    */
   if ((status = var_config(var_ctx, VAR_CONFIG_CB_OPERATION, operate_var, (void *)jcr)) != VAR_OK) {
      Jmsg(jcr, M_ERROR, 0, _("Cannot set var operate: ERR=%s\n"), var_strerror(var_ctx, status));
      goto bail_out;
   }

   /*
    * Unescape in place
    */
   if ((status = var_unescape(var_ctx, inp, in_len, inp, in_len+1, 0)) != VAR_OK) {
      Jmsg(jcr, M_ERROR, 0, _("Cannot unescape string: ERR=%s\n"), var_strerror(var_ctx, status));
      goto bail_out;
   }

   in_len = strlen(inp);

   /*
    * Expand variables
    */
   if ((status = var_expand(var_ctx, inp, in_len, &outp, &out_len, 0)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot expand expression \"%s\": ERR=%s\n"), inp, var_strerror(var_ctx, status));
       goto bail_out;
   }

   /*
    * Unescape once more in place
    */
   if ((status = var_unescape(var_ctx, outp, out_len, outp, out_len+1, 1)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot unescape string: ERR=%s\n"), var_strerror(var_ctx, status));
       goto bail_out;
   }

   PmStrcpy(exp, outp);

   rtn_stat = 1;

bail_out:
   /*
    * Destroy expansion context
    */
   if ((status = var_destroy(var_ctx)) != VAR_OK) {
       Jmsg(jcr, M_ERROR, 0, _("Cannot destroy var context: ERR=%s\n"), var_strerror(var_ctx, status));
   }

   if (outp) {
      free(outp);
   }

   return rtn_stat;
}
} /* namespace directordaemon */
