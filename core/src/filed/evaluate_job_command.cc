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

#include "filed/evaluate_job_command.h"
#include "include/bareos.h"

namespace filedaemon {

const std::string JobCommand::jobcmd_{"JobId=%d Job=%127s SDid=%u SDtime=%u Authorization=%100s"};
const std::string JobCommand::jobcmdssl_{
    "JobId=%d Job=%127s SDid=%u SDtime=%u Authorization=%100s ssl=%d\n"};

JobCommand::JobCommand(const char *msg) : job_{0}, sd_auth_key_{0}
{
  ProtocolVersion protocol = ProtocolVersion::kVersionUndefinded;

  std::vector<ProtocolVersion> implemented_protocols{ProtocolVersion::kVersionFrom_18_2,
                                                     ProtocolVersion::KVersionBefore_18_2};

  for (auto protocol_try : implemented_protocols) {
    switch (protocol_try) {
      case ProtocolVersion::kVersionFrom_18_2:
        if (sscanf(msg, jobcmdssl_.c_str(), &job_id_, job_, &vol_session_id_, &vol_session_time_, sd_auth_key_,
                   &tls_policy_) == 6) {
          protocol = protocol_try;
        }
        break;
      case ProtocolVersion::KVersionBefore_18_2:
        if (sscanf(msg, jobcmd_.c_str(), &job_id_, job_, &vol_session_id_, &vol_session_time_, sd_auth_key_) ==
            5) {
          protocol = protocol_try;
        }
        break;
      default:
        /* never */
        break;
    } /* switch () */
    if (protocol != ProtocolVersion::kVersionUndefinded) { break; }
  } /* for ( auto.. */
  protocol_version_ = protocol;
}

bool JobCommand::EvaluationSuccesful() const
{
  return protocol_version_ != ProtocolVersion::kVersionUndefinded;
}

}  // namespace filedaemon
