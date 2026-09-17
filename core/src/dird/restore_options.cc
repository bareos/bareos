/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

#include "dird/restore_options.h"

#include "include/bareos.h"

#include <algorithm>
#include <cctype>

namespace directordaemon::restore_options {

std::string ReplacePolicyName(ReplacePolicy policy)
{
  switch (policy) {
    case ReplacePolicy::kAlways:
      return "Always";
    case ReplacePolicy::kIfNewer:
      return "IfNewer";
    case ReplacePolicy::kIfOlder:
      return "IfOlder";
    case ReplacePolicy::kNever:
      return "Never";
  }
  return "Always";
}

std::optional<ReplacePolicy> ParseReplacePolicy(std::string_view policy)
{
  std::string normalized{policy};
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  if (normalized == "always") { return ReplacePolicy::kAlways; }
  if (normalized == "ifnewer") { return ReplacePolicy::kIfNewer; }
  if (normalized == "ifolder") { return ReplacePolicy::kIfOlder; }
  if (normalized == "never") { return ReplacePolicy::kNever; }
  return std::nullopt;
}

std::string QuoteDirectorString(std::string_view value)
{
  std::string result;
  result.reserve(value.size() + 2);
  result += '"';
  for (char ch : value) {
    if (ch == '\\' || ch == '"') { result += '\\'; }
    result += ch;
  }
  result += '"';
  return result;
}

namespace {

void AppendQuoted(std::string& command,
                  std::string_view key,
                  std::string_view value)
{
  if (value.empty()) { return; }
  command += ' ';
  command += key;
  command += '=';
  command += QuoteDirectorString(value);
}

}  // namespace

std::string BuildRestoreRunCommand(const RestoreRunOptions& options)
{
  std::string command = "run job=" + QuoteDirectorString(options.restore_job);
  AppendQuoted(command, "client", options.backup_client);
  AppendQuoted(command, "restoreclient", options.restore_client);
  AppendQuoted(command, "storage", options.storage);
  AppendQuoted(command, "bootstrap", options.bootstrap);

  if (options.files > 0) {
    command += " files=";
    command += std::to_string(options.files);
  }

  AppendQuoted(command, "catalog", options.catalog);
  AppendQuoted(command, "backupformat", options.backup_format);

  if (!options.regex_where.empty()) {
    AppendQuoted(command, "regexwhere", options.regex_where);
  } else {
    AppendQuoted(command, "where", options.where);
  }

  if (!options.replace.empty()) {
    command += " replace=";
    command += options.replace;
  }

  AppendQuoted(command, "pluginoptions", options.plugin_options);
  AppendQuoted(command, "comment", options.comment);
  AppendQuoted(command, "when", options.when);

  if (options.priority && *options.priority > 0) {
    command += " priority=";
    command += std::to_string(*options.priority);
  }

  if (options.yes) { command += " yes"; }
  return command;
}

}  // namespace directordaemon::restore_options
