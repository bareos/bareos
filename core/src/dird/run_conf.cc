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

template <class T> struct Parser {
  static std::optional<T> Parse(std::string_view str);
};

template <> struct Parser<int> {
  static std::optional<int> Parse(std::string_view str)
  {
    int value = 0;
    auto [ptr, ec]
        = std::from_chars(str.data(), str.data() + str.length(), value);
    if (ec == std::errc() && ptr == str.data() + str.length()) { return value; }
    return std::nullopt;
  }
};
template <> struct Parser<MonthOfYear> {
  static std::optional<MonthOfYear> Parse(std::string_view str)
  {
    return MonthOfYear::FromName(str);
  }
};
template <> struct Parser<WeekOfYear> {
  static std::optional<WeekOfYear> Parse(std::string_view str)
  {
    if (str.size() == 0 || str.at(0) != 'w') { return std::nullopt; }
    auto index = Parser<int>::Parse(str.substr(1));
    if (index && 0 <= *index && *index <= 53) { return WeekOfYear{*index}; }
    return std::nullopt;
  }
};
template <> struct Parser<WeekOfMonth> {
  static std::optional<WeekOfMonth> Parse(std::string_view str)
  {
    return WeekOfMonth::FromName(str);
  }
};
template <> struct Parser<DayOfMonth> {
  static std::optional<DayOfMonth> Parse(std::string_view str)
  {
    auto number = Parser<int>::Parse(str);
    if (number && (1 <= *number && *number <= 31)) {
      return DayOfMonth{*number - 1};
    }
    return std::nullopt;
  }
};
template <> struct Parser<DayOfWeek> {
  static std::optional<DayOfWeek> Parse(std::string_view str)
  {
    return DayOfWeek::FromName(str);
  }
};
template <> struct Parser<TimeOfDay> {
  static std::optional<TimeOfDay> Parse(std::string_view str)
  {
    static constexpr std::string_view kPrefix{"at "};
    if (str.find_first_of(kPrefix) != 0) { return std::nullopt; }
    str = str.substr(std::string_view{kPrefix}.length());
    size_t index = str.find(':');
    if (str.find("am") == str.length() - 2) {
      str = str.substr(0, str.length() - 2);
      auto hour = Parser<int>::Parse(str.substr(0, index));
      auto minute = Parser<int>::Parse(str.substr(index + 1));
      if (hour && minute) {
        if (0 <= *hour && *hour <= 12 && 0 <= *minute && *minute <= 59) {
          return TimeOfDay{(*hour % 12), *minute};
        }
      }
    } else if (str.find("pm") == str.length() - 2) {
      str = str.substr(0, str.length() - 2);
      auto hour = Parser<int>::Parse(str.substr(0, index));
      auto minute = Parser<int>::Parse(str.substr(index + 1));
      if (hour && minute) {
        if (0 <= *hour && *hour <= 12 && 0 <= *minute && *minute <= 59) {
          return TimeOfDay{(*hour % 12) + 12, *minute};
        }
      }
    } else {
      auto hour = Parser<int>::Parse(str.substr(0, index));
      if (index == 0) {  // such that "hourly at :XX" is valid
        hour = std::optional{0};
      }
      auto minute = Parser<int>::Parse(str.substr(index + 1));
      if (hour && minute) {
        if (0 <= *hour && *hour <= 23 && 0 <= *minute && *minute <= 59) {
          return TimeOfDay{*hour, *minute};
        }
      }
    }
    return std::nullopt;
  }
};
template <> struct Parser<Hourly> {
  static std::optional<Hourly> Parse(std::string_view str)
  {
    return str == "hourly" ? std::optional{Hourly{}} : std::nullopt;
  }
};
template <class T> struct Parser<Interval<T>> {
  static std::optional<Interval<T>> Parse(std::string_view str)
  {
    size_t index = str.find('-');
    if (index == std::string::npos) { return std::nullopt; }
    auto first = Parser<T>::Parse(str.substr(0, index));
    auto last = Parser<T>::Parse(str.substr(index + 1));
    if (first && last) { return Interval<T>{*first, *last}; }
    return std::nullopt;
  }
};
template <class T> struct Parser<Modulo<T>> {
  static std::optional<Modulo<T>> Parse(std::string_view str)
  {
    size_t index = str.find('/');
    if (index == std::string::npos) { return std::nullopt; }
    if constexpr (std::is_same_v<DayOfMonth, T>) {
      auto remainder = Parser<int>::Parse(str.substr(0, index));
      auto divisor = Parser<int>::Parse(str.substr(index + 1));
      if (remainder && divisor) {
        if (static_cast<int>(*remainder) < static_cast<int>(*divisor)) {
          return Modulo<T>{static_cast<int>(*remainder), static_cast<int>(*divisor)};
        }
        else if (static_cast<int>(*remainder) == static_cast<int>(*divisor)) {
          return Modulo<T>{0, static_cast<int>(*divisor)};
        }
      }
    }
    else {
      auto remainder = Parser<T>::Parse(str.substr(0, index));
      auto divisor = Parser<T>::Parse(str.substr(index + 1));
      if (remainder && divisor) {
        if (static_cast<int>(*remainder) < static_cast<int>(*divisor)) {
          return Modulo<T>{static_cast<int>(*remainder), static_cast<int>(*divisor)};
        }
        else if (static_cast<int>(*remainder) == static_cast<int>(*divisor)) {
          return Modulo<T>{0, static_cast<int>(*divisor)};
        }
      }
    }
    return std::nullopt;
  }
};
template <class... Args> struct Parser<std::variant<Args...>> {
  template <size_t Index = 0>
  static std::optional<std::variant<Args...>> Parse(std::string_view str)
  {
    static_assert(Index < sizeof...(Args), "Index out of bounds.");
    if (auto value
        = Parser<std::variant_alternative_t<Index, std::variant<Args...>>>::
            Parse(str)) {
      return std::make_optional<std::variant<Args...>>(std::move(*value));
    }
    if constexpr (Index + 1 < sizeof...(Args)) { return Parse<Index + 1>(str); }
    return std::nullopt;
  }
};
std::pair<std::variant<Schedule, Parser<Schedule>::Error>,
          Parser<Schedule>::Warnings>
Parser<Schedule>::Parse(const std::vector<std::string>& tokens)
{
  std::vector<std::string> warnings;
  Schedule schedule;
  for (size_t i = 0; i < tokens.size(); ++i) {
    std::string lower_str = tokens[i];
    for (char& ch : lower_str) {
      if (std::isupper(ch)) { ch = std::tolower(ch); }
    }
    if (lower_str == "on") {
      if (i != 0) {
        warnings.emplace_back("Token \"on\" should only appear at the start of the schedule.");
      }
      continue;
    }
    if (lower_str == "daily" || lower_str == "weekly"
        || lower_str == "monthly") {
      warnings.emplace_back("Run directive includes token \"" + lower_str + "\", which is deprecated "
                     "and does nothing");
      continue;
    }
    if (auto day_spec
        = Parser<std::variant<Mask<MonthOfYear>, Mask<WeekOfYear>,
                              Mask<WeekOfMonth>, Mask<DayOfMonth>,
                              Mask<DayOfWeek>>>::Parse(lower_str)) {
      std::visit(
          [&](auto&& value) {
            using T = std::remove_reference_t<decltype(value)>;
            std::get<std::vector<T>>(schedule.day_masks)
                .emplace_back(std::move(value));
          },
          day_spec.value());
    } else if (auto time_spec
               = Parser<std::variant<TimeOfDay, Hourly>>::Parse(lower_str)) {
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
          schedule.times = Hourly{{std::move(minutes)}};
        } else {
          schedule.times = Hourly{};
        }
      }
    } else {
      return {
          Error{std::string{
                    "Could not parse Run directive because of illegal token \""}
                + tokens[i] + "\""},
          {std::move(warnings)}};
    }
  }
  if (auto* times_of_day
      = std::get_if<std::vector<TimeOfDay>>(&schedule.times)) {
    if (times_of_day->empty()) { times_of_day->emplace_back(TimeOfDay(0, 0)); }
  } else if (auto* hourly = std::get_if<Hourly>(&schedule.times)) {
    if (hourly->minutes.empty()) { hourly->minutes.insert(0); }
  }
  auto now = time(nullptr);
  if (schedule.GetMatchingTimes(now, now + kSecondsPerDay * 366).empty()) {
    warnings.emplace_back(
        "Run directive schedule never runs in the next 366 days");
  }
  return {schedule, {std::move(warnings)}};
}
std::pair<std::variant<Schedule, Parser<Schedule>::Error>,
          Parser<Schedule>::Warnings>
Parser<Schedule>::Parse(std::string_view str)
{
  std::vector<std::string> tokens{""};
  for (char ch : str) {
    if (ch == ',') { continue; }
    if (ch == ' ') {
      if (tokens.back() != "at") {
        tokens.emplace_back();
      } else {
        tokens.back() += ' ';
      }
    } else {
      tokens.back() += ch;
    }
  }
  if (tokens.back().empty()) { tokens.pop_back(); }
  return Parse(tokens);
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
void StoreRun(lexer* lc, const ResourceItem* item, int index, int pass)
{
  int i, j;
  int options = lc->options;
  int token;
  bool found;
  utime_t utime;
  BareosResource* res;
  RunResource res_run;
  std::vector<std::string> tokens;

  lc->options.set(lexer::options::NoIdent); /* Want only "strings" */

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

  /* Scan schedule tokens. */
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

  auto [result, warnings] = Parser<Schedule>::Parse(tokens);
  if (pass == 1) {
    for (const std::string& warning : warnings.messages) {
      scan_warn0(lc, warning.c_str());
    }
  }
  if (auto* schedule = std::get_if<Schedule>(&result)) {
    res_run.schedule = *schedule;
  } else {
    scan_err0(lc, std::get<Parser<Schedule>::Error>(result).message.c_str());
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
