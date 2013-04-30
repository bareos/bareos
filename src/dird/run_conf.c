/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
 *
 *  Configuration parser for Director Run Configuration
 *   directives, which are part of the Schedule Resource
 *
 *     Kern Sibbald, May MM
 *
 */

#include "bacula.h"
#include "dird.h"

#if defined(_MSC_VER)
extern "C" { // work around visual compiler mangling variables
   extern URES res_all;
}
#else
extern URES res_all;
#endif
extern struct s_jl joblevels[];

/* Forward referenced subroutines */

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
   s_wom,                           /* 1st, 2nd, ...*/
   s_woy                            /* week of year w00 - w53 */
};

struct s_keyw {
  const char *name;                   /* keyword */
  enum e_state state;                 /* parser state */
  int code;                           /* state value */
};

/* Keywords understood by parser */
static struct s_keyw keyw[] = {
  {NT_("on"),         s_none,    0},
  {NT_("at"),         s_at,      0},

  {NT_("sun"),        s_wday,    0},
  {NT_("mon"),        s_wday,    1},
  {NT_("tue"),        s_wday,    2},
  {NT_("wed"),        s_wday,    3},
  {NT_("thu"),        s_wday,    4},
  {NT_("fri"),        s_wday,    5},
  {NT_("sat"),        s_wday,    6},
  {NT_("jan"),        s_month,   0},
  {NT_("feb"),        s_month,   1},
  {NT_("mar"),        s_month,   2},
  {NT_("apr"),        s_month,   3},
  {NT_("may"),        s_month,   4},
  {NT_("jun"),        s_month,   5},
  {NT_("jul"),        s_month,   6},
  {NT_("aug"),        s_month,   7},
  {NT_("sep"),        s_month,   8},
  {NT_("oct"),        s_month,   9},
  {NT_("nov"),        s_month,  10},
  {NT_("dec"),        s_month,  11},

  {NT_("sunday"),     s_wday,    0},
  {NT_("monday"),     s_wday,    1},
  {NT_("tuesday"),    s_wday,    2},
  {NT_("wednesday"),  s_wday,    3},
  {NT_("thursday"),   s_wday,    4},
  {NT_("friday"),     s_wday,    5},
  {NT_("saturday"),   s_wday,    6},
  {NT_("january"),    s_month,   0},
  {NT_("february"),   s_month,   1},
  {NT_("march"),      s_month,   2},
  {NT_("april"),      s_month,   3},
  {NT_("june"),       s_month,   5},
  {NT_("july"),       s_month,   6},
  {NT_("august"),     s_month,   7},
  {NT_("september"),  s_month,   8},
  {NT_("october"),    s_month,   9},
  {NT_("november"),   s_month,  10},
  {NT_("december"),   s_month,  11},

  {NT_("daily"),      s_daily,   0},
  {NT_("weekly"),     s_weekly,  0},
  {NT_("monthly"),    s_monthly, 0},
  {NT_("hourly"),     s_hourly,  0},

  {NT_("1st"),        s_wom,     0},
  {NT_("2nd"),        s_wom,     1},
  {NT_("3rd"),        s_wom,     2},
  {NT_("4th"),        s_wom,     3},
  {NT_("5th"),        s_wom,     4},

  {NT_("first"),      s_wom,     0},
  {NT_("second"),     s_wom,     1},
  {NT_("third"),      s_wom,     2},
  {NT_("fourth"),     s_wom,     3},
  {NT_("fifth"),      s_wom,     4},
  {NULL,         s_none,    0}
};

static bool have_hour, have_mday, have_wday, have_month, have_wom;
static bool have_at, have_woy;
static RUN lrun;

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


/* Keywords (RHS) permitted in Run records */
static struct s_kw RunFields[] = {
   {"pool",              'P'},
   {"fullpool",          'f'},
   {"incrementalpool",   'i'},
   {"differentialpool",  'd'},
   {"level",             'L'},
   {"storage",           'S'},
   {"messages",          'M'},
   {"priority",          'p'},
   {"spooldata",         's'},
   {"writepartafterjob", 'W'},
   {"maxrunschedtime",   'm'},
   {"accurate",          'a'},
   {NULL,                 0}
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
   int i, j;
   bool found;
   utime_t utime;
   int token, state, state2 = 0, code = 0, code2 = 0;
   int options = lc->options;
   RUN **run = (RUN **)(item->value);
   char *p;
   RES *res;


   lc->options |= LOPT_NO_IDENT;      /* want only "strings" */

   /* clear local copy of run record */
   memset(&lrun, 0, sizeof(RUN));

   /* scan for Job level "full", "incremental", ... */
   for (found=true; found; ) {
      found = false;
      token = lex_get_token(lc, T_NAME);
      for (i=0; !found && RunFields[i].name; i++) {
         if (strcasecmp(lc->str, RunFields[i].name) == 0) {
            found = true;
            if (lex_get_token(lc, T_ALL) != T_EQUALS) {
               scan_err1(lc, _("Expected an equals, got: %s"), lc->str);
               /* NOT REACHED */
            }
            switch (RunFields[i].token) {
            case 's':                 /* Data spooling */
               token = lex_get_token(lc, T_NAME);
               if (strcasecmp(lc->str, "yes") == 0 || strcasecmp(lc->str, "true") == 0) {
                  lrun.spool_data = true;
                  lrun.spool_data_set = true;
               } else if (strcasecmp(lc->str, "no") == 0 || strcasecmp(lc->str, "false") == 0) {
                  lrun.spool_data = false;
                  lrun.spool_data_set = true;
               } else {
                  scan_err1(lc, _("Expect a YES or NO, got: %s"), lc->str);
               }
               break;
            case 'W':                 /* Write part after job */
               token = lex_get_token(lc, T_NAME);
               if (strcasecmp(lc->str, "yes") == 0 || strcasecmp(lc->str, "true") == 0) {
                  lrun.write_part_after_job = true;
                  lrun.write_part_after_job_set = true;
               } else if (strcasecmp(lc->str, "no") == 0 || strcasecmp(lc->str, "false") == 0) {
                  lrun.write_part_after_job = false;
                  lrun.write_part_after_job_set = true;
               } else {
                  scan_err1(lc, _("Expect a YES or NO, got: %s"), lc->str);
               }
               break;
            case 'L':                 /* level */
               token = lex_get_token(lc, T_NAME);
               for (j=0; joblevels[j].level_name; j++) {
                  if (strcasecmp(lc->str, joblevels[j].level_name) == 0) {
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
            case 'd':                 /* DifPool */
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
                     lrun.pool = (POOL *)res;
                     break;
                  case 'f':
                     lrun.full_pool = (POOL *)res;
                     break;
                  case 'i':
                     lrun.inc_pool = (POOL *)res;
                     break;
                  case 'd':
                     lrun.diff_pool = (POOL *)res;
                     break;
                  }
               }
               break;
            case 'S':                 /* storage */
               token = lex_get_token(lc, T_NAME);
               if (pass == 2) {
                  res = GetResWithName(R_STORAGE, lc->str);
                  if (res == NULL) {
                     scan_err1(lc, _("Could not find specified Storage Resource: %s"),
                                lc->str);
                     /* NOT REACHED */
                  }
                  lrun.storage = (STORE *)res;
               }
               break;
            case 'M':                 /* messages */
               token = lex_get_token(lc, T_NAME);
               if (pass == 2) {
                  res = GetResWithName(R_MSGS, lc->str);
                  if (res == NULL) {
                     scan_err1(lc, _("Could not find specified Messages Resource: %s"),
                                lc->str);
                     /* NOT REACHED */
                  }
                  lrun.msgs = (MSGS *)res;
               }
               break;
            case 'm':           /* max run sched time */
               token = lex_get_token(lc, T_QUOTED_STRING); 
               if (!duration_to_utime(lc->str, &utime)) {
                  scan_err1(lc, _("expected a time period, got: %s"), lc->str);
                  return;
               }
               lrun.MaxRunSchedTime = utime;
               lrun.MaxRunSchedTime_set = true;
               break;
            case 'a':           /* accurate */
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
         } /* end if strcasecmp */
      } /* end for RunFields */

      /* At this point, it is not a keyword. Check for old syle
       * Job Levels without keyword. This form is depreciated!!!
       */
      if (!found) {
         for (j=0; joblevels[j].level_name; j++) {
            if (strcasecmp(lc->str, joblevels[j].level_name) == 0) {
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

   for ( ; token != T_EOL; (token = lex_get_token(lc, T_ALL))) {
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
      case T_NAME:                 /* this handles drop through from keyword */
      case T_UNQUOTED_STRING:
         if (strchr(lc->str, (int)'-')) {
            state = s_range;
            break;
         }
         if (strchr(lc->str, (int)':')) {
            state = s_time;
            break;
         }
         if (lc->str_len == 3 && (lc->str[0] == 'w' || lc->str[0] == 'W') &&
             is_an_integer(lc->str+1)) {
            code = atoi(lc->str+1);
            if (code < 0 || code > 53) {
               scan_err0(lc, _("Week number out of range (0-53)"));
              /* NOT REACHED */
            }
            state = s_woy;            /* week of year */
            break;
         }
         /* everything else must be a keyword */
         for (i=0; keyw[i].name; i++) {
            if (strcasecmp(lc->str, keyw[i].name) == 0) {
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
      case s_mday:                 /* day of month */
         if (!have_mday) {
            clear_bits(0, 30, lrun.mday);
            have_mday = true;
         }
         set_bit(code, lrun.mday);
         break;
      case s_month:                /* month of year */
         if (!have_month) {
            clear_bits(0, 11, lrun.month);
            have_month = true;
         }
         set_bit(code, lrun.month);
         break;
      case s_wday:                 /* week day */
         if (!have_wday) {
            clear_bits(0, 6, lrun.wday);
            have_wday = true;
         }
         set_bit(code, lrun.wday);
         break;
      case s_wom:                  /* Week of month 1st, ... */
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
      case s_time:                 /* time */
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
         *p++ = 0;                 /* separate two halves */
         code = atoi(lc->str);     /* pick up hour */
         code2 = atoi(p);          /* pick up minutes */
         len = strlen(p);
         if (len >= 2) {
            p += 2;
         }
         if (strcasecmp(p, "pm") == 0) {
            pm = true;
         } else if (strcasecmp(p, "am") == 0) {
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
            /* Convert to 24 hour time */
            if (code != 12) {
               code += 12;
            }
         /* am */
         } else if (am && code == 12) {
            code -= 12;
         }
         if (code < 0 || code > 23 || code2 < 0 || code2 > 59) {
            scan_err0(lc, _("Bad time specification."));
            /* NOT REACHED */
         }
//       Dmsg2(000, "hour=%d min=%d\n", code, code2);
         set_bit(code, lrun.hour);
         lrun.minute = code2;
         have_hour = true;
         break;
      case s_at:
         have_at = true;
         break;
      case s_range:
         p = strchr(lc->str, '-');
         if (!p) {
            scan_err0(lc, _("Range logic error.\n"));
         }
         *p++ = 0;                 /* separate two halves */

         /* Check for day range */
         if (is_an_integer(lc->str) && is_an_integer(p)) {
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
            break;
         }
         /* Check for week of year range */
         if (strlen(lc->str) == 3 && strlen(p) == 3 &&
             (lc->str[0] == 'w' || lc->str[0] == 'W') &&
             (p[0] == 'w' || p[0] == 'W') &&
             is_an_integer(lc->str+1) && is_an_integer(p+1)) {
            code = atoi(lc->str+1);
            code2 = atoi(p+1);
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
            break;
         }
         /* lookup first half of keyword range (week days or months) */
         lcase(lc->str);
         for (i=0; keyw[i].name; i++) {
            if (strcmp(lc->str, keyw[i].name) == 0) {
               state = keyw[i].state;
               code   = keyw[i].code;
               i = 0;
               break;
            }
         }
         if (i != 0 || (state != s_month && state != s_wday && state != s_wom)) {
            scan_err0(lc, _("Invalid month, week or position day range"));
            /* NOT REACHED */
         }

         /* Lookup end of range */
         lcase(p);
         for (i=0; keyw[i].name; i++) {
            if (strcmp(p, keyw[i].name) == 0) {
               state2  = keyw[i].state;
               code2   = keyw[i].code;
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
               /* this is a bit odd, but we accept it anyway */
               set_bits(code, 11, lrun.month);
               set_bits(0, code2, lrun.month);
            }
         } else {
            /* Must be position */
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
      RUN *tail;

      /* Create new run record */
      RUN *nrun = (RUN *)malloc(sizeof(RUN));
      memcpy(nrun, &lrun, sizeof(RUN));
      nrun ->next = NULL;

      if (!*run) {                    /* if empty list */
         *run = nrun;                 /* add new record */
      } else {
         for (tail = *run; tail->next; tail=tail->next)
            {  }
         tail->next = nrun;
      }
   }

   lc->options = options;             /* restore scanner options */
   set_bit(index, res_all.res_sch.hdr.item_present);
}
