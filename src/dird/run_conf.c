/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Configuration parser for Director Run Configuration
 * directives, which are part of the Schedule Resource
 *
 * Kern Sibbald, May MM
 */

#include "bareos.h"
#include "dird.h"

extern struct s_jl joblevels[];

/*
 * Forward referenced subroutines
 */
enum e_state {
   s_none = 0,
   s_range,
   s_mday,
   s_month,
   s_time,
   s_at,
   s_wday,
   s_daily,
   s_weekly,
   s_monthly,
   s_hourly,
   s_wom,                             /* 1st, 2nd, ...*/
   s_woy,                             /* week of year w00 - w53 */
   s_last,                            /* last week of month */
   s_modulo                           /* every nth monthday/week */
};

struct s_keyw {
  const char *name;                   /* keyword */
  enum e_state state;                 /* parser state */
  int code;                           /* state value */
};

/*
 * Keywords understood by parser
 */
static struct s_keyw keyw[] = {
   { NT_("on"), s_none, 0 },
   { NT_("at"), s_at, 0 },
   { NT_("last"), s_last, 0 },
   { NT_("sun"), s_wday, 0 },
   { NT_("mon"), s_wday, 1 },
   { NT_("tue"), s_wday, 2 },
   { NT_("wed"), s_wday, 3 },
   { NT_("thu"), s_wday, 4 },
   { NT_("fri"), s_wday, 5 },
   { NT_("sat"), s_wday, 6 },
   { NT_("jan"), s_month, 0 },
   { NT_("feb"), s_month, 1 },
   { NT_("mar"), s_month, 2 },
   { NT_("apr"), s_month, 3 },
   { NT_("may"), s_month, 4 },
   { NT_("jun"), s_month, 5 },
   { NT_("jul"), s_month, 6 },
   { NT_("aug"), s_month, 7 },
   { NT_("sep"), s_month, 8 },
   { NT_("oct"), s_month, 9 },
   { NT_("nov"), s_month, 10 },
   { NT_("dec"), s_month, 11 },
   { NT_("sunday"), s_wday, 0 },
   { NT_("monday"), s_wday, 1 },
   { NT_("tuesday"), s_wday, 2 },
   { NT_("wednesday"), s_wday, 3 },
   { NT_("thursday"), s_wday, 4 },
   { NT_("friday"), s_wday, 5 },
   { NT_("saturday"), s_wday, 6 },
   { NT_("january"), s_month, 0 },
   { NT_("february"), s_month, 1 },
   { NT_("march"), s_month, 2 },
   { NT_("april"), s_month, 3 },
   { NT_("june"), s_month, 5 },
   { NT_("july"), s_month, 6 },
   { NT_("august"), s_month, 7 },
   { NT_("september"), s_month, 8 },
   { NT_("october"), s_month, 9 },
   { NT_("november"), s_month, 10 },
   { NT_("december"), s_month, 11 },
   { NT_("daily"), s_daily, 0 },
   { NT_("weekly"), s_weekly, 0 },
   { NT_("monthly"), s_monthly, 0 },
   { NT_("hourly"), s_hourly, 0 },
   { NT_("1st"), s_wom, 0 },
   { NT_("2nd"), s_wom, 1 },
   { NT_("3rd"), s_wom, 2 },
   { NT_("4th"), s_wom, 3 },
   { NT_("5th"), s_wom, 4 },
   { NT_("first"), s_wom, 0 },
   { NT_("second"), s_wom, 1 },
   { NT_("third"), s_wom, 2 },
   { NT_("fourth"), s_wom, 3 },
   { NT_("fifth"), s_wom, 4 },
   { NULL, s_none, 0 }
};

static bool have_hour, have_mday, have_wday, have_month, have_wom;
static bool have_at, have_woy;
static RUNRES lrun;

static void set_defaults()
{
   have_hour = have_mday = have_wday = have_month = have_wom = have_woy = false;
   have_at = false;
   set_bits(0, 23, lrun.hour);
   set_bits(0, 30, lrun.mday);
   set_bits(0, 6,  lrun.wday);
   set_bits(0, 11, lrun.month);
   set_bits(0, 4,  lrun.wom);
   set_bits(0, 53, lrun.woy);
}

/*
 * Keywords (RHS) permitted in Run records
 */
static struct s_kw RunFields[] = {
   { "pool", 'P' },
   { "fullpool", 'f' },
   { "incrementalpool", 'i' },
   { "differentialpool", 'd' },
   { "nextpool", 'n' },
   { "level", 'L' },
   { "storage", 'S' },
   { "messages", 'M' },
   { "priority", 'p' },
   { "spooldata", 's' },
   { "maxrunschedtime", 'm' },
   { "accurate", 'a' },
   { NULL, 0 }
};

/*
 * Store Schedule Run information
 *
 * Parse Run statement:
 *
 *  Run <keyword=value ...> [on] 2 january at 23:45
 *
 *   Default Run time is daily at 0:0
 *
 *   There can be multiple run statements, they are simply chained
 *   together.
 *
 */
void store_run(LEX *lc, RES_ITEM *item, int index, int pass)
{
   char *p;
   int i, j;
   int options = lc->options;
   int token, state, state2 = 0, code = 0, code2 = 0;
   bool found;
   utime_t utime;
   RES *res;
   RUNRES **run = (RUNRES **)(item->value);
   URES *res_all = (URES *)my_config->m_res_all;

   lc->options |= LOPT_NO_IDENT;      /* Want only "strings" */

   /*
    * Clear local copy of run record
    */
   memset(&lrun, 0, sizeof(lrun));

   /*
    * Scan for Job level "full", "incremental", ...
    */
   for (found = true; found; ) {
      found = false;
      token = lex_get_token(lc, T_NAME);
      for (i = 0; !found && RunFields[i].name; i++) {
         if (bstrcasecmp(lc->str, RunFields[i].name)) {
            found = true;
            if (lex_get_token(lc, T_ALL) != T_EQUALS) {
               scan_err1(lc, _("Expected an equals, got: %s"), lc->str);
               /* NOT REACHED */
            }
            switch (RunFields[i].token) {
            case 's':                 /* Data spooling */
               token = lex_get_token(lc, T_NAME);
               if (bstrcasecmp(lc->str, "yes") || bstrcasecmp(lc->str, "true")) {
                  lrun.spool_data = true;
                  lrun.spool_data_set = true;
               } else if (bstrcasecmp(lc->str, "no") || bstrcasecmp(lc->str, "false")) {
                  lrun.spool_data = false;
                  lrun.spool_data_set = true;
               } else {
                  scan_err1(lc, _("Expect a YES or NO, got: %s"), lc->str);
               }
               break;
            case 'L':                 /* Level */
               token = lex_get_token(lc, T_NAME);
               for (j = 0; joblevels[j].level_name; j++) {
                  if (bstrcasecmp(lc->str, joblevels[j].level_name)) {
                     lrun.level = joblevels[j].level;
                     lrun.job_type = joblevels[j].job_type;
                     j = 0;
                     break;
                  }
               }
               if (j != 0) {
                  scan_err1(lc, _("Job level field: %s not found in run record"), lc->str);
                  /* NOT REACHED */
               }
               break;
            case 'p':                 /* Priority */
               token = lex_get_token(lc, T_PINT32);
               if (pass == 2) {
                  lrun.Priority = lc->pint32_val;
               }
               break;
            case 'P':                 /* Pool */
            case 'f':                 /* FullPool */
            case 'i':                 /* IncPool */
            case 'd':                 /* DiffPool */
            case 'n':                 /* NextPool */
               token = lex_get_token(lc, T_NAME);
               if (pass == 2) {
                  res = GetResWithName(R_POOL, lc->str);
                  if (res == NULL) {
                     scan_err1(lc, _("Could not find specified Pool Resource: %s"),
                                lc->str);
                     /* NOT REACHED */
                  }
                  switch(RunFields[i].token) {
                  case 'P':
                     lrun.pool = (POOLRES *)res;
                     break;
                  case 'f':
                     lrun.full_pool = (POOLRES *)res;
                     break;
                  case 'i':
                     lrun.inc_pool = (POOLRES *)res;
                     break;
                  case 'd':
                     lrun.diff_pool = (POOLRES *)res;
                     break;
                  case 'n':
                     lrun.next_pool = (POOLRES *)res;
                     break;
                  }
               }
               break;
            case 'S':                 /* Storage */
               token = lex_get_token(lc, T_NAME);
               if (pass == 2) {
                  res = GetResWithName(R_STORAGE, lc->str);
                  if (res == NULL) {
                     scan_err1(lc, _("Could not find specified Storage Resource: %s"),
                                lc->str);
                     /* NOT REACHED */
                  }
                  lrun.storage = (STORERES *)res;
               }
               break;
            case 'M':                 /* Messages */
               token = lex_get_token(lc, T_NAME);
               if (pass == 2) {
                  res = GetResWithName(R_MSGS, lc->str);
                  if (res == NULL) {
                     scan_err1(lc, _("Could not find specified Messages Resource: %s"),
                                lc->str);
                     /* NOT REACHED */
                  }
                  lrun.msgs = (MSGSRES *)res;
               }
               break;
            case 'm':                 /* Max run sched time */
               token = lex_get_token(lc, T_QUOTED_STRING);
               if (!duration_to_utime(lc->str, &utime)) {
                  scan_err1(lc, _("expected a time period, got: %s"), lc->str);
                  return;
               }
               lrun.MaxRunSchedTime = utime;
               lrun.MaxRunSchedTime_set = true;
               break;
            case 'a':                 /* Accurate */
               token = lex_get_token(lc, T_NAME);
               if (strcasecmp(lc->str, "yes") == 0 || strcasecmp(lc->str, "true") == 0) {
                  lrun.accurate = true;
                  lrun.accurate_set = true;
               } else if (strcasecmp(lc->str, "no") == 0 || strcasecmp(lc->str, "false") == 0) {
                  lrun.accurate = false;
                  lrun.accurate_set = true;
               } else {
                  scan_err1(lc, _("Expect a YES or NO, got: %s"), lc->str);
               }
               break;
            default:
               scan_err1(lc, _("Expected a keyword name, got: %s"), lc->str);
               /* NOT REACHED */
               break;
            } /* end switch */
         } /* end if bstrcasecmp */
      } /* end for RunFields */

      /*
       * At this point, it is not a keyword. Check for old syle
       * Job Levels without keyword. This form is depreciated!!!
       */
      if (!found) {
         for (j = 0; joblevels[j].level_name; j++) {
            if (bstrcasecmp(lc->str, joblevels[j].level_name)) {
               lrun.level = joblevels[j].level;
               lrun.job_type = joblevels[j].job_type;
               found = true;
               break;
            }
         }
      }
   } /* end for found */

   /*
    * Scan schedule times.
    * Default is: daily at 0:0
    */
   state = s_none;
   set_defaults();

   for (; token != T_EOL; (token = lex_get_token(lc, T_ALL))) {
      int len;
      bool pm = false;
      bool am = false;
      switch (token) {
      case T_NUMBER:
         state = s_mday;
         code = atoi(lc->str) - 1;
         if (code < 0 || code > 30) {
            scan_err0(lc, _("Day number out of range (1-31)"));
         }
         break;
      case T_NAME:                    /* This handles drop through from keyword */
      case T_UNQUOTED_STRING:
         if (strchr(lc->str, (int)'-')) {
            state = s_range;
            break;
         }
         if (strchr(lc->str, (int)':')) {
            state = s_time;
            break;
         }
         if (strchr(lc->str, (int)'/')) {
            state = s_modulo;
            break;
         }
         if (lc->str_len == 3 && (lc->str[0] == 'w' || lc->str[0] == 'W') &&
             is_an_integer(lc->str+1)) {
            code = atoi(lc->str+1);
            if (code < 0 || code > 53) {
               scan_err0(lc, _("Week number out of range (0-53)"));
              /* NOT REACHED */
            }
            state = s_woy;            /* Week of year */
            break;
         }
         /*
          * Everything else must be a keyword
          */
         for (i = 0; keyw[i].name; i++) {
            if (bstrcasecmp(lc->str, keyw[i].name)) {
               state = keyw[i].state;
               code   = keyw[i].code;
               i = 0;
               break;
            }
         }
         if (i != 0) {
            scan_err1(lc, _("Job type field: %s in run record not found"), lc->str);
            /* NOT REACHED */
         }
         break;
      case T_COMMA:
         continue;
      default:
         scan_err2(lc, _("Unexpected token: %d:%s"), token, lc->str);
         /* NOT REACHED */
         break;
      }
      switch (state) {
      case s_none:
         continue;
      case s_mday:                    /* Day of month */
         if (!have_mday) {
            clear_bits(0, 30, lrun.mday);
            have_mday = true;
         }
         set_bit(code, lrun.mday);
         break;
      case s_month:                   /* Month of year */
         if (!have_month) {
            clear_bits(0, 11, lrun.month);
            have_month = true;
         }
         set_bit(code, lrun.month);
         break;
      case s_wday:                    /* Week day */
         if (!have_wday) {
            clear_bits(0, 6, lrun.wday);
            have_wday = true;
         }
         set_bit(code, lrun.wday);
         break;
      case s_wom:                     /* Week of month 1st, ... */
         if (!have_wom) {
            clear_bits(0, 4, lrun.wom);
            have_wom = true;
         }
         set_bit(code, lrun.wom);
         break;
      case s_woy:
         if (!have_woy) {
            clear_bits(0, 53, lrun.woy);
            have_woy = true;
         }
         set_bit(code, lrun.woy);
         break;
      case s_time:                    /* Time */
         if (!have_at) {
            scan_err0(lc, _("Time must be preceded by keyword AT."));
            /* NOT REACHED */
         }
         if (!have_hour) {
            clear_bits(0, 23, lrun.hour);
         }
//       Dmsg1(000, "s_time=%s\n", lc->str);
         p = strchr(lc->str, ':');
         if (!p)  {
            scan_err0(lc, _("Time logic error.\n"));
            /* NOT REACHED */
         }
         *p++ = 0;                    /* Separate two halves */
         code = atoi(lc->str);        /* Pick up hour */
         code2 = atoi(p);             /* Pick up minutes */
         len = strlen(p);
         if (len >= 2) {
            p += 2;
         }
         if (bstrcasecmp(p, "pm")) {
            pm = true;
         } else if (bstrcasecmp(p, "am")) {
            am = true;
         } else if (len != 2) {
            scan_err0(lc, _("Bad time specification."));
            /* NOT REACHED */
         }
         /*
          * Note, according to NIST, 12am and 12pm are ambiguous and
          *  can be defined to anything.  However, 12:01am is the same
          *  as 00:01 and 12:01pm is the same as 12:01, so we define
          *  12am as 00:00 and 12pm as 12:00.
          */
         if (pm) {
            /*
             * Convert to 24 hour time
             */
            if (code != 12) {
               code += 12;
            }
         } else if (am && code == 12) {
            /*
             * AM
             */
            code -= 12;
         }
         if (code < 0 || code > 23 || code2 < 0 || code2 > 59) {
            scan_err0(lc, _("Bad time specification."));
            /* NOT REACHED */
         }
         set_bit(code, lrun.hour);
         lrun.minute = code2;
         have_hour = true;
         break;
      case s_at:
         have_at = true;
         break;
      case s_last:
         lrun.last_set = true;
         if (!have_wom) {
            clear_bits(0, 4, lrun.wom);
            have_wom = true;
         }
         break;
      case s_modulo:
         p = strchr(lc->str, '/');
         if (!p) {
            scan_err0(lc, _("Modulo logic error.\n"));
         }
         *p++ = 0;                 /* Separate two halves */

         if (is_an_integer(lc->str) && is_an_integer(p)) {
            /*
             * Check for day modulo specification.
             */
            code = atoi(lc->str) - 1;
            code2 = atoi(p);
            if (code < 0 || code > 30 || code2 < 0 || code2 > 30) {
               scan_err0(lc, _("Bad day specification in modulo."));
            }
            if (code > code2) {
               scan_err0(lc, _("Bad day specification, offset must always be <= than modulo."));
            }
            if (!have_mday) {
               clear_bits(0, 30, lrun.mday);
               have_mday = true;
            }
            /*
             * Set the bits according to the modulo specification.
             */
            for (i = 0; i < 31; i++) {
               if (i % code2 == 0) {
                  set_bit(i + code, lrun.mday);
               }
            }
         } else if (strlen(lc->str) == 3 && strlen(p) == 3 &&
                   (lc->str[0] == 'w' || lc->str[0] == 'W') &&
                   (p[0] == 'w' || p[0] == 'W') &&
                    is_an_integer(lc->str + 1) &&
                    is_an_integer(p + 1)) {
            /*
             * Check for week modulo specification.
             */
            code = atoi(lc->str + 1);
            code2 = atoi(p + 1);
            if (code < 0 || code > 53 || code2 < 0 || code2 > 53) {
               scan_err0(lc, _("Week number out of range (0-53) in modulo"));
            }
            if (code > code2) {
               scan_err0(lc, _("Bad week number specification in modulo, offset must always be <= than modulo."));
            }
            if (!have_woy) {
               clear_bits(0, 53, lrun.woy);
               have_woy = true;
            }
            /*
             * Set the bits according to the modulo specification.
             */
            for (i = 0; i < 54; i++) {
               if (i % code2 == 0) {
                  set_bit(i + code - 1, lrun.woy);
               }
            }
         } else {
            scan_err0(lc, _("Bad modulo time specification. Format for weekdays is '01/02', for yearweeks is 'w01/w02'."));
         }
         break;
      case s_range:
         p = strchr(lc->str, '-');
         if (!p) {
            scan_err0(lc, _("Range logic error.\n"));
         }
         *p++ = 0;                    /* Separate two halves */

         if (is_an_integer(lc->str) && is_an_integer(p)) {
            /*
             * Check for day range.
             */
            code = atoi(lc->str) - 1;
            code2 = atoi(p) - 1;
            if (code < 0 || code > 30 || code2 < 0 || code2 > 30) {
               scan_err0(lc, _("Bad day range specification."));
            }
            if (!have_mday) {
               clear_bits(0, 30, lrun.mday);
               have_mday = true;
            }
            if (code < code2) {
               set_bits(code, code2, lrun.mday);
            } else {
               set_bits(code, 30, lrun.mday);
               set_bits(0, code2, lrun.mday);
            }
         } else if (strlen(lc->str) == 3 && strlen(p) == 3 &&
                   (lc->str[0] == 'w' || lc->str[0] == 'W') &&
                   (p[0] == 'w' || p[0] == 'W') &&
                    is_an_integer(lc->str + 1) &&
                    is_an_integer(p + 1)) {
            /*
             * Check for week of year range.
             */
            code = atoi(lc->str + 1);
            code2 = atoi(p + 1);
            if (code < 0 || code > 53 || code2 < 0 || code2 > 53) {
               scan_err0(lc, _("Week number out of range (0-53)"));
            }
            if (!have_woy) {
               clear_bits(0, 53, lrun.woy);
               have_woy = true;
            }
            if (code < code2) {
               set_bits(code, code2, lrun.woy);
            } else {
               set_bits(code, 53, lrun.woy);
               set_bits(0, code2, lrun.woy);
            }
         } else {
            /*
             * lookup first half of keyword range (week days or months).
             */
            lcase(lc->str);
            for (i = 0; keyw[i].name; i++) {
               if (bstrcmp(lc->str, keyw[i].name)) {
                  state = keyw[i].state;
                  code = keyw[i].code;
                  i = 0;
                  break;
               }
            }
            if (i != 0 || (state != s_month && state != s_wday && state != s_wom)) {
               scan_err0(lc, _("Invalid month, week or position day range"));
               /* NOT REACHED */
            }

            /*
             * Lookup end of range.
             */
            lcase(p);
            for (i = 0; keyw[i].name; i++) {
               if (bstrcmp(p, keyw[i].name)) {
                  state2 = keyw[i].state;
                  code2 = keyw[i].code;
                  i = 0;
                  break;
               }
            }
            if (i != 0 || state != state2 || code == code2) {
               scan_err0(lc, _("Invalid month, weekday or position range"));
               /* NOT REACHED */
            }
            if (state == s_wday) {
               if (!have_wday) {
                  clear_bits(0, 6, lrun.wday);
                  have_wday = true;
               }
               if (code < code2) {
                  set_bits(code, code2, lrun.wday);
               } else {
                  set_bits(code, 6, lrun.wday);
                  set_bits(0, code2, lrun.wday);
               }
            } else if (state == s_month) {
               if (!have_month) {
                  clear_bits(0, 11, lrun.month);
                  have_month = true;
               }
               if (code < code2) {
                  set_bits(code, code2, lrun.month);
               } else {
                  /*
                   * This is a bit odd, but we accept it anyway
                   */
                  set_bits(code, 11, lrun.month);
                  set_bits(0, code2, lrun.month);
               }
            } else {
               /*
                * Must be position
                */
               if (!have_wom) {
                  clear_bits(0, 4, lrun.wom);
                  have_wom = true;
               }
               if (code < code2) {
                  set_bits(code, code2, lrun.wom);
               } else {
                  set_bits(code, 4, lrun.wom);
                  set_bits(0, code2, lrun.wom);
               }
            }
         }
         break;
      case s_hourly:
         have_hour = true;
         set_bits(0, 23, lrun.hour);
         break;
      case s_weekly:
         have_mday = have_wom = have_woy = true;
         set_bits(0, 30, lrun.mday);
         set_bits(0, 4,  lrun.wom);
         set_bits(0, 53, lrun.woy);
         break;
      case s_daily:
         have_mday = true;
         set_bits(0, 6, lrun.wday);
         break;
      case s_monthly:
         have_month = true;
         set_bits(0, 11, lrun.month);
         break;
      default:
         scan_err0(lc, _("Unexpected run state\n"));
         /* NOT REACHED */
         break;
      }
   }

   /* Allocate run record, copy new stuff into it,
    * and append it to the list of run records
    * in the schedule resource.
    */
   if (pass == 2) {
      RUNRES *tail;

      /* Create new run record */
      RUNRES *nrun = (RUNRES *)malloc(sizeof(RUNRES));
      memcpy(nrun, &lrun, sizeof(RUNRES));
      nrun ->next = NULL;

      if (!*run) {                       /* If empty list */
         *run = nrun;                    /* Add new record */
      } else {
         for (tail = *run; tail->next; tail=tail->next)
            {  }
         tail->next = nrun;
      }
   }

   lc->options = options;                /* Restore scanner options */
   set_bit(index, res_all->res_sch.hdr.item_present);
}
