/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
 * Kern Sibbald, May MM
 */
/**
 * @file
 * Configuration parser for Director Run Configuration
 * directives, which are part of the Schedule Resource
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "lib/edit.h"
#include "lib/keyword_table_s.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

namespace directordaemon {

extern struct s_jl joblevels[];

/*
 * Forward referenced subroutines
 */
enum e_state
{
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
  s_wom,   /* 1st, 2nd, ...*/
  s_woy,   /* week of year w00 - w53 */
  s_last,  /* last week of month */
  s_modulo /* every nth monthday/week */
};

struct s_keyw {
  const char* name;   /* keyword */
  enum e_state state; /* parser state */
  int code;           /* state value */
};

/*
 * Keywords understood by parser
 */
static struct s_keyw keyw[] = {{NT_("on"), s_none, 0},
                               {NT_("at"), s_at, 0},
                               {NT_("last"), s_last, 0},
                               {NT_("sun"), s_wday, 0},
                               {NT_("mon"), s_wday, 1},
                               {NT_("tue"), s_wday, 2},
                               {NT_("wed"), s_wday, 3},
                               {NT_("thu"), s_wday, 4},
                               {NT_("fri"), s_wday, 5},
                               {NT_("sat"), s_wday, 6},
                               {NT_("jan"), s_month, 0},
                               {NT_("feb"), s_month, 1},
                               {NT_("mar"), s_month, 2},
                               {NT_("apr"), s_month, 3},
                               {NT_("may"), s_month, 4},
                               {NT_("jun"), s_month, 5},
                               {NT_("jul"), s_month, 6},
                               {NT_("aug"), s_month, 7},
                               {NT_("sep"), s_month, 8},
                               {NT_("oct"), s_month, 9},
                               {NT_("nov"), s_month, 10},
                               {NT_("dec"), s_month, 11},
                               {NT_("sunday"), s_wday, 0},
                               {NT_("monday"), s_wday, 1},
                               {NT_("tuesday"), s_wday, 2},
                               {NT_("wednesday"), s_wday, 3},
                               {NT_("thursday"), s_wday, 4},
                               {NT_("friday"), s_wday, 5},
                               {NT_("saturday"), s_wday, 6},
                               {NT_("january"), s_month, 0},
                               {NT_("february"), s_month, 1},
                               {NT_("march"), s_month, 2},
                               {NT_("april"), s_month, 3},
                               {NT_("june"), s_month, 5},
                               {NT_("july"), s_month, 6},
                               {NT_("august"), s_month, 7},
                               {NT_("september"), s_month, 8},
                               {NT_("october"), s_month, 9},
                               {NT_("november"), s_month, 10},
                               {NT_("december"), s_month, 11},
                               {NT_("daily"), s_daily, 0},
                               {NT_("weekly"), s_weekly, 0},
                               {NT_("monthly"), s_monthly, 0},
                               {NT_("hourly"), s_hourly, 0},
                               {NT_("1st"), s_wom, 0},
                               {NT_("2nd"), s_wom, 1},
                               {NT_("3rd"), s_wom, 2},
                               {NT_("4th"), s_wom, 3},
                               {NT_("5th"), s_wom, 4},
                               {NT_("first"), s_wom, 0},
                               {NT_("second"), s_wom, 1},
                               {NT_("third"), s_wom, 2},
                               {NT_("fourth"), s_wom, 3},
                               {NT_("fifth"), s_wom, 4},
                               {NULL, s_none, 0}};

static bool have_hour, have_mday, have_wday, have_month, have_wom;
static bool have_at, have_woy;
static RunResource* res_run;

static void set_defaults()
{
  have_hour = have_mday = have_wday = have_month = have_wom = have_woy = false;
  have_at = false;
  SetBitRange(0, 23, res_run->date_time_bitfield.hour);
  SetBitRange(0, 30, res_run->date_time_bitfield.mday);
  SetBitRange(0, 6, res_run->date_time_bitfield.wday);
  SetBitRange(0, 11, res_run->date_time_bitfield.month);
  SetBitRange(0, 4, res_run->date_time_bitfield.wom);
  SetBitRange(0, 53, res_run->date_time_bitfield.woy);
}

/**
 * Keywords (RHS) permitted in Run records
 */
struct s_kw RunFields[] = {{"pool", 'P'},
                           {"fullpool", 'f'},
                           {"incrementalpool", 'i'},
                           {"differentialpool", 'd'},
                           {"nextpool", 'n'},
                           {"level", 'L'},
                           {"storage", 'S'},
                           {"messages", 'M'},
                           {"priority", 'p'},
                           {"spooldata", 's'},
                           {"maxrunschedtime", 'm'},
                           {"accurate", 'a'},
                           {NULL, 0}};

/**
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
void StoreRun(LEX* lc, ResourceItem* item, int index, int pass)
{
  char* p;
  int i, j;
  int options = lc->options;
  int token, state, state2 = 0, code = 0, code2 = 0;
  bool found;
  utime_t utime;
  BareosResource* res;

  lc->options |= LOPT_NO_IDENT; /* Want only "strings" */

  /*
   * Clear local copy of run record
   */
  res_run = new RunResource;

  /*
   * Scan for Job level "full", "incremental", ...
   */
  for (found = true; found;) {
    found = false;
    token = LexGetToken(lc, BCT_NAME);
    for (i = 0; !found && RunFields[i].name; i++) {
      if (Bstrcasecmp(lc->str, RunFields[i].name)) {
        found = true;
        if (LexGetToken(lc, BCT_ALL) != BCT_EQUALS) {
          scan_err1(lc, _("Expected an equals, got: %s"), lc->str);
          return;
        }
        switch (RunFields[i].token) {
          case 's': /* Data spooling */
            token = LexGetToken(lc, BCT_NAME);
            if (Bstrcasecmp(lc->str, "yes") || Bstrcasecmp(lc->str, "true")) {
              res_run->spool_data = true;
              res_run->spool_data_set = true;
            } else if (Bstrcasecmp(lc->str, "no") ||
                       Bstrcasecmp(lc->str, "false")) {
              res_run->spool_data = false;
              res_run->spool_data_set = true;
            } else {
              scan_err1(lc, _("Expect a YES or NO, got: %s"), lc->str);
              return;
            }
            break;
          case 'L': /* Level */
            token = LexGetToken(lc, BCT_NAME);
            for (j = 0; joblevels[j].level_name; j++) {
              if (Bstrcasecmp(lc->str, joblevels[j].level_name)) {
                res_run->level = joblevels[j].level;
                res_run->job_type = joblevels[j].job_type;
                j = 0;
                break;
              }
            }
            if (j != 0) {
              scan_err1(lc, _("Job level field: %s not found in run record"),
                        lc->str);
              return;
            }
            break;
          case 'p': /* Priority */
            token = LexGetToken(lc, BCT_PINT32);
            if (pass == 2) { res_run->Priority = lc->u.pint32_val; }
            break;
          case 'P': /* Pool */
          case 'f': /* FullPool */
          case 'v': /* VFullPool */
          case 'i': /* IncPool */
          case 'd': /* DiffPool */
          case 'n': /* NextPool */
            token = LexGetToken(lc, BCT_NAME);
            if (pass == 2) {
              res = my_config->GetResWithName(R_POOL, lc->str);
              if (res == NULL) {
                scan_err1(lc, _("Could not find specified Pool Resource: %s"),
                          lc->str);
                return;
              }
              switch (RunFields[i].token) {
                case 'P':
                  res_run->pool = (PoolResource*)res;
                  break;
                case 'f':
                  res_run->full_pool = (PoolResource*)res;
                  break;
                case 'v':
                  res_run->vfull_pool = (PoolResource*)res;
                  break;
                case 'i':
                  res_run->inc_pool = (PoolResource*)res;
                  break;
                case 'd':
                  res_run->diff_pool = (PoolResource*)res;
                  break;
                case 'n':
                  res_run->next_pool = (PoolResource*)res;
                  break;
              }
            }
            break;
          case 'S': /* Storage */
            token = LexGetToken(lc, BCT_NAME);
            if (pass == 2) {
              res = my_config->GetResWithName(R_STORAGE, lc->str);
              if (res == NULL) {
                scan_err1(lc,
                          _("Could not find specified Storage Resource: %s"),
                          lc->str);
                return;
              }
              res_run->storage = (StorageResource*)res;
            }
            break;
          case 'M': /* Messages */
            token = LexGetToken(lc, BCT_NAME);
            if (pass == 2) {
              res = my_config->GetResWithName(R_MSGS, lc->str);
              if (res == NULL) {
                scan_err1(lc,
                          _("Could not find specified Messages Resource: %s"),
                          lc->str);
                return;
              }
              res_run->msgs = (MessagesResource*)res;
            }
            break;
          case 'm': /* Max run sched time */
            token = LexGetToken(lc, BCT_QUOTED_STRING);
            if (!DurationToUtime(lc->str, &utime)) {
              scan_err1(lc, _("expected a time period, got: %s"), lc->str);
              return;
            }
            res_run->MaxRunSchedTime = utime;
            res_run->MaxRunSchedTime_set = true;
            break;
          case 'a': /* Accurate */
            token = LexGetToken(lc, BCT_NAME);
            if (strcasecmp(lc->str, "yes") == 0 ||
                strcasecmp(lc->str, "true") == 0) {
              res_run->accurate = true;
              res_run->accurate_set = true;
            } else if (strcasecmp(lc->str, "no") == 0 ||
                       strcasecmp(lc->str, "false") == 0) {
              res_run->accurate = false;
              res_run->accurate_set = true;
            } else {
              scan_err1(lc, _("Expect a YES or NO, got: %s"), lc->str);
            }
            break;
          default:
            scan_err1(lc, _("Expected a keyword name, got: %s"), lc->str);
            return;
            break;
        } /* end switch */
      }   /* end if Bstrcasecmp */
    }     /* end for RunFields */

    /*
     * At this point, it is not a keyword. Check for old syle
     * Job Levels without keyword. This form is depreciated!!!
     */
    if (!found) {
      for (j = 0; joblevels[j].level_name; j++) {
        if (Bstrcasecmp(lc->str, joblevels[j].level_name)) {
          res_run->level = joblevels[j].level;
          res_run->job_type = joblevels[j].job_type;
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

  for (; token != BCT_EOL; (token = LexGetToken(lc, BCT_ALL))) {
    int len;
    bool pm = false;
    bool am = false;
    switch (token) {
      case BCT_NUMBER:
        state = s_mday;
        code = atoi(lc->str) - 1;
        if (code < 0 || code > 30) {
          scan_err0(lc, _("Day number out of range (1-31)"));
          return;
        }
        break;
      case BCT_NAME: /* This handles drop through from keyword */
      case BCT_UNQUOTED_STRING:
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
            IsAnInteger(lc->str + 1)) {
          code = atoi(lc->str + 1);
          if (code < 0 || code > 53) {
            scan_err0(lc, _("Week number out of range (0-53)"));
            return;
          }
          state = s_woy; /* Week of year */
          break;
        }
        /*
         * Everything else must be a keyword
         */
        for (i = 0; keyw[i].name; i++) {
          if (Bstrcasecmp(lc->str, keyw[i].name)) {
            state = keyw[i].state;
            code = keyw[i].code;
            i = 0;
            break;
          }
        }
        if (i != 0) {
          scan_err1(lc, _("Job type field: %s in run record not found"),
                    lc->str);
          return;
        }
        break;
      case BCT_COMMA:
        continue;
      default:
        scan_err2(lc, _("Unexpected token: %d:%s"), token, lc->str);
        return;
        break;
    }
    switch (state) {
      case s_none:
        continue;
      case s_mday: /* Day of month */
        if (!have_mday) {
          ClearBitRange(0, 30, res_run->date_time_bitfield.mday);
          have_mday = true;
        }
        SetBit(code, res_run->date_time_bitfield.mday);
        break;
      case s_month: /* Month of year */
        if (!have_month) {
          ClearBitRange(0, 11, res_run->date_time_bitfield.month);
          have_month = true;
        }
        SetBit(code, res_run->date_time_bitfield.month);
        break;
      case s_wday: /* Week day */
        if (!have_wday) {
          ClearBitRange(0, 6, res_run->date_time_bitfield.wday);
          have_wday = true;
        }
        SetBit(code, res_run->date_time_bitfield.wday);
        break;
      case s_wom: /* Week of month 1st, ... */
        if (!have_wom) {
          ClearBitRange(0, 4, res_run->date_time_bitfield.wom);
          have_wom = true;
        }
        SetBit(code, res_run->date_time_bitfield.wom);
        break;
      case s_woy:
        if (!have_woy) {
          ClearBitRange(0, 53, res_run->date_time_bitfield.woy);
          have_woy = true;
        }
        SetBit(code, res_run->date_time_bitfield.woy);
        break;
      case s_time: /* Time */
        if (!have_at) {
          scan_err0(lc, _("Time must be preceded by keyword AT."));
          return;
        }
        if (!have_hour) {
          ClearBitRange(0, 23, res_run->date_time_bitfield.hour);
        }
        //       Dmsg1(000, "s_time=%s\n", lc->str);
        p = strchr(lc->str, ':');
        if (!p) {
          scan_err0(lc, _("Time logic error.\n"));
          return;
        }
        *p++ = 0;             /* Separate two halves */
        code = atoi(lc->str); /* Pick up hour */
        code2 = atoi(p);      /* Pick up minutes */
        len = strlen(p);
        if (len >= 2) { p += 2; }
        if (Bstrcasecmp(p, "pm")) {
          pm = true;
        } else if (Bstrcasecmp(p, "am")) {
          am = true;
        } else if (len != 2) {
          scan_err0(lc, _("Bad time specification."));
          return;
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
          if (code != 12) { code += 12; }
        } else if (am && code == 12) {
          /*
           * AM
           */
          code -= 12;
        }
        if (code < 0 || code > 23 || code2 < 0 || code2 > 59) {
          scan_err0(lc, _("Bad time specification."));
          return;
        }
        SetBit(code, res_run->date_time_bitfield.hour);
        res_run->minute = code2;
        have_hour = true;
        break;
      case s_at:
        have_at = true;
        break;
      case s_last:
        res_run->date_time_bitfield.last_week_of_month = true;
        if (!have_wom) {
          ClearBitRange(0, 4, res_run->date_time_bitfield.wom);
          have_wom = true;
        }
        break;
      case s_modulo:
        p = strchr(lc->str, '/');
        if (!p) {
          scan_err0(lc, _("Modulo logic error.\n"));
          return;
        }
        *p++ = 0; /* Separate two halves */

        if (IsAnInteger(lc->str) && IsAnInteger(p)) {
          /*
           * Check for day modulo specification.
           */
          code = atoi(lc->str) - 1;
          code2 = atoi(p);
          if (code < 0 || code > 30 || code2 < 0 || code2 > 30) {
            scan_err0(lc, _("Bad day specification in modulo."));
            return;
          }
          if (code > code2) {
            scan_err0(lc, _("Bad day specification, offset must always be <= "
                            "than modulo."));
            return;
          }
          if (!have_mday) {
            ClearBitRange(0, 30, res_run->date_time_bitfield.mday);
            have_mday = true;
          }
          /*
           * Set the bits according to the modulo specification.
           */
          for (i = 0; i < 31; i++) {
            if (i % code2 == 0) {
              SetBit(i + code, res_run->date_time_bitfield.mday);
            }
          }
        } else if (strlen(lc->str) == 3 && strlen(p) == 3 &&
                   (lc->str[0] == 'w' || lc->str[0] == 'W') &&
                   (p[0] == 'w' || p[0] == 'W') && IsAnInteger(lc->str + 1) &&
                   IsAnInteger(p + 1)) {
          /*
           * Check for week modulo specification.
           */
          code = atoi(lc->str + 1);
          code2 = atoi(p + 1);
          if (code < 0 || code > 53 || code2 < 0 || code2 > 53) {
            scan_err0(lc, _("Week number out of range (0-53) in modulo"));
            return;
          }
          if (code > code2) {
            scan_err0(lc, _("Bad week number specification in modulo, offset "
                            "must always be <= than modulo."));
            return;
          }
          if (!have_woy) {
            ClearBitRange(0, 53, res_run->date_time_bitfield.woy);
            have_woy = true;
          }
          /*
           * Set the bits according to the modulo specification.
           */
          for (i = 0; i < 54; i++) {
            if (i % code2 == 0) {
              SetBit(i + code - 1, res_run->date_time_bitfield.woy);
            }
          }
        } else {
          scan_err0(lc, _("Bad modulo time specification. Format for weekdays "
                          "is '01/02', for yearweeks is 'w01/w02'."));
          return;
        }
        break;
      case s_range:
        p = strchr(lc->str, '-');
        if (!p) {
          scan_err0(lc, _("Range logic error.\n"));
          return;
        }
        *p++ = 0; /* Separate two halves */

        if (IsAnInteger(lc->str) && IsAnInteger(p)) {
          /*
           * Check for day range.
           */
          code = atoi(lc->str) - 1;
          code2 = atoi(p) - 1;
          if (code < 0 || code > 30 || code2 < 0 || code2 > 30) {
            scan_err0(lc, _("Bad day range specification."));
            return;
          }
          if (!have_mday) {
            ClearBitRange(0, 30, res_run->date_time_bitfield.mday);
            have_mday = true;
          }
          if (code < code2) {
            SetBitRange(code, code2, res_run->date_time_bitfield.mday);
          } else {
            SetBitRange(code, 30, res_run->date_time_bitfield.mday);
            SetBitRange(0, code2, res_run->date_time_bitfield.mday);
          }
        } else if (strlen(lc->str) == 3 && strlen(p) == 3 &&
                   (lc->str[0] == 'w' || lc->str[0] == 'W') &&
                   (p[0] == 'w' || p[0] == 'W') && IsAnInteger(lc->str + 1) &&
                   IsAnInteger(p + 1)) {
          /*
           * Check for week of year range.
           */
          code = atoi(lc->str + 1);
          code2 = atoi(p + 1);
          if (code < 0 || code > 53 || code2 < 0 || code2 > 53) {
            scan_err0(lc, _("Week number out of range (0-53)"));
            return;
          }
          if (!have_woy) {
            ClearBitRange(0, 53, res_run->date_time_bitfield.woy);
            have_woy = true;
          }
          if (code < code2) {
            SetBitRange(code, code2, res_run->date_time_bitfield.woy);
          } else {
            SetBitRange(code, 53, res_run->date_time_bitfield.woy);
            SetBitRange(0, code2, res_run->date_time_bitfield.woy);
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
          if (i != 0 ||
              (state != s_month && state != s_wday && state != s_wom)) {
            scan_err0(lc, _("Invalid month, week or position day range"));
            return;
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
            return;
          }
          if (state == s_wday) {
            if (!have_wday) {
              ClearBitRange(0, 6, res_run->date_time_bitfield.wday);
              have_wday = true;
            }
            if (code < code2) {
              SetBitRange(code, code2, res_run->date_time_bitfield.wday);
            } else {
              SetBitRange(code, 6, res_run->date_time_bitfield.wday);
              SetBitRange(0, code2, res_run->date_time_bitfield.wday);
            }
          } else if (state == s_month) {
            if (!have_month) {
              ClearBitRange(0, 11, res_run->date_time_bitfield.month);
              have_month = true;
            }
            if (code < code2) {
              SetBitRange(code, code2, res_run->date_time_bitfield.month);
            } else {
              /*
               * This is a bit odd, but we accept it anyway
               */
              SetBitRange(code, 11, res_run->date_time_bitfield.month);
              SetBitRange(0, code2, res_run->date_time_bitfield.month);
            }
          } else {
            /*
             * Must be position
             */
            if (!have_wom) {
              ClearBitRange(0, 4, res_run->date_time_bitfield.wom);
              have_wom = true;
            }
            if (code < code2) {
              SetBitRange(code, code2, res_run->date_time_bitfield.wom);
            } else {
              SetBitRange(code, 4, res_run->date_time_bitfield.wom);
              SetBitRange(0, code2, res_run->date_time_bitfield.wom);
            }
          }
        }
        break;
      case s_hourly:
        have_hour = true;
        SetBitRange(0, 23, res_run->date_time_bitfield.hour);
        break;
      case s_weekly:
        have_mday = have_wom = have_woy = true;
        SetBitRange(0, 30, res_run->date_time_bitfield.mday);
        SetBitRange(0, 4, res_run->date_time_bitfield.wom);
        SetBitRange(0, 53, res_run->date_time_bitfield.woy);
        break;
      case s_daily:
        have_mday = true;
        SetBitRange(0, 6, res_run->date_time_bitfield.wday);
        break;
      case s_monthly:
        have_month = true;
        SetBitRange(0, 11, res_run->date_time_bitfield.month);
        break;
      default:
        scan_err0(lc, _("Unexpected run state\n"));
        return;
        break;
    }
  }

  /* Allocate run record, copy new stuff into it,
   * and append it to the list of run records
   * in the schedule resource.
   */
  if (pass == 2) {
    RunResource** run = GetItemVariablePointer<RunResource**>(*item);
    RunResource* tail;

    RunResource* nrun = res_run;
    res_run = nullptr;

    nrun->next = NULL;

    if (!*run) {   /* If empty list */
      *run = nrun; /* Add new record */
    } else {
      for (tail = *run; tail->next; tail = tail->next) {}
      tail->next = nrun;
    }
  }

  lc->options = options; /* Restore scanner options */
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}
} /* namespace directordaemon */
