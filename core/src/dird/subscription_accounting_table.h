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
#ifndef BAREOS_DIRD_SUBSCRIPTION_ACCOUNTING_TABLE_H_
#define BAREOS_DIRD_SUBSCRIPTION_ACCOUNTING_TABLE_H_

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace directordaemon {

using SubscriptionAccountingTableRow = std::array<std::string, 7>;

inline std::string SubscriptionAccountingRuleCode(const std::string& rule)
{
  if (rule == "st_blocks*512") { return "B"; }
  if (rule == "st_size") { return "S"; }
  return rule;
}

inline std::string FormatSubscriptionAccountingTable(
    const SubscriptionAccountingTableRow& header,
    const std::vector<SubscriptionAccountingTableRow>& rows,
    const SubscriptionAccountingTableRow& total,
    bool show_status)
{
  const size_t columns = show_status ? header.size() : header.size() - 1;
  std::array<size_t, 7> widths{};
  for (size_t i = 0; i < columns; ++i) {
    widths[i] = std::max(header[i].size(), total[i].size());
    for (const auto& row : rows) {
      widths[i] = std::max(widths[i], row[i].size());
    }
  }
  std::string output;
  const auto append_row = [&](const SubscriptionAccountingTableRow& row) {
    for (size_t i = 0; i < columns; ++i) {
      if (i != 0) { output += "  "; }
      const size_t padding = widths[i] - row[i].size();
      const bool right_aligned = i == 1 || i == 2 || i == 3 || i == 5;
      if (right_aligned) { output.append(padding, ' '); }
      output += row[i];
      if (!right_aligned) { output.append(padding, ' '); }
    }
    while (!output.empty() && output.back() == ' ') { output.pop_back(); }
    output += '\n';
  };
  std::string separator;
  for (size_t i = 0; i < columns; ++i) {
    if (i != 0) { separator += "  "; }
    separator.append(widths[i], '-');
  }
  separator += '\n';
  append_row(header);
  output += separator;
  for (const auto& row : rows) { append_row(row); }
  output += separator;
  append_row(total);
  return output;
}

}  // namespace directordaemon
#endif  // BAREOS_DIRD_SUBSCRIPTION_ACCOUNTING_TABLE_H_
