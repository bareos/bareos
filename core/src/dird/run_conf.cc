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
#include "lib/bsys.h"

#include <charconv>

namespace directordaemon {

extern struct s_jl joblevels[];

// Deformat
template <class First, class... Rest>
bool Deformat(std::string_view input,
              std::string_view fmt,
              First& first,
              Rest&... rest);

// FromString
// :: int
template <class T, std::enable_if_t<std::is_same_v<T, int>, int> = 0>
std::optional<int> FromString(const std::string& str)
{
  int value = 0;
  auto [ptr, ec] = std::from_chars(str.c_str(), str.c_str() + str.length(), value);
  if (ec == std::errc() && ptr == str.c_str() + str.length()) {
    return value;
  }
  return std::nullopt;
}
// :: MonthOfYear
template <class T, std::enable_if_t<std::is_same_v<T, MonthOfYear>, int> = 0>
std::optional<MonthOfYear> FromString(const std::string& str)
{
  for (size_t i = 0; i < kMonthOfYearLiterals.size(); ++i) {
    if (Bstrcasecmp(str.c_str(), kMonthOfYearLiterals.at(i).data()) || (str.length() == 3 && bstrncasecmp(str.c_str(), kMonthOfYearLiterals.at(i).data(), 3))) {
      return MonthOfYear(i);
    }
  }
  return std::nullopt;
}
// :: WeekOfYear
template <class T, std::enable_if_t<std::is_same_v<T, WeekOfYear>, int> = 0>
std::optional<T> FromString(const std::string& str)
{
  if (str.size() == 0 || str.at(0) != 'w') {
    return std::nullopt;
  }
  auto index = FromString<int>(str.substr(1));
  if (index && 0 <= *index && *index <= 53) { return T(*index); }
  return std::nullopt;
}
// :: WeekOfMonth
template <class T, std::enable_if_t<std::is_same_v<T, WeekOfMonth>, int> = 0>
std::optional<T> FromString(const std::string& str)
{
  if (str == "first" || str == "1st") {
    return WeekOfMonth::kFirst;
  } else if (str == "second" || str == "2nd") {
    return WeekOfMonth::kSecond;
  } else if (str == "third" || str == "3rd") {
    return WeekOfMonth::kThird;
  } else if (str == "fourth" || str == "4th") {
    return WeekOfMonth::kFourth;
  } else if (str == "fifth" || str == "5th") {
    return WeekOfMonth::kFifth;
  } else if (str == "last") {
    return WeekOfMonth::kLast;
  }
  return std::nullopt;
}
// :: DayOfMonth
template <class T, std::enable_if_t<std::is_same_v<T, DayOfMonth>, int> = 0>
std::optional<DayOfMonth> FromString(const std::string& str)
{
  auto number = FromString<int>(str);
  if (number && (0 <= *number && *number <= 30)) {
    return T(*number);
  }
  return std::nullopt;
}
// :: DayOfWeek
template <class T, std::enable_if_t<std::is_same_v<T, DayOfWeek>, int> = 0>
std::optional<T> FromString(const std::string& str)
{
  for (size_t i = 0; i < kDayOfWeekLiterals.size(); ++i) {
    if (Bstrcasecmp(str.c_str(), kDayOfWeekLiterals.at(i).data()) || (str.length() == 3 && bstrncasecmp(str.c_str(), kDayOfWeekLiterals.at(i).data(), 3))) {
      return DayOfWeek(i);
    }
  }
  return std::nullopt;
}
// :: TimeOfDay
template <class T, std::enable_if_t<std::is_same_v<TimeOfDay, T>, int> = 0>
std::optional<T> FromString(const std::string& str)
{
  int hour, minute;
  if (Deformat(str, "at %:%", hour, minute)) {
    return T({hour, minute});
  } else if (Deformat(str, "at %:%am", hour, minute)) {
    return T({(hour % 12), minute});
  } else if (Deformat(str, "at %:%pm", hour, minute)) {
    return T({(hour % 12) + 12, minute});
  }
  return std::nullopt;
}
// :: Hourly
template <class T, std::enable_if_t<std::is_same_v<Hourly, T>, int> = 0>
std::optional<T> FromString(const std::string& str)
{
  return str == "hourly" ? std::optional(Hourly()) : std::nullopt;
}
// :: Interval
template <class T, std::enable_if_t<kIsRange<T>, int> = 0>
std::optional<T> FromString(const std::string& str)
{
  typename T::Type from, to;
  if (Deformat(str, "%-%", from, to)) { return T({from, to}); }
  return std::nullopt;
}
// :: Modulo
template <class T, std::enable_if_t<kIsModulo<T>, int> = 0>
std::optional<T> FromString(const std::string& str)
{
  typename T::Type left, right;
  if (Deformat(str, "%/%", left, right)) { return T({left, right}); }
  return std::nullopt;
}
// :: std::variant
template <class T> static constexpr bool kIsVariant = false;
template <class... Args>
static constexpr bool kIsVariant<std::variant<Args...>> = true;
template <class T, size_t Index = 0, std::enable_if_t<kIsVariant<T>, int> = 0>
std::optional<T> FromString(const std::string& str)
{
  if constexpr (Index < std::variant_size_v<T>) {
    if (auto value = FromString<std::variant_alternative_t<Index, T>>(str)) {
      return T(*value);
    } else {
      return FromString<T, Index + 1>(str);
    }
  }
  return std::nullopt;
}

// Deformat
// :: base
bool Deformat(std::string_view input, std::string_view fmt)
{
  return fmt.find('%') == std::string::npos && input == fmt;
}
// :: recursive
template <class First, class... Rest>
bool Deformat(std::string_view input,
              std::string_view fmt,
              First& first,
              Rest&... rest)
{
  if (input.length() < fmt.length()) { return false; }
  for (size_t i = 0; i < input.length(); ++i) {
    if (input[i] == fmt[i]) {
      continue;
    } else if (fmt[i] == '%') {
      size_t end = i;
      while (end < input.length()
             && (i + 1 == fmt.length() || input[end] != fmt[i + 1])) {
        ++end;
      }
      auto value = FromString<First>(std::string(input.substr(i, end - i)));
      if (value) {
        first = *value;
      } else {
        return false;
      }
      return Deformat(input.substr(end), fmt.substr(i + 1), rest...);
    } else {
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
  int token;
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
        break;
      case BCT_NAME: /* This handles drop through from keyword */
      case BCT_UNQUOTED_STRING:
        if (tokens.size() > 0 && tokens.back() == "at") {
          tokens.back() += " ";
          tokens.back() += lc->str;
        } else {
          tokens.emplace_back(lc->str);
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

  Schedule schedule;
  for (std::string token_str : tokens) {
    std::string str = token_str;
    for (char& ch : str) {
      if (std::isupper(ch)) { ch = std::tolower(ch); }
    }
    if (str == "daily" || str == "weekly" || str == "monthly") {
      if (pass == 1) {
        scan_warn1(lc,
                   "Run directive includes token \"%s\", which is deprecated "
                   "and does nothing",
                   token_str.c_str());
      }
      continue;
    }
    if (auto day_spec = FromString<
            std::variant<Mask<MonthOfYear>, Mask<WeekOfYear>, Mask<WeekOfMonth>,
                         Mask<DayOfMonth>, Mask<DayOfWeek>>>(str)) {
      schedule.day_masks.emplace_back(*day_spec);
    } else if (auto time_spec
               = FromString<std::variant<TimeOfDay, Hourly>>(str)) {
      if (auto* time_of_day = std::get_if<TimeOfDay>(&time_spec.value())) {
        if (auto* times_of_day
            = std::get_if<std::vector<TimeOfDay>>(&schedule.times)) {
          times_of_day->emplace_back(*time_of_day);
        } else {
          std::get<Hourly>(schedule.times).minutes.insert(time_of_day->minute);
        }
      } else {
        if (auto* times_of_day
            = std::get_if<std::vector<TimeOfDay>>(&schedule.times)) {
          std::set<int> minutes;
          for (const TimeOfDay& other_time_of_day : *times_of_day) {
            minutes.insert(other_time_of_day.minute);
          }
          schedule.times = Hourly({std::move(minutes)});
        } else {
          schedule.times = Hourly();
        }
      }
    } else {
      scan_err1(
          lc,
          T_("Could not parse Run directive because of illegal token \"%s\""),
          token_str.c_str());
    }
  }
  if (auto* times_of_day
      = std::get_if<std::vector<TimeOfDay>>(&schedule.times)) {
    if (times_of_day->empty()) { times_of_day->emplace_back(TimeOfDay(0, 0)); }
  } else if (auto* hourly = std::get_if<Hourly>(&schedule.times)) {
    if (hourly->minutes.empty()) { hourly->minutes.insert(0); }
  }
  auto now = time(nullptr);
  if (pass == 1
      && schedule.GetMatchingTimes(now, now + 60 * 60 * 24 * 366).empty()) {
    scan_warn0(lc, "Run directive schedule never runs in the next 366 days");
  }
  res_run.schedule = schedule;


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
