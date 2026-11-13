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
#ifndef BAREOS_DIRD_UA_ACCT_H_
#define BAREOS_DIRD_UA_ACCT_H_

#include "dird/ua.h"

#include <cstddef>
#include <cstdint>

namespace directordaemon {

struct SubscriptionAccountingThreadStatus {
  bool available{false};
  bool queued{false};
  bool running{false};
  uint64_t generation{0};
};

// Append persisted accounting details to the unified subscription report.
bool ReportSubscriptionAccounting(UaContext* ua,
                                  bool anonymize,
                                  size_t pending_jobs);
bool ReportSubscriptionAccountingStatus(UaContext* ua);

bool StartSubscriptionAccountingThread();
void StopSubscriptionAccountingThread();
bool RequestSubscriptionAccountingRefresh(UaContext* ua);
bool QueueSubscriptionAccountingJob(JobId_t jobid);
size_t QueueMissingSubscriptionAccounting(UaContext* ua,
                                          const char* client_filter = nullptr,
                                          const char* fileset_filter = nullptr);
SubscriptionAccountingThreadStatus GetSubscriptionAccountingThreadStatus();

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_ACCT_H_
