/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
#include "dird/dird_globals.h"
#include "lib/edit.h"
#include "lib/keyword_table_s.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "schedule.h"

namespace directordaemon {

extern struct s_jl joblevels[];

// Deformat
template<class First, class... Rest>
bool Deformat(std::string_view input, std::string_view fmt, First& first, Rest&... rest);

// FromString
// :: int
template<class T, std::enable_if_t<std::is_same_v<T, int>, int> = 0>
std::optional<int> FromString(const std::string& str) {
  try {
    return std::stoi(str);
  }
  catch (...) {
    return std::nullopt;
  }
}
// :: MonthOfYear
template<class T, std::enable_if_t<std::is_same_v<T, MonthOfYear>, int> = 0>
std::optional<MonthOfYear> FromString(const std::string& str) {
  for (size_t i = 0; i < kMonthOfYearLiterals.size(); ++i) {
    bool equals = true;
    for (size_t j = 0; j < kMonthOfYearLiterals[i].length(); ++j) {
      if (kMonthOfYearLiterals[i][j] != std::tolower(str.at(j))) {
        equals = false;
        break;
      }
    }
    if (equals) {
      return MonthOfYear(i);
    }
  }
  return std::nullopt;
}
// :: WeekOfYear
template<class T, std::enable_if_t<std::is_same_v<T, WeekOfYear>, int> = 0>
std::optional<T> FromString(const std::string& str) {
  auto index = FromString<int>(str.substr(1));
  if (index && 0 <= *index && *index <= 53) {
    return T(*index);
  }
  return std::nullopt;
}
// :: WeekOfMonth
template<class T, std::enable_if_t<std::is_same_v<T, WeekOfMonth>, int> = 0>
std::optional<T> FromString(const std::string& str) {
  if (str == "first" || str == "1rd") {
    return WeekOfMonth::kFirst;
  }
  else if (str == "second" || str == "2nd") {
    return WeekOfMonth::kSecond;
  }
  else if (str == "third" || str == "3rd") {
    return WeekOfMonth::kThird;
  }
  else if (str == "fourth" || str == "4th") {
    return WeekOfMonth::kFourth;
  }
  else if (str == "fifth" || str == "5th") {
    return WeekOfMonth::kFifth;
  }
  else if (str == "last") {
    return WeekOfMonth::kLast;
  }
  return std::nullopt;
}
// :: DayOfMonth
template<class T, std::enable_if_t<std::is_same_v<T, DayOfMonth>, int> = 0>
std::optional<DayOfMonth> FromString(const std::string& str) {
  try {
    return DayOfMonth(std::stoi(str));
  }
  catch (...) {
    return std::nullopt;
  }
}
// :: DayOfWeek
template<class T, std::enable_if_t<std::is_same_v<T, DayOfWeek>, int> = 0>
std::optional<T> FromString(const std::string& str) {
  for (size_t i = 0; i < kDayOfWeekLiterals.size(); ++i) {
    bool equals = true;
    for (size_t j = 0; j < kDayOfWeekLiterals[i].length(); ++j) {
      if (kDayOfWeekLiterals[i][j] != std::tolower(str.at(j))) {
        equals = false;
        break;
      }
    }
    if (equals) {
      return DayOfWeek(i);
    }
  }
  return std::nullopt;
}
// :: TimeOfDay
template<class T, std::enable_if_t<std::is_same_v<TimeOfDay, T>, int> = 0>
std::optional<T> FromString(const std::string& str) {
  int hour, minute;
  if (Deformat(str, "at %:%", hour, minute)) {
    return T({ hour, minute });
  }
  else if (Deformat(str, "at %:%am", hour, minute)) {
    return T({ hour, minute });
  }
  else if (Deformat(str, "at %:%pm", hour, minute)) {
    return T({ hour + 12, minute });
  }
  return std::nullopt;
}
// :: Hourly
template<class T, std::enable_if_t<std::is_same_v<Hourly, T>, int> = 0>
std::optional<T> FromString(const std::string& str) {
  return str == "hourly" ? std::optional(Hourly()) : std::nullopt;
}
// :: Range
template<class T, std::enable_if_t<kIsRange<T>, int> = 0>
std::optional<T> FromString(const std::string& str) {
  typename T::Type from, to;
  if constexpr (kFullRangeLiteral<typename T::Type>.length() > 0) {
    if (kFullRangeLiteral<typename T::Type> == str) {
      from = (typename T::Type)0;
      to = (typename T::Type)kMaxValue<typename T::Type>;
      return T({ from, to });
    }
  }
  if (Deformat(str, "%-%", from, to)) {
    return T({ from, to });
  }
  return std::nullopt;
}
// :: Modulo
template<class T, std::enable_if_t<kIsModulo<T>, int> = 0>
std::optional<T> FromString(const std::string& str) {
  typename T::Type left, right;
  if (Deformat(str, "%/%", left, right)) {
    return T({ left, right });
  }
  return std::nullopt;
}
// :: std::variant
template<class T>
static constexpr bool kIsVariant = false;
template<class... Args>
static constexpr bool kIsVariant<std::variant<Args...>> = true;
template<class T, size_t Index = 0, std::enable_if_t<kIsVariant<T>, int> = 0>
std::optional<T> FromString(const std::string& str) {
  if constexpr (Index < std::variant_size_v<T>) {
    if (auto value = FromString<std::variant_alternative_t<Index, T>>(str)) {
      return T(*value);
    }
    else {
      return FromString<T, Index + 1>(str);
    }
  }
  return std::nullopt;
}

// Deformat
// :: base
bool Deformat(std::string_view input, std::string_view fmt) {
  return fmt.find('%') == std::string::npos && input == fmt;
}
// :: recursive
template<class First, class... Rest>
bool Deformat(std::string_view input, std::string_view fmt, First& first, Rest&... rest) {
  if (input.length() < fmt.length()) {
    return false;
  }
  for (size_t i = 0; i < input.length(); ++i) {
    if (input[i] == fmt[i]) {
      continue;
    }
    else if (fmt[i] == '%') {
      size_t end = i;
      while (end < input.length() && (i + 1 == fmt.length() || input[end] != fmt[i + 1])) {
        ++end;
      }
      auto value = FromString<First>(std::string(input.substr(i, end - i)));
      if (value) {
        first = *value;
      }
      else {
        return false;
      }
      return Deformat(input.substr(end), fmt.substr(i + 1), rest...);
    }
    else {
      return false;
    }
  }
  return true;
}

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
void StoreRun(LEX* lc, const ResourceItem* item, int index, int pass)
{
  int i, j;
  int options = lc->options;
  int token, code = 0;
  bool found;
  utime_t utime;
  BareosResource* res;
  RunResource res_run;
  std::vector<std::string> tokens;

  lc->options |= LOPT_NO_IDENT; /* Want only "strings" */

  // Scan for Job level "full", "incremental", ...
  for (found = true; found;) {
    found = false;
    token = LexGetToken(lc, BCT_NAME);
    for (i = 0; !found && RunFields[i].name; i++) {
      if (Bstrcasecmp(lc->str, RunFields[i].name)) {
        found = true;
        if (LexGetToken(lc, BCT_ALL) != BCT_EQUALS) {
          scan_err1(lc, T_("Expected an equals, got: %s"), lc->str);
          return;
        }
        switch (RunFields[i].token) {
          case 's': /* Data spooling */
            token = LexGetToken(lc, BCT_NAME);
            if (Bstrcasecmp(lc->str, "yes") || Bstrcasecmp(lc->str, "true")) {
              res_run.spool_data = true;
              res_run.spool_data_set = true;
            } else if (Bstrcasecmp(lc->str, "no")
                       || Bstrcasecmp(lc->str, "false")) {
              res_run.spool_data = false;
              res_run.spool_data_set = true;
            } else {
              scan_err1(lc, T_("Expect a YES or NO, got: %s"), lc->str);
              return;
            }
            break;
          case 'L': /* Level */
            token = LexGetToken(lc, BCT_NAME);
            for (j = 0; joblevels[j].level_name; j++) {
              if (Bstrcasecmp(lc->str, joblevels[j].level_name)) {
                res_run.level = joblevels[j].level;
                res_run.job_type = joblevels[j].job_type;
                j = 0;
                break;
              }
            }
            if (j != 0) {
              scan_err1(lc, T_("Job level field: %s not found in run record"),
                        lc->str);
              return;
            }
            break;
          case 'p': /* Priority */
            token = LexGetToken(lc, BCT_PINT32);
            if (pass == 2) { res_run.Priority = lc->u.pint32_val; }
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
                scan_err1(lc, T_("Could not find specified Pool Resource: %s"),
                          lc->str);
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
            if (pass == 2) {
              res = my_config->GetResWithName(R_STORAGE, lc->str);
              if (res == NULL) {
                scan_err1(lc,
                          T_("Could not find specified Storage Resource: %s"),
                          lc->str);
                return;
              }
              res_run.storage = (StorageResource*)res;
            }
            break;
          case 'M': /* Messages */
            token = LexGetToken(lc, BCT_NAME);
            if (pass == 2) {
              res = my_config->GetResWithName(R_MSGS, lc->str);
              if (res == NULL) {
                scan_err1(lc,
                          T_("Could not find specified Messages Resource: %s"),
                          lc->str);
                return;
              }
              res_run.msgs = (MessagesResource*)res;
            }
            break;
          case 'm': /* Max run sched time */
            token = LexGetToken(lc, BCT_QUOTED_STRING);
            if (!DurationToUtime(lc->str, &utime)) {
              scan_err1(lc, T_("expected a time period, got: %s"), lc->str);
              return;
            }
            res_run.MaxRunSchedTime = utime;
            res_run.MaxRunSchedTime_set = true;
            break;
          case 'a': /* Accurate */
            token = LexGetToken(lc, BCT_NAME);
            if (strcasecmp(lc->str, "yes") == 0
                || strcasecmp(lc->str, "true") == 0) {
              res_run.accurate = true;
              res_run.accurate_set = true;
            } else if (strcasecmp(lc->str, "no") == 0
                       || strcasecmp(lc->str, "false") == 0) {
              res_run.accurate = false;
              res_run.accurate_set = true;
            } else {
              scan_err1(lc, T_("Expect a YES or NO, got: %s"), lc->str);
            }
            break;
          default:
            scan_err1(lc, T_("Expected a keyword name, got: %s"), lc->str);
            return;
            break;
        } /* end switch */
      } /* end if Bstrcasecmp */
    } /* end for RunFields */

    /* At this point, it is not a keyword. Check for old syle
     * Job Levels without keyword. This form is depreciated!!! */
    if (!found) {
      for (j = 0; joblevels[j].level_name; j++) {
        if (Bstrcasecmp(lc->str, joblevels[j].level_name)) {
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
  for (; token != BCT_EOL; (token = LexGetToken(lc, BCT_ALL))) {
    switch (token) {
      case BCT_NUMBER:
        tokens.emplace_back(lc->str);
        code = atoi(lc->str) - 1;
        if (code < 0 || code > 30) {
          scan_err0(lc, T_("Day number out of range (1-31)"));
          return;
        }
        break;
      case BCT_NAME: /* This handles drop through from keyword */
      case BCT_UNQUOTED_STRING:
        if (tokens.size() > 0 && tokens.back() == "at") {
          tokens.back() += " ";
          tokens.back() += lc->str;
        }
        else {
          tokens.emplace_back(lc->str);
        }
        if (strchr(lc->str, (int)'-')) {
          break;
        }
        if (strchr(lc->str, (int)':')) {
          break;
        }
        if (strchr(lc->str, (int)'/')) {
          break;
        }
        if (lc->str_len == 3 && (lc->str[0] == 'w' || lc->str[0] == 'W')
            && IsAnInteger(lc->str + 1)) {
          code = atoi(lc->str + 1);
          if (code < 0 || code > 53) {
            scan_err0(lc, T_("Week number out of range (0-53)"));
            return;
          }
          break;
        }
        // Everything else must be a keyword
        for (i = 0; keyw[i].name; i++) {
          if (Bstrcasecmp(lc->str, keyw[i].name)) {
            code = keyw[i].code;
            i = 0;
            break;
          }
        }
        if (i != 0) {
          scan_err1(lc, T_("Job type field: %s in run record not found"),
                    lc->str);
          return;
        }
        break;
      case BCT_COMMA:
        continue;
      default:
        scan_err2(lc, T_("Unexpected token: %d:%s"), token, lc->str);
        return;
        break;
    }
  }

  for (std::string str : tokens) {
    for (char& ch : str) {
      if (std::isupper(ch)) {
        ch = std::tolower(ch);
      }
    }
    auto day_spec = FromString<std::variant<
      Mask<MonthOfYear>,
      Mask<WeekOfYear>,
      Mask<WeekOfMonth>,
      Mask<DayOfMonth>,
      Mask<DayOfWeek>
    >>(str);
    if (day_spec) {
      res_run.schedule.day_masks.emplace_back(*day_spec);
    }
    auto time_spec = FromString<std::variant<TimeOfDay, Hourly>>(str);
    if (time_spec) {
      if (std::holds_alternative<TimeOfDay>(*time_spec)) {
        if (std::holds_alternative<Hourly>(res_run.schedule.times)) {
          res_run.schedule.times = std::vector<TimeOfDay>();
        }
        std::get<std::vector<TimeOfDay>>(res_run.schedule.times).emplace_back(std::get<TimeOfDay>(*time_spec));
      }
      else {
        res_run.schedule.times = Hourly(); 
      }
    }
  }

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

  lc->options = options; /* Restore scanner options */
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}
} /* namespace directordaemon */
