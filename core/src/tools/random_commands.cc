/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include "lib/cli.h"
#include <cstdint>
#include <random>
#include <string_view>
#include <fmt/format.h>
#include <sstream>
#include <span>
#include <algorithm>

std::string_view pools[] = {
    "Full",
    "Incremental",
    "Differential",
};

std::uint64_t job_ids[] = {
    1,
    2,
    3,
};

std::string_view config_jobs[] = {
    "copy-full",    "copy-incr",     "backup-bareos-fd",
    "RestoreFiles", "BackupCatalog",
};

std::string_view client_names[] = {"bareos-fd"};

std::uint64_t fileset_id[] = {
    1,
};

std::string_view storage_names[] = {
    "File",
};

std::string_view job_types[] = {
    "B", "M", "V", "R", "U", "I", "D", "A", "C", "c", "g", "S", "O",
};

std::string_view volume_names[] = {
    "Full-0001",         "Full-0002",         "Full-0003",
    "Incremental-0001",  "Incremental-0002",  "Incremental-0003",
    "Differential-0001", "Differential-0002", "Differential-0003",
};

std::string_view volume_status[] = {
    "Append",   "Archive", "Disabled",  "Full",  "Used",
    "Cleaning", "Recycle", "Read-Only", "Error",
};


struct prand {
  std::uint64_t state{};

  constexpr std::uint64_t next()
  {
    // xorshift*

    std::uint64_t x = state;

    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;

    state = x * 0x2545F4914F6CDD1DULL;

    return state;
  }
};

template <std::size_t N, typename T>
constexpr T select_random(prand& rand, const T (&arr)[N])
{
  static_assert(N > 0);
  return arr[rand.next() % N];
}

template <typename T> T select_random(prand& rand, std::span<const T> values)
{
  static_assert(values.size() > 0);
  return values[rand.next() % values.size()];
}

template <typename T> constexpr T select_in_range(prand& rand, T min, T max)
{
  auto r = rand.next();
  return min + (r % (max - min));
}

enum class update_type
{
  Job,
  Stats,
  Pool,
  Volume,
};

struct part {
  std::string name;
  std::string value;
};

std::string random_string(prand& rand,
                          std::size_t max_size = 100,
                          std::size_t min_size = 0)
{
  auto size_count = select_in_range(rand, min_size, max_size);

  std::string s;
  s.reserve(size_count);
  for (std::size_t i = 0; i < size_count; ++i) {
    s.push_back(select_in_range(rand, '!', '~'));
  }

  return s;
}

std::string random_number(prand& rand,
                          std::size_t digit_max_count = 19,
                          std::size_t digit_min_count = 0)
{
  auto digit_count = select_in_range(rand, digit_min_count, digit_max_count);

  std::string s;
  s.reserve(digit_count);
  for (std::size_t i = 0; i < digit_count; ++i) {
    s.push_back(select_in_range(rand, '0', '9'));
  }

  return s;
}

std::string random_size(prand& rand,
                        std::size_t max_count = 10,
                        std::size_t min_count = 1)
{
  auto size_count = select_in_range(rand, min_count, max_count);

  std::stringstream ss;
  for (std::size_t i = 0; i < size_count; ++i) {
    ss << random_number(rand);
    if (select_in_range(rand, 1, 100) < 20) ss << " ";
    ss << select_random(rand, {"*", "K", "KB", "M", "MB", "G", "GB", "T", "TB",
                               "P", "PB", "E", "EB", ""});
    if (select_in_range(rand, 1, 100) < 20) ss << " ";
  }

  return ss.str();
}

std::string random_duration(prand& rand,
                            std::size_t max_count = 10,
                            std::size_t min_count = 1)
{
  auto size_count = select_in_range(rand, min_count, max_count);

  std::stringstream ss;
  for (std::size_t i = 0; i < size_count; ++i) {
    ss << random_number(rand);
    if (select_in_range(rand, 1, 100) < 20) ss << " ";
    ss << select_random(
        rand, {"N", "SECONDS", "MONTHS", "MINUTES", "MINS", "HOURS", "DAYS",
               "WEEKS", "QUARTERS", "YEARS", ""});
    if (select_in_range(rand, 1, 100) < 20) ss << " ";
  }

  return ss.str();
}

std::string random_yesno(prand& rand)
{
  return select_random(rand, {"Yes", "True", "No", "False"});
}

part generate_jobid(prand& rand)
{
  return {"jobid", std::to_string(select_random(rand, job_ids))};
}

part generate_jobname(prand& rand)
{
  return {"jobname", std::string(select_random(rand, config_jobs))};
}

part generate_jobtype(prand& rand)
{
  return {"jobtype", std::string(select_random(rand, job_types))};
}

part generate_starttime(prand& rand)
{
  return {
      "starttime",
      fmt::format("\"{}-{}-{} {}:{}:{}{}\"", random_number(rand),
                  random_number(rand), random_number(rand), random_number(rand),
                  random_number(rand), random_number(rand), random_number(rand),
                  random_string(rand, 15))};
}

part generate_client(prand& rand)
{
  return {"client", std::string(select_random(rand, client_names))};
}

part generate_filesetid(prand& rand)
{
  return {"filesetid", std::to_string(select_random(rand, fileset_id))};
}

part generate_stats(prand&) { return {"stats", ""}; }

part generate_days(prand& rand) { return {"days", random_number(rand)}; }

part generate_pool(prand& rand)
{
  return {"pool", std::string(select_random(rand, pools))};
}

part generate_volume(prand& rand)
{
  enum class volume_type
  {
    Specific,
    General,
  };

  switch (select_random(rand, {volume_type::Specific, volume_type::General})) {
    case volume_type::Specific: {
      return {"volume", std::string(select_random(rand, volume_names))};
    } break;
    default:
    case volume_type::General: {
      return {"volume", ""};
    } break;
  }
}

part generate_maxvoljobs(prand& rand)
{
  return {"maxvoljobs", random_number(rand)};
}

part generate_maxvolfiles(prand& rand)
{
  return {"maxvolfiles", random_number(rand)};
}

part generate_maxvolbytes(prand& rand)
{
  return {"maxvolbytes", random_size(rand)};
}

part generate_recycle(prand& rand) { return {"recycle", random_yesno(rand)}; }

part generate_inchanger(prand& rand)
{
  return {"inchanger", random_yesno(rand)};
}

part generate_frompool(prand& rand)
{
  return {"frompool", std::string(select_random(rand, pools))};
}

part generate_allfrompool(prand& rand)
{
  return {"allfrompool", std::string(select_random(rand, pools))};
}

part generate_recyclepool(prand& rand)
{
  enum class recycle_type
  {
    Specific,
    Default,
  };

  switch (
      select_random(rand, {recycle_type::Specific, recycle_type::Default})) {
    case recycle_type::Specific: {
      return {"recyclepool", std::string(select_random(rand, pools))};
    } break;
    default:
    case recycle_type::Default: {
      return {"recyclepool", ""};
    } break;
  }
}

part generate_storage(prand& rand)
{
  return {"storage", std::string(select_random(rand, storage_names))};
}

part generate_slot(prand& rand) { return {"slot", random_number(rand)}; }

part generate_actiononpurge(prand& rand)
{
  // action on purge is supposed(?) to treat everything but truncate as no
  return {"actiononpurge",
          select_random(rand, {std::string("truncate"), random_string(rand)})};
}

part generate_enabled(prand& rand)
{
  auto yn = random_yesno(rand);
  auto num = random_number(rand);
  return {"enabled", select_random(rand, {yn, num, std::string("archive")})};
}

part generate_volretention(prand& rand)
{
  return {"volretention", random_duration(rand)};
}

part generate_voluse(prand& rand) { return {"voluse", random_duration(rand)}; }

part generate_volstatus(prand& rand)
{
  return {"volstatus", std::string(select_random(rand, volume_status))};
}

part generate_garbage(prand& rand) { return {random_string(rand), ""}; }

std::string generate_random_update(prand& rand)
{
  static constexpr std::size_t max_parts = 10;
  static constexpr std::size_t min_parts = 1;

  auto part_count = select_in_range(rand, min_parts, max_parts);

  std::stringstream s;
  for (std::size_t i = 0; i < part_count; ++i) {
    auto* generator = select_random(
        rand,
        {
            generate_jobid,       generate_jobname,      generate_jobtype,
            generate_starttime,   generate_client,       generate_filesetid,

            generate_stats,       generate_days,

            generate_pool,

            generate_volume,      generate_maxvoljobs,   generate_maxvolfiles,
            generate_maxvolbytes, generate_recycle,      generate_inchanger,
            generate_frompool,    generate_allfrompool,  generate_recyclepool,
            generate_storage,     generate_slot,         generate_actiononpurge,
            generate_enabled,     generate_volretention, generate_voluse,
            generate_volstatus,   generate_garbage,
        });

    auto [name, value] = generator(rand);

    if (select_in_range(rand, 1, 100) <= 5) { value = random_string(rand); }

    s << name;
    if (!value.empty()) {
      if (std::find(std::begin(value), std::end(value), ' ')
          != std::end(value)) {
        s << "=\"" << value << "\"";
      } else {
        s << "=" << value;
      }
    }
    s << " ";
  }

  return s.str();
}

int main(int argc, char* argv[])
{
  std::size_t command_offset = 0;
  std::size_t command_count = 1000;

  std::uint64_t seed = 0;

  CLI::App app{"generate random commands"};

  bool zero_delimit = false;
  app.add_flag(
      "--zero", zero_delimit,
      "use a literal 0 instead of a line feed to delimit generated strings");
  app.add_option("-s,--seed", seed, "number to seed the prng")
      ->check(CLI::PositiveNumber);
  app.add_option("-o,--offset", command_offset,
                 "skip the first `offset` generated values");
  app.add_option("-c,--count", command_count, "generate `count` values");

  CLI11_PARSE(app, argc, argv);

  if (!seed) {
    std::random_device rd;
    std::default_random_engine generator{rd()};

    seed = std::uniform_int_distribution<std::uint64_t>{
        1, std::numeric_limits<std::uint64_t>::max()}(generator);
  }

  fmt::print(stderr, "Chosen seed: 0x{:x}\n", seed);

  prand rand{seed};

  char delimiter = zero_delimit ? '\0' : '\n';

  for (std::size_t i = 0; i < command_offset + command_count; ++i) {
    auto command = generate_random_update(rand);
    if (i < command_offset) { continue; }
    fmt::print(stdout, "update {}{}", command, delimiter);
    fflush(stdout);
  }
}
