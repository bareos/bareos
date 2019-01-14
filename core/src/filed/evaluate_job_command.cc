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

#include "filed/dir_cmd.h"
#include "include/bareos.h"

const std::string EvaluateJobcommand::jobcmd { "JobId=%d Job=%127s SDid=%u SDtime=%u Authorization=%100s" };
const std::string EvaluateJobcommand::jobcmdssl { "JobId=%d Job=%127s SDid=%u SDtime=%u Authorization=%100s ssl=%d\n" };

namespace filedaemon {

class EvaluateJobcommand
{
  public:
    EvaluateJobcommand();
     : uint32_t JobId(0)
     , Job[24]
     , VolSessionId(0)
     , VolSessionTime(0)
    PoolMem sd_auth_key;
     , tls_policy(TlsPolicy::kBnetTlsUnknown)


    uint32_t JobId;
    char Job[24];
    uint32_t VolSessionId;
    uint32_t VolSessionTime;
    PoolMem sd_auth_key;
    TlsPolicy tls_policy = TlsPolicy::kBnetTlsUnknown;

  private:
    std::string jobcmd { "JobId=%d Job=%127s SDid=%u SDtime=%u Authorization=%100s" };
    std::string jobcmdssl { "JobId=%d Job=%127s SDid=%u SDtime=%u Authorization=%100s ssl=%d\n" };
};

JobMessageProtocolVersion EvaluateJobcommand(const POOLMEM *msg,
                                             uint32_t &JobId,
                                             char *Job,
                                             uint32_t &VolSessionId,
                                             uint32_t &VolSessionTime,
                                             PoolMem &sd_auth_key,
                                             TlsPolicy &tls_policy)
{
  int tries = 2;
  JobMessageProtocolVersion successful_protocol = JobMessageProtocolVersion::kVersionUndefinded;

  for (int protocol_try = 1; !successful_protocol && protocol_try <= tries; protocol_try++) {
    switch (protocol_try) {
      case JobMessageProtocolVersion::kVersionFrom_18_2:
        if (sscanf(msg, jobcmdssl, &JobId, Job, &VolSessionId, &VolSessionTime,
                   sd_auth_key.c_str(), &tls_policy) == 6) {
          successful_protocol = static_cast<JobMessageProtocolVersion>(protocol_try);
        }
        break;
      case JobMessageProtocolVersion::KVersionBefore_18_2:
        if (sscanf(msg, jobcmd, &JobId, Job, &VolSessionId, &VolSessionTime,
                   sd_auth_key.c_str()) == 5) {
          successful_protocol = static_cast<JobMessageProtocolVersion>(protocol_try);
        }
        break;
      default:
        break;
    }
  }
  return successful_protocol;
}

} /* namespace filedamon */
