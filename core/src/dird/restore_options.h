/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_RESTORE_OPTIONS_H_
#define BAREOS_DIRD_RESTORE_OPTIONS_H_

#include <cstdint>
#include <optional>
#include <string>

namespace directordaemon::restore_options {

inline constexpr std::string_view kDefaultRelocationExamplePath
    = "/home/alice/documents/report.pdf";
inline constexpr std::string_view kDefaultCustomRegexWhere
    = "!^/home/!/restore/home/!i";

enum class ReplacePolicy
{
  kAlways,
  kIfNewer,
  kIfOlder,
  kNever,
};

struct RelocationRules {
  std::string strip_prefix;
  std::string add_prefix;
  std::string add_suffix;
  std::string regex_where;
};

struct RestoreRunOptions {
  std::string restore_job;
  std::string backup_client;
  std::string restore_client;
  std::string storage;
  std::string bootstrap;
  uint32_t files = 0;
  std::string catalog;
  std::string backup_format;
  std::string where;
  std::string regex_where;
  std::string replace;
  std::string plugin_options;
  std::string comment;
  std::string when;
  std::optional<uint32_t> priority;
  bool yes = false;
};

std::string ReplacePolicyName(ReplacePolicy policy);
std::optional<ReplacePolicy> ParseReplacePolicy(std::string_view policy);
std::string QuoteDirectorString(std::string_view value);
std::string BuildRestoreRunCommand(const RestoreRunOptions& options);
std::optional<std::string> PreviewRegexWhere(std::string_view regex_where,
                                             std::string_view path);

}  // namespace directordaemon::restore_options

#endif  // BAREOS_DIRD_RESTORE_OPTIONS_H_
