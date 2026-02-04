/*
  BAREOS® - Backup Archiving REcovery Open Sourced

  Copyright (C) 2019-2025 Bareos GmbH & Co. KG

  This program is Free Software; you can redistribute it and/or
  modify it under the terms of version three of the GNU Affero General Public
  License as published by the Free Software Foundation, which is
  listed in the file LICENSE.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.
*/

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif


#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/scheduler.h"
#include "dird/dird_conf.h"
#define DIRECTOR_DAEMON
#include "include/jcr.h"
#include "lib/parse_conf.h"
#include "tests/scheduler_time_source.h"
#include "dird/scheduler_system_time_source.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>

using namespace directordaemon;

class SimulatedTimeAdapter : public SchedulerTimeAdapter {
 public:
  SimulatedTimeAdapter()
      : SchedulerTimeAdapter(std::make_unique<SimulatedTimeSource>())
  {
    default_wait_interval_ = 60;
  }
};

static std::unique_ptr<Scheduler> scheduler;
static SimulatedTimeAdapter* time_adapter;

class SchedulerTest : public ::testing::Test {
  void SetUp() override
  {
    OSDependentInit();
    std::unique_ptr<SimulatedTimeAdapter> ta
        = std::make_unique<SimulatedTimeAdapter>();
    time_adapter = ta.get();

    scheduler = std::make_unique<Scheduler>(std::move(ta),
                                            SimulatedTimeSource::ExecuteJob);
  }
  void TearDown() override { scheduler.reset(); }
};

static void StopScheduler(std::chrono::milliseconds timeout)
{
  std::this_thread::sleep_for(timeout);
  scheduler->Terminate();
}

TEST_F(SchedulerTest, terminate)
{
  std::string path_to_config_file
      = std::string("configs/scheduler/scheduler-hourly");

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  my_config->ParseConfig();

  std::thread scheduler_canceler(StopScheduler, std::chrono::milliseconds(200));

  scheduler->Run();

  scheduler_canceler.join();
  delete my_config;
}

TEST_F(SchedulerTest, system_time_source)
{
  SystemTimeSource s;
  time_t start = s.SystemTime();
  s.SleepFor(std::chrono::seconds(2));
  time_t end = s.SystemTime();
  EXPECT_GT(end - start, 0);
  EXPECT_LT(end - start, 4);
}

TEST_F(SchedulerTest, system_time_source_canceled)
{
  SystemTimeSource s;

  time_t start = s.SystemTime();

  std::thread terminate_wait_for_thread([&s]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    s.Terminate();
  });

  s.SleepFor(std::chrono::seconds(1));
  time_t end = s.SystemTime();

  terminate_wait_for_thread.join();

  bool end_equals_start = (end == start);
  bool end_wrapped_to_next_second_while_waiting = (end == start + 1);

  EXPECT_TRUE(end_equals_start || end_wrapped_to_next_second_while_waiting);
}

static std::vector<time_t> list_of_job_execution_time_stamps;
static std::vector<time_t> list_of_time_gaps_between_adjacent_jobs;
static int maximum_number_of_jobs_run{0};
static int counter_of_number_of_jobs_run{0};

void SimulatedTimeSource::ExecuteJob(JobControlRecord* jcr)
{
  // called by the scheduler to simulate a job run
  counter_of_number_of_jobs_run++;

  list_of_job_execution_time_stamps.emplace_back(
      time_adapter->time_source_->SystemTime());

  if (debug) {
    std::cout << jcr->dir_impl->res.job->resource_name_ << std::endl;
  }

  if (counter_of_number_of_jobs_run == maximum_number_of_jobs_run) {
    scheduler->Terminate();
  }

  // auto tm = *std::gmtime(&t);
  // std::cout << "Executing job: " << jcr->Job << std::endl;
  // std::cout << put_time(&tm, "%d-%m-%Y %H:%M:%S") << std::endl;
}

static auto CalculateAverage()
{
  std::adjacent_difference(
      list_of_job_execution_time_stamps.begin(),
      list_of_job_execution_time_stamps.end(),
      std::back_inserter(list_of_time_gaps_between_adjacent_jobs));

  list_of_time_gaps_between_adjacent_jobs.erase(
      list_of_time_gaps_between_adjacent_jobs.begin());

  auto sum = std::accumulate(list_of_time_gaps_between_adjacent_jobs.begin(),
                             list_of_time_gaps_between_adjacent_jobs.end(),
                             std::size_t{0});

  return sum / list_of_time_gaps_between_adjacent_jobs.size();
}

TEST_F(SchedulerTest, hourly)
{
  if (debug) { std::cout << "Start test" << std::endl; }

  std::string path_to_config_file{
      std::string("configs/scheduler/scheduler-hourly")};

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  if (debug) { std::cout << "Parse config" << std::endl; }

  my_config->ParseConfig();
  ASSERT_TRUE(PopulateDefs());

  list_of_job_execution_time_stamps.clear();
  list_of_time_gaps_between_adjacent_jobs.clear();
  counter_of_number_of_jobs_run = 0;
  maximum_number_of_jobs_run = 25;

  if (debug) { std::cout << "Run scheduler" << std::endl; }
  scheduler->Run();

  if (debug) { std::cout << "End" << std::endl; }
  delete my_config;

  auto average = CalculateAverage();

  bool average_time_between_adjacent_jobs_is_too_low = average < 3600 - 36;
  bool average_time_between_adjacent_jobs_is_too_high = average > 3600 + 36;

  if (debug || average_time_between_adjacent_jobs_is_too_low
      || average_time_between_adjacent_jobs_is_too_high) {
    int hour = 0;
    for (const auto& t : list_of_job_execution_time_stamps) {
      auto tm = *std::gmtime(&t);
      std::cout << "Hour " << hour << ": "
                << put_time(&tm, "%d-%m-%Y %H:%M:%S");
      if (hour) {
        std::cout << " - " << list_of_time_gaps_between_adjacent_jobs[hour]
                  << " sec";
      }
      std::cout << std::endl;
      hour++;
    }
  }

  if (debug) {
    std::cout << "Average simulated time between two jobs: " << average
              << " seconds" << std::endl;
  }

  EXPECT_FALSE(average_time_between_adjacent_jobs_is_too_low);
  EXPECT_FALSE(average_time_between_adjacent_jobs_is_too_high);
}


static void TestWithConfig(std::string path_to_config_file,
                           std::vector<uint8_t> wdays)
{
  if (debug) { std::cout << "Start test" << std::endl; }

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  if (debug) { std::cout << "Parse config" << std::endl; }

  my_config->ParseConfig();
  ASSERT_TRUE(PopulateDefs());

  list_of_job_execution_time_stamps.clear();
  maximum_number_of_jobs_run = 3;
  counter_of_number_of_jobs_run = 0;

  if (debug) { std::cout << "Run scheduler" << std::endl; }
  scheduler->Run();

  int wday_index = 0;
  for (const auto& t : list_of_job_execution_time_stamps) {
    auto tm = *std::localtime(&t);
    bool is_two_o_clock = tm.tm_hour == 2;

    ASSERT_LT(wday_index, wdays.size())
        << "List of weekdays should match the number of execution time stamps.";
    bool is_right_day = wdays.at(wday_index) == tm.tm_wday;

    EXPECT_TRUE(is_two_o_clock);
    EXPECT_TRUE(is_right_day);
    if (debug || !is_two_o_clock || !is_right_day) {
      std::cout << put_time(&tm, "%A, %d-%m-%Y %H:%M:%S ") << std::endl;
    }
    wday_index++;
  }

  if (debug) { std::cout << "End" << std::endl; }

  delete my_config;
}

enum
{
  kMonday = 1,
  kTuesday = 2,
  kWednesday = 3,
  kThursday = 4,
  kFriday = 5,
  kSaturday = 6,
  kSunday = 7
};

TEST_F(SchedulerTest, parse_schedule_correctly)
{
  static std::vector<std::pair<std::string_view, std::string_view>> kValidSchedules
      = {
          {"", "at 00:00"},
          {"hourly", "hourly at 00:00"},
          {"hourly at :15", "hourly at 00:15"},
          {"hourly at 00:15", "hourly at 00:15"},
          {"hourly at 13:15", "hourly at 00:15"},
          {"at 20:00", "at 20:00"},
          {"daily at 20:00", "at 20:00"},
          {"w00/w02 Fri at 23:10", "w00/w02 Fri at 23:10"},
          {"Mon-Fri", "Mon-Fri at 00:00"},
          {"Mon-Fri at 20:00", "Mon-Fri at 20:00"},
          {"1st saturday at 20:00", "first Sat at 20:00"},
          {"on 1st saturday at 20:00", "first Sat at 20:00"},
          {"Mon, Tue, Wed-Fri, Sat", "Mon, Tue, Wed-Fri, Sat at 00:00"},
          {"Fri-Mon", "Fri-Mon at 00:00"},
          {"on 27 at 21:45", "27 at 21:45"},
          {"20-10 at 21:45", "20-10 at 21:45"},
          {"1/2 at 23:10", "1/2 at 23:10"},
          {"5/5 at 23:10", "0/5 at 23:10"},
      };
  for (auto [schedule, expected_generated] : kValidSchedules) {
    auto [result, _] = Parser<Schedule>::Parse(schedule);
    EXPECT_TRUE(std::holds_alternative<Schedule>(result))
        << "Could not parse '" << schedule << "'" << std::endl
        << std::get<Parser<Schedule>::Error>(result).message;
    if (std::holds_alternative<Schedule>(result)) {
      std::string generated = ToString(std::get<Schedule>(result));
      EXPECT_EQ(expected_generated, generated)
          << "Generated schedule string \"" << generated
          << "\" does not match the expected string \"" << expected_generated
          << "\"";
    }
  }

  static std::vector<std::string_view> kInvalidSchedules
  = {
      "w20/w05",
  };
  for (std::string_view schedule : kInvalidSchedules) {
    auto [result, _] = Parser<Schedule>::Parse(schedule);
    EXPECT_TRUE(std::holds_alternative<Parser<Schedule>::Error>(result));
  }
}

struct DateTimeValidator {
  virtual ~DateTimeValidator() = default;

  virtual bool operator()(const DateTime& date_time) const = 0;
  virtual std::string String() const = 0;
};
template <class T> class ValueValidator : public DateTimeValidator {
 public:
  ValueValidator(const std::vector<T>& valid_values)
      : valid_values_(valid_values)
  {
  }
  ValueValidator(T value) : valid_values_({value}) {}

  virtual bool operator()(const DateTime& date_time) const override
  {
    return std::find(valid_values_.begin(), valid_values_.end(),
                     Get<T>(date_time))
           != valid_values_.end();
  }

  virtual std::string String() const override
  {
    std::string result;
    result += "ValueValidator{";
    for (size_t i = 0; i < valid_values_.size(); ++i) {
      // date / time specifications are either convertible to int or have a name
      if constexpr (std::is_convertible_v<T, int>) {
        result += int(valid_values_.at(i));
      } else {
        result += valid_values_.at(i).name;
      }
      if (i + 1 < valid_values_.size()) { result += ", "; }
    }
    result += "}";
    return result;
  }

 private:
  template <class U> static auto Get(const DateTime& date_time)
  {
    if constexpr (std::is_same_v<U, MonthOfYear>) {
      return MonthOfYear{date_time.month};
    } else if constexpr (std::is_same_v<U, WeekOfYear>) {
      return WeekOfYear{date_time.week_of_year};
    } else if constexpr (std::is_same_v<U, WeekOfMonth>) {
      return WeekOfMonth{date_time.week_of_month};
    } else if constexpr (std::is_same_v<U, DayOfMonth>) {
      return DayOfMonth{date_time.day_of_month};
    } else if constexpr (std::is_same_v<U, DayOfWeek>) {
      return DayOfWeek{date_time.day_of_week};
    } else if constexpr (std::is_same_v<U, TimeOfDay>) {
      return TimeOfDay{date_time.hour, date_time.minute};
    } else {
      static_assert("Invalid template argument.");
    }
  }

  std::vector<T> valid_values_;
};
template <class T>
std::shared_ptr<DateTimeValidator> MakeValidator(const T& value)
{
  return std::make_shared<ValueValidator<T>>(value);
}
template <class T>
std::shared_ptr<DateTimeValidator> MakeValidator(const std::vector<T>& values)
{
  return std::make_shared<ValueValidator<T>>(values);
}

TEST_F(SchedulerTest, trigger_correctly)
{
  // We construct the start-time from DateTime such that it is the same across
  // different time zones.
  DateTime start_date_time{0};
  start_date_time.year = 1970;
  start_date_time.month = 0;
  start_date_time.week_of_year = 1;
  start_date_time.week_of_month = 0;
  start_date_time.day_of_year = 0;
  start_date_time.day_of_month = 0;
  start_date_time.day_of_week = 4;
  start_date_time.hour = 12;
  start_date_time.minute = 0;
  start_date_time.second = 0;
  const time_t start_time = start_date_time.GetTime();

  static constexpr int kDays = 365;

  static std::vector<
      std::tuple<std::string_view, std::optional<int>,
                 std::vector<std::shared_ptr<DateTimeValidator>>>>
      kSchedules = {
          {"", kDays, {}},
          {"jan", 31, {}},
          {"mon", 52, {MakeValidator(*DayOfWeek::FromName("Monday"))}},
          {"mon, fri",
           105,
           {MakeValidator(std::vector{*DayOfWeek::FromName("Monday"),
                                      *DayOfWeek::FromName("Friday")})}},
          {
              "jan mon, fri feb",
              18,
              {MakeValidator(std::vector{*DayOfWeek::FromName("Monday"),
                                         *DayOfWeek::FromName("Friday")}),
               MakeValidator(std::vector{*MonthOfYear::FromName("January"),
                                         *MonthOfYear::FromName("February")})},
          },
          {"apr mon, fri dec-feb",
           34,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Monday"),
                                         *DayOfWeek::FromName("Friday")}),
               MakeValidator(std::vector{*MonthOfYear::FromName("December"),
                                         *MonthOfYear::FromName("January"),
                                         *MonthOfYear::FromName("February"),
                                         *MonthOfYear::FromName("April")}),
           }},
          {"fri-tue, dec-feb",
           64,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Friday"),
                                         *DayOfWeek::FromName("Saturday"),
                                         *DayOfWeek::FromName("Sunday"),
                                         *DayOfWeek::FromName("Monday"),
                                         *DayOfWeek::FromName("Tuesday")}),
               MakeValidator(std::vector{*MonthOfYear::FromName("December"),
                                         *MonthOfYear::FromName("January"),
                                         *MonthOfYear::FromName("February")}),
           }},
          {"Last sunday, february, Mar at 20:00",
           2,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Sunday")}),
               MakeValidator(std::vector{*MonthOfYear::FromName("February"),
                                         *MonthOfYear::FromName("March")}),
           }},
          {"1st, second, 3rd, Last sunday, february, Mar at 20:00",
           8,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Sunday")}),
               MakeValidator(std::vector{*MonthOfYear::FromName("February"),
                                         *MonthOfYear::FromName("March")}),
           }},
          {"w00-w22 fri at 20:00",
           22,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Friday")}),
           }},
          {"w00-w22 mon at 20:00",
           21,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Monday")}),
           }},
          {"w00/w10 mon at 20:00",
           5,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Monday")}),
           }},
          {"w00/w02 mon at 20:00 jan mon",
           2,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Monday")}),
               MakeValidator(std::vector{*MonthOfYear::FromName("January")}),
           }},
          {"w01/w02 mon at 20:00 jan",
           2,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Monday")}),
               MakeValidator(std::vector{*MonthOfYear::FromName("January")}),
           }},
          {"w01/w02 thu at 20:00 jan",
           3,
           {
               MakeValidator(std::vector{*DayOfWeek::FromName("Thursday")}),
               MakeValidator(std::vector{*MonthOfYear::FromName("January")}),
           }},
          {"30-1 at 20:00",
           30,
           {
               MakeValidator(
                   std::vector{DayOfMonth{29}, DayOfMonth{30}, DayOfMonth{0}}),
           }},
           {"0/2 at 23:10", 186, {}},
           {"1/2 at 23:10", 179, {}},
           {"0/7 at 23:10", 59, {
            MakeValidator(std::vector{DayOfMonth{0}, DayOfMonth{7}, DayOfMonth{14}, DayOfMonth{21}, DayOfMonth{28}})
           }},
      };
  for (auto [schedule, expected_count, validators] : kSchedules) {
    auto [result, warnings] = Parser<Schedule>::Parse(schedule);
    EXPECT_TRUE(std::holds_alternative<Schedule>(result))
        << "Could not parse '" << schedule << "'" << std::endl
        << std::get<Parser<Schedule>::Error>(result).message;
    auto times = std::get<Schedule>(result).GetMatchingTimes(
        start_time, start_time + kDays * kSecondsPerDay);
    if (expected_count.has_value()) {
      EXPECT_EQ(times.size(), expected_count.value())
          << "In schedule \"" << schedule << "\", matching times count "
          << times.size() << " does not match expected count "
          << expected_count.value();
    }
    for (auto time : times) {
      DateTime date_time{time};
      for (const auto& validator : validators) {
        EXPECT_TRUE((*validator)(date_time))
            << "In schedule \"" << schedule << "\", matching time " << time
            << " did not suffice all validators:" << std::endl
            << validator->String() << std::endl
            << date_time;
      }
    }
  }
}

TEST_F(SchedulerTest, on_time)
{
  std::vector<uint8_t> wdays{kMonday, kMonday, kMonday};
  TestWithConfig("configs/scheduler/scheduler-on-time", wdays);
}

TEST_F(SchedulerTest, on_time_noday)
{
  std::vector<uint8_t> wdays{kThursday, kFriday, kSaturday};
  TestWithConfig("configs/scheduler/scheduler-on-time-noday", wdays);
}

TEST_F(SchedulerTest, on_time_noday_noclient)
{
  std::vector<uint8_t> wdays{kThursday, kFriday, kSaturday};
  TestWithConfig("configs/scheduler/scheduler-on-time-noday-noclient", wdays);
}

TEST_F(SchedulerTest, add_job_with_no_run_resource_to_queue)
{
  if (debug) { std::cout << "Start test" << std::endl; }

  std::string path_to_config_file{
      std::string("configs/bareos-configparser-tests")};

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  if (debug) { std::cout << "Parse config" << std::endl; }

  my_config->ParseConfig();
  ASSERT_TRUE(PopulateDefs());

  counter_of_number_of_jobs_run = 0;
  maximum_number_of_jobs_run = 1;

  std::thread scheduler_thread{[]() { scheduler->Run(); }};

  JobResource* job{dynamic_cast<JobResource*>(
      my_config->GetResWithName(R_JOB, "backup-bareos-fd"))};
  ASSERT_TRUE(job) << "Job Resource \"backup-bareos-fd\" not found";

  scheduler->AddJobWithNoRunResourceToQueue(job, JobTrigger::kUndefined);

  scheduler_thread.join();
  ASSERT_EQ(counter_of_number_of_jobs_run, 1);
  delete my_config;
}
