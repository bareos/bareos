/**
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_FILED_EVALUATE_JOB_COMMAND_H_
#define BAREOS_FILED_EVALUATE_JOB_COMMAND_H_

#include "include/bareos.h"

namespace filedaemon {

class JobCommand {
 public:
  enum class ProtocolVersion
  {
    kVersionUndefinded,
    kVersionFrom_18_2,
    KVersionBefore_18_2
  };

 public:
  JobCommand(const char *msg);
  JobCommand(const JobCommand &) = delete;
  bool EvaluationSuccesful() const;

  uint32_t job_id_ = 0;
  char job_[MAX_NAME_LENGTH];
  uint32_t vol_session_id_   = 0;
  uint32_t vol_session_time_ = 0;
  char sd_auth_key_[MAX_NAME_LENGTH];
  TlsPolicy tls_policy_             = TlsPolicy::kBnetTlsUnknown;

  ProtocolVersion protocol_version_ = ProtocolVersion::kVersionUndefinded;

 private:
  static const std::string jobcmd_;
  static const std::string jobcmdssl_;
};
} /* namespace filedaemon */

#endif /* BAREOS_FILED_EVALUATE_JOB_COMMAND_H_ */
