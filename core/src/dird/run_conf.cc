/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, May MM
/**
 * @file
 * Configuration parser for Director Run Configuration
 * directives, which are part of the Schedule Resource
 */

#include "include/bareos.h"
#include "dird.h"
#include "lib/edit.h"
#include "lib/keyword_table_s.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#include "job_levels.h"

namespace directordaemon {

// Forward referenced subroutines
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

// Keywords understood by parser
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

static void set_defaults(RunResource& res_run)
{
  have_hour = have_mday = have_wday = have_month = have_wom = have_woy = false;
  have_at = false;
  SetBitRange(0, 23, res_run.date_time_mask.hour);
  SetBitRange(0, 30, res_run.date_time_mask.mday);
  SetBitRange(0, 6, res_run.date_time_mask.wday);
  SetBitRange(0, 11, res_run.date_time_mask.month);
  SetBitRange(0, 4, res_run.date_time_mask.wom);
  SetBitRange(0, 53, res_run.date_time_mask.woy);
}

// Keywords (RHS) permitted in Run records
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
void StoreRun(ConfigurationParser* conf,
              lexer* lc,
              const ResourceItem* item,
              int index,
              int pass)
{
  int i, j;
  auto options = lc->options;
  int token, state, state2 = 0, code = 0, code2 = 0;
  bool found;
  utime_t utime;
  BareosResource* res;
  RunResource res_run;

  lc->options.set(lexer::options::NoIdent); /* Want only "strings" */

  conf->PushMergeArray();
  conf->PushObject();
  // Scan for Job level "full", "incremental", ...
  for (found = true; found;) {
    found = false;
    token = LexGetToken(lc, BCT_NAME);
    for (i = 0; !found && RunFields[i].name; i++) {
      if (Bstrcasecmp(lc->str(), RunFields[i].name)) {
        conf->PushLabel(RunFields[i].name);
        found = true;
        if (LexGetToken(lc, BCT_ALL) != BCT_EQUALS) {
          scan_err(lc, T_("Expected an equals, got: %s"), lc->str());
          return;
        }
        switch (RunFields[i].token) {
          case 's': /* Data spooling */
            if (auto* parsed = ReadKeyword(conf, lc, bool_kw)) {
              res_run.spool_data = parsed->token != 0;
              res_run.spool_data_set = true;
            } else {
              scan_err(lc, T_("Expect a YES or NO, got: %s"), lc->str());
            }
            break;
          case 'L': /* Level */
            if (auto* parsed = ReadKeyword(conf, lc, joblevels)) {
              res_run.level = parsed->level;
              res_run.job_type = parsed->job_type;
            } else {
              scan_err(lc, T_("Job level field: %s not found in run record"),
                       lc->str());
              return;
            }
            break;
          case 'p': /* Priority */
            token = LexGetToken(lc, BCT_PINT32);
            conf->PushU(lc->u.pint32_val);
            if (pass == 2) { res_run.Priority = lc->u.pint32_val; }
            break;
          case 'P': /* Pool */
          case 'f': /* FullPool */
          case 'v': /* VFullPool */
          case 'i': /* IncPool */
          case 'd': /* DiffPool */
          case 'n': /* NextPool */

            token = LexGetToken(lc, BCT_NAME);
            conf->PushString(lc->str());
            if (pass == 2) {
              res = conf->GetResWithName(R_POOL, lc->str());
              if (res == NULL) {
                scan_err(lc, T_("Could not find specified Pool Resource: %s"),
                         lc->str());
                return;
              }
              switch (RunFields[i].token) {
                case 'P':
                  res_run.pool = (PoolResource*)res;
                  break;
                case 'f':
                  res_run.full_pool = (PoolResource*)res;
                  break;
                case 'v':
                  res_run.vfull_pool = (PoolResource*)res;
                  break;
                case 'i':
                  res_run.inc_pool = (PoolResource*)res;
                  break;
                case 'd':
                  res_run.diff_pool = (PoolResource*)res;
                  break;
                case 'n':
                  res_run.next_pool = (PoolResource*)res;
                  break;
              }
            }
            break;
          case 'S': /* Storage */
            token = LexGetToken(lc, BCT_NAME);
            conf->PushString(lc->str());
            if (pass == 2) {
              res = conf->GetResWithName(R_STORAGE, lc->str());
              if (res == NULL) {
                scan_err(lc,
                         T_("Could not find specified Storage Resource: %s"),
                         lc->str());
                return;
              }
              res_run.storage = (StorageResource*)res;
            }
            break;
          case 'M': /* Messages */
            token = LexGetToken(lc, BCT_NAME);
            conf->PushString(lc->str());
            if (pass == 2) {
              res = conf->GetResWithName(R_MSGS, lc->str());
              if (res == NULL) {
                scan_err(lc,
                         T_("Could not find specified Messages Resource: %s"),
                         lc->str());
                return;
              }
              res_run.msgs = (MessagesResource*)res;
            }
            break;
          case 'm': /* Max run sched time */
            token = LexGetToken(lc, BCT_QUOTED_STRING);
            conf->PushString(lc->str());
            if (!DurationToUtime(lc->str(), &utime)) {
              scan_err(lc, T_("expected a time period, got: %s"), lc->str());
              return;
            }
            res_run.MaxRunSchedTime = utime;
            res_run.MaxRunSchedTime_set = true;
            break;
          case 'a': /* Accurate */
            token = LexGetToken(lc, BCT_NAME);
            if (strcasecmp(lc->str(), "yes") == 0
                || strcasecmp(lc->str(), "true") == 0) {
              res_run.accurate = true;
              res_run.accurate_set = true;
              conf->PushB(true);
            } else if (strcasecmp(lc->str(), "no") == 0
                       || strcasecmp(lc->str(), "false") == 0) {
              res_run.accurate = false;
              res_run.accurate_set = true;
              conf->PushB(false);
            } else {
              scan_err(lc, T_("Expect a YES or NO, got: %s"), lc->str());
            }
            break;
          default:
            scan_err(lc, T_("Expected a keyword name, got: %s"), lc->str());
            return;
            break;
        } /* end switch */
      } /* end if Bstrcasecmp */
    } /* end for RunFields */

    /* At this point, it is not a keyword. Check for old syle
     * Job Levels without keyword. This form is depreciated!!! */
    if (!found) {
      for (j = 0; joblevels[j].name; j++) {
        if (Bstrcasecmp(lc->str(), joblevels[j].name)) {
          conf->PushLabel("Level");
          conf->PushString(lc->str());

          res_run.level = joblevels[j].level;
          res_run.job_type = joblevels[j].job_type;
          found = true;
          break;
        }
      }
    }
  } /* end for found */

  /* Scan schedule times.
   * Default is: daily at 0:0 */
  state = s_none;
  set_defaults(res_run);

  conf->PushLabel("times");
  conf->PushArray();
  for (; token != BCT_EOL; (token = LexGetToken(lc, BCT_ALL))) {
    int len;
    bool pm = false;
    bool am = false;
    conf->PushString(lc->str());
    switch (token) {
      case BCT_NUMBER:
        state = s_mday;
        code = atoi(lc->str()) - 1;
        if (code < 0 || code > 30) {
          scan_err(lc, T_("Day number out of range (1-31)"));
          return;
        }
        break;
      case BCT_NAME: /* This handles drop through from keyword */
      case BCT_UNQUOTED_STRING:
        if (strchr(lc->str(), (int)'-')) {
          state = s_range;
          break;
        }
        if (strchr(lc->str(), (int)':')) {
          state = s_time;
          break;
        }
        if (strchr(lc->str(), (int)'/')) {
          state = s_modulo;
          break;
        }
        if (lc->str_len() == 3 && (lc->str()[0] == 'w' || lc->str()[0] == 'W')
            && IsAnInteger(lc->str() + 1)) {
          code = atoi(lc->str() + 1);
          if (code < 0 || code > 53) {
            scan_err(lc, T_("Week number out of range (0-53)"));
            return;
          }
          state = s_woy; /* Week of year */
          break;
        }
        // Everything else must be a keyword
        for (i = 0; keyw[i].name; i++) {
          if (Bstrcasecmp(lc->str(), keyw[i].name)) {
            state = keyw[i].state;
            code = keyw[i].code;
            i = 0;
            break;
          }
        }
        if (i != 0) {
          scan_err(lc, T_("Job type field: %s in run record not found"),
                   lc->str());
          return;
        }
        break;
      case BCT_COMMA:
        continue;
      default:
        scan_err(lc, T_("Unexpected token: %d:%s"), token, lc->str());
        return;
        break;
    }
    switch (state) {
      case s_none:
        continue;
      case s_mday: /* Day of month */
        if (!have_mday) {
          ClearBitRange(0, 30, res_run.date_time_mask.mday);
          have_mday = true;
        }
        SetBit(code, res_run.date_time_mask.mday);
        break;
      case s_month: /* Month of year */
        if (!have_month) {
          ClearBitRange(0, 11, res_run.date_time_mask.month);
          have_month = true;
        }
        SetBit(code, res_run.date_time_mask.month);
        break;
      case s_wday: /* Week day */
        if (!have_wday) {
          ClearBitRange(0, 6, res_run.date_time_mask.wday);
          have_wday = true;
        }
        SetBit(code, res_run.date_time_mask.wday);
        break;
      case s_wom: /* Week of month 1st, ... */
        if (!have_wom) {
          ClearBitRange(0, 4, res_run.date_time_mask.wom);
          have_wom = true;
        }
        SetBit(code, res_run.date_time_mask.wom);
        break;
      case s_woy:
        if (!have_woy) {
          ClearBitRange(0, 53, res_run.date_time_mask.woy);
          have_woy = true;
        }
        SetBit(code, res_run.date_time_mask.woy);
        break;
      case s_time: /* Time */ {
        if (!have_at) {
          scan_err(lc, T_("Time must be preceded by keyword AT."));
          return;
        }
        if (!have_hour) { ClearBitRange(0, 23, res_run.date_time_mask.hour); }
        //       Dmsg1(000, "s_time=%s\n", lc->str());
        std::string_view view = lc->str();
        size_t pos = view.find(':');
        if (pos == view.npos) {
          scan_err(lc, T_("Time logic error.\n"));
          return;
        }

        std::from_chars(view.data() + 0, view.data() + pos, code);
        std::from_chars(view.data() + pos + 1, view.data() + view.size(), code);
        const char* p = view.data() + pos + 1;
        len = strlen(p);
        if (len >= 2) { p += 2; }
        if (Bstrcasecmp(p, "pm")) {
          pm = true;
        } else if (Bstrcasecmp(p, "am")) {
          am = true;
        } else if (len != 2) {
          scan_err(lc, T_("Bad time specification."));
          return;
        }
        /* Note, according to NIST, 12am and 12pm are ambiguous and
         *  can be defined to anything.  However, 12:01am is the same
         *  as 00:01 and 12:01pm is the same as 12:01, so we define
         *  12am as 00:00 and 12pm as 12:00. */
        if (pm) {
          // Convert to 24 hour time
          if (code != 12) { code += 12; }
        } else if (am && code == 12) {
          // AM
          code -= 12;
        }
        if (code < 0 || code > 23 || code2 < 0 || code2 > 59) {
          scan_err(lc, T_("Bad time specification."));
          return;
        }
        SetBit(code, res_run.date_time_mask.hour);
        res_run.minute = code2;
        have_hour = true;
      } break;
      case s_at:
        have_at = true;
        break;
      case s_last:
        res_run.date_time_mask.last_7days_of_month = true;
        if (!have_wom) {
          ClearBitRange(0, 4, res_run.date_time_mask.wom);
          have_wom = true;
        }
        break;
      case s_modulo: {
        std::string_view view = lc->str();
        size_t pos = view.find(':');
        if (pos == view.npos) {
          scan_err(lc, T_("Modulo logic error.\n"));
          return;
        }

        auto first = view.substr(0, pos);
        auto second = view.substr(pos + 1);

        if (IsAnInteger(first) && IsAnInteger(second)) {
          // Check for day modulo specification.
          auto first_res = std::from_chars(first.data(),
                                           first.data() + first.size(), code);
          auto second_res = std::from_chars(
              second.data(), second.data() + second.size(), code2);
          if (first_res.ec != std::errc{}
              || first_res.ptr != first.data() + first.size()
              || second_res.ec != std::errc{}
              || second_res.ptr != second.data() + second.size() || code < 0
              || code > 30 || code2 < 0 || code2 > 30) {
            scan_err(lc, T_("Bad day specification in modulo."));
            return;
          }
          if (code > code2) {
            scan_err(lc, T_("Bad day specification, offset must always be <= "
                            "than modulo."));
            return;
          }
          if (!have_mday) {
            ClearBitRange(0, 30, res_run.date_time_mask.mday);
            have_mday = true;
          }
          // Set the bits according to the modulo specification.
          for (i = 0; i < 31; i++) {
            if (i % code2 == 0) {
              SetBit(i + code, res_run.date_time_mask.mday);
            }
          }
        } else if (first.size() == 3 && second.size() == 3
                   && (first[0] == 'w' || first[0] == 'W')
                   && (second[0] == 'w' || second[0] == 'W')
                   && IsAnInteger(first.substr(1))
                   && IsAnInteger(second.substr(1))) {
          // Check for week modulo specification.
          auto first_res = std::from_chars(first.data() + 1,
                                           first.data() + first.size(), code);
          auto second_res = std::from_chars(
              second.data() + 1, second.data() + second.size(), code2);
          if (first_res.ec != std::errc{}
              || first_res.ptr != first.data() + first.size() || code < 0
              || code > 53) {
            scan_err(lc, T_("Week number out of range (0-53) in modulo"));
            return;
          }
          if (second_res.ec != std::errc{}
              || second_res.ptr != second.data() + second.size() || code2 <= 0
              || code2 > 53) {
            scan_err(lc, T_("Week interval out of range (1-53) in modulo"));
            return;
          }
          if (code > code2) {
            scan_err(lc, T_("Bad week number specification in modulo, offset "
                            "must always be <= than modulo."));
            return;
          }
          if (!have_woy) {
            ClearBitRange(0, 53, res_run.date_time_mask.woy);
            have_woy = true;
          }
          // Set the bits according to the modulo specification.
          for (int week = code; week <= 53; week += code2) {
            SetBit(week, res_run.date_time_mask.woy);
          }
        } else {
          scan_err(lc, T_("Bad modulo time specification. Format for weekdays "
                          "is '01/02', for yearweeks is 'w01/w02'."));
          return;
        }
      } break;
      case s_range: {
        std::string_view view = lc->str();
        size_t pos = view.find('-');
        if (pos == view.npos) {
          scan_err(lc, T_("Range logic error.\n"));
          return;
        }

        auto first = view.substr(0, pos);
        auto second = view.substr(pos + 1);

        if (IsAnInteger(first) && IsAnInteger(second)) {
          // Check for day range.
          std::from_chars(first.data(), first.data() + first.size(), code);
          std::from_chars(second.data(), second.data() + second.size(), code2);
          if (code < 0 || code > 30 || code2 < 0 || code2 > 30) {
            scan_err(lc, T_("Bad day range specification."));
            return;
          }
          if (!have_mday) {
            ClearBitRange(0, 30, res_run.date_time_mask.mday);
            have_mday = true;
          }
          if (code < code2) {
            SetBitRange(code, code2, res_run.date_time_mask.mday);
          } else {
            SetBitRange(code, 30, res_run.date_time_mask.mday);
            SetBitRange(0, code2, res_run.date_time_mask.mday);
          }
        } else if (first.size() == 3 && second.size() == 3
                   && (first[0] == 'w' || first[0] == 'W')
                   && (second[0] == 'w' || second[0] == 'W')
                   && IsAnInteger(first.substr(1))
                   && IsAnInteger(second.substr(1))) {
          // Check for week of year range.
          std::from_chars(first.data() + 1, first.data() + first.size(), code);
          std::from_chars(second.data() + 1, second.data() + second.size(),
                          code2);
          if (code < 0 || code > 53 || code2 < 0 || code2 > 53) {
            scan_err(lc, T_("Week number out of range (0-53)"));
            return;
          }
          if (!have_woy) {
            ClearBitRange(0, 53, res_run.date_time_mask.woy);
            have_woy = true;
          }
          if (code < code2) {
            SetBitRange(code, code2, res_run.date_time_mask.woy);
          } else {
            SetBitRange(code, 53, res_run.date_time_mask.woy);
            SetBitRange(0, code2, res_run.date_time_mask.woy);
          }
        } else {
          // lookup first half of keyword range (week days or months).
          for (i = 0; keyw[i].name; i++) {
            if (first.size() == strlen(keyw[i].name)
                && bstrncasecmp(first.data(), keyw[i].name, first.size())) {
              state = keyw[i].state;
              code = keyw[i].code;
              i = 0;
              break;
            }
          }
          if (i != 0
              || (state != s_month && state != s_wday && state != s_wom)) {
            scan_err(lc, T_("Invalid month, week or position day range"));
            return;
          }

          // Lookup end of range.
          for (i = 0; keyw[i].name; i++) {
            if (second.size() == strlen(keyw[i].name)
                && bstrncasecmp(second.data(), keyw[i].name, second.size())) {
              state2 = keyw[i].state;
              code2 = keyw[i].code;
              i = 0;
              break;
            }
          }
          if (i != 0 || state != state2 || code == code2) {
            scan_err(lc, T_("Invalid month, weekday or position range"));
            return;
          }
          if (state == s_wday) {
            if (!have_wday) {
              ClearBitRange(0, 6, res_run.date_time_mask.wday);
              have_wday = true;
            }
            if (code < code2) {
              SetBitRange(code, code2, res_run.date_time_mask.wday);
            } else {
              SetBitRange(code, 6, res_run.date_time_mask.wday);
              SetBitRange(0, code2, res_run.date_time_mask.wday);
            }
          } else if (state == s_month) {
            if (!have_month) {
              ClearBitRange(0, 11, res_run.date_time_mask.month);
              have_month = true;
            }
            if (code < code2) {
              SetBitRange(code, code2, res_run.date_time_mask.month);
            } else {
              // This is a bit odd, but we accept it anyway
              SetBitRange(code, 11, res_run.date_time_mask.month);
              SetBitRange(0, code2, res_run.date_time_mask.month);
            }
          } else {
            // Must be position
            if (!have_wom) {
              ClearBitRange(0, 4, res_run.date_time_mask.wom);
              have_wom = true;
            }
            if (code < code2) {
              SetBitRange(code, code2, res_run.date_time_mask.wom);
            } else {
              SetBitRange(code, 4, res_run.date_time_mask.wom);
              SetBitRange(0, code2, res_run.date_time_mask.wom);
            }
          }
        }
      } break;
      case s_hourly:
        have_hour = true;
        SetBitRange(0, 23, res_run.date_time_mask.hour);
        break;
      case s_weekly:
        have_mday = have_wom = have_woy = true;
        SetBitRange(0, 30, res_run.date_time_mask.mday);
        SetBitRange(0, 4, res_run.date_time_mask.wom);
        SetBitRange(0, 53, res_run.date_time_mask.woy);
        break;
      case s_daily:
        have_mday = true;
        SetBitRange(0, 6, res_run.date_time_mask.wday);
        break;
      case s_monthly:
        have_month = true;
        SetBitRange(0, 11, res_run.date_time_mask.month);
        break;
      default:
        scan_err(lc, T_("Unexpected run state\n"));
        return;
        break;
    }
  }
  conf->PopArray();

  /* Allocate run record, copy new stuff into it,
   * and append it to the list of run records
   * in the schedule resource.
   */
  if (pass == 2) {
    RunResource** run = GetItemVariablePointer<RunResource**>(*item);
    RunResource* tail;

    RunResource* nrun = new RunResource(std::move(res_run));

    nrun->next = NULL;

    if (!*run) {   /* If empty list */
      *run = nrun; /* Add new record */
    } else {
      for (tail = *run; tail->next; tail = tail->next) {}
      tail->next = nrun;
    }
  }

  conf->PopObject();
  conf->PopArray();
  lc->options = options; /* Restore scanner options */
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

void ParseRun(ConfigurationParser* conf,
              lexer* lc,
              const ResourceItem*,
              int,
              int)
{
  auto options = lc->options;
  bool found;
  int token;

  lc->options.set(lexer::options::NoIdent); /* Want only "strings" */

  conf->PushObject();
  // Scan for Job level "full", "incremental", ...
  for (found = true; found;) {
    found = false;
    token = LexGetToken(lc, BCT_NAME);
    for (int i = 0; !found && RunFields[i].name; i++) {
      if (Bstrcasecmp(lc->str(), RunFields[i].name)) {
        conf->PushLabel(RunFields[i].name);
        found = true;
        if (LexGetToken(lc, BCT_ALL) != BCT_EQUALS) {
          scan_err(lc, T_("Expected an equals, got: %s"), lc->str());
          return;
        }
        switch (RunFields[i].token) {
          case 's': /* Data spooling */
            token = LexGetToken(lc, BCT_NAME);
            if (Bstrcasecmp(lc->str(), "yes")
                || Bstrcasecmp(lc->str(), "true")) {
              conf->PushB(true);
            } else if (Bstrcasecmp(lc->str(), "no")
                       || Bstrcasecmp(lc->str(), "false")) {
              conf->PushB(false);
            } else {
              scan_err(lc, T_("Expect a YES or NO, got: %s"), lc->str());
              return;
            }
            break;
          case 'L': /* Level */ {
            token = LexGetToken(lc, BCT_NAME);
            conf->PushString(lc->str());

            bool found_level = false;
            for (int j = 0; joblevels[j].name; j++) {
              if (Bstrcasecmp(lc->str(), joblevels[j].name)) {
                found_level = true;
                break;
              }
            }
            if (!found_level) {
              scan_err(lc, T_("Job level field: %s not found in run record"),
                       lc->str());
              return;
            }
          } break;
          case 'p': /* Priority */
            token = LexGetToken(lc, BCT_PINT32);
            conf->PushU(lc->u.pint32_val);
            break;
          case 'P': /* Pool */
          case 'f': /* FullPool */
          case 'v': /* VFullPool */
          case 'i': /* IncPool */
          case 'd': /* DiffPool */
          case 'n': /* NextPool */
            token = LexGetToken(lc, BCT_NAME);
            conf->PushString(lc->str());
            break;
          case 'S': /* Storage */
            token = LexGetToken(lc, BCT_NAME);
            conf->PushString(lc->str());
            break;
          case 'M': /* Messages */
            token = LexGetToken(lc, BCT_NAME);
            conf->PushString(lc->str());
            break;
          case 'm': /* Max run sched time */
            token = LexGetToken(lc, BCT_QUOTED_STRING);
            conf->PushString(lc->str());
            break;
          case 'a': /* Accurate */
            token = LexGetToken(lc, BCT_NAME);
            if (strcasecmp(lc->str(), "yes") == 0
                || strcasecmp(lc->str(), "true") == 0) {
              conf->PushB(true);
            } else if (strcasecmp(lc->str(), "no") == 0
                       || strcasecmp(lc->str(), "false") == 0) {
              conf->PushB(false);
            } else {
              scan_err(lc, T_("Expect a YES or NO, got: %s"), lc->str());
            }
            break;
          default:
            scan_err(lc, T_("Expected a keyword name, got: %s"), lc->str());
            return;
            break;
        } /* end switch */
      } /* end if Bstrcasecmp */
    } /* end for RunFields */

    /* At this point, it is not a keyword. Check for old syle
     * Job Levels without keyword. This form is depreciated!!! */
    if (!found) {
      for (int j = 0; joblevels[j].name; j++) {
        if (Bstrcasecmp(lc->str(), joblevels[j].name)) {
          conf->PushLabel("Level");
          conf->PushString(lc->str());

          found = true;
          break;
        }
      }
    }
  } /* end for found */

  /* Scan schedule times.
   * Default is: daily at 0:0 */
  conf->PushLabel("times");
  conf->PushArray();
  for (; token != BCT_EOL; (token = LexGetToken(lc, BCT_ALL))) {
    conf->PushString(lc->str());
  }
  conf->PopArray();

  /* Allocate run record, copy new stuff into it,
   * and append it to the list of run records
   * in the schedule resource.
   */
  conf->PopObject();
  lc->options = options; /* Restore scanner options */
}
} /* namespace directordaemon */
