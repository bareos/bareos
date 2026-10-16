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
#ifndef BAREOS_DIRD_UA_ACCT_INTERNAL_H_
#define BAREOS_DIRD_UA_ACCT_INTERNAL_H_

#include "dird/ua.h"

#include <atomic>
#include <string>
#include <vector>

namespace directordaemon {

struct AccountingRow {
  std::string ClientName;
  std::string FileSetName;
  bool excluded{false};
  std::string exclusion_reason;
  uint64_t files{0};
  uint64_t bytes{0};
  uint64_t logical_bytes{0};
  std::string rule;
  uint64_t jobs_in_chain{0};
};

struct SnapshotMetadata {
  std::string timestamp;
  std::string last_error;
  bool has_snapshot{false};
  bool stale{true};
};

// Computes the accounting rows per (Client, FileSet) from File.LStat.
bool CalculateSubscriptionAccounting(UaContext* ua,
                                     std::vector<AccountingRow>* rows,
                                     std::string* error,
                                     const std::atomic_bool* cancel = nullptr);

// Persistent snapshot of the accounting rows in the catalog.
bool LoadSubscriptionAccountingSnapshot(UaContext* ua,
                                        const char* client_filter,
                                        const char* fileset_filter,
                                        SnapshotMetadata* metadata,
                                        std::vector<AccountingRow>* rows);
bool RecordSubscriptionAccountingFailure(UaContext* ua,
                                         const std::string& error);
bool SaveSubscriptionAccountingSnapshot(UaContext* ua,
                                        const std::vector<AccountingRow>& rows);

}  // namespace directordaemon
#endif  // BAREOS_DIRD_UA_ACCT_INTERNAL_H_
