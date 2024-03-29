/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2022 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_UA_RUN_H_
#define BAREOS_DIRD_UA_RUN_H_

#include "dird/ua.h"

namespace directordaemon {

struct RerunArguments {
  int since_jobid = 0;
  int until_jobid = 0;
  int days = 0;
  int hours = 0;
  bool yes = false;
  std::vector<JobId_t> JobIds;
  bool parsingerror = false;
};

RerunArguments GetRerunCmdlineArguments(UaContext* ua);
bool reRunCmd(UaContext* ua, const char* cmd);
bool RunCmd(UaContext* ua, const char* cmd);
int DoRunCmd(UaContext* ua, const char* cmd);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_RUN_H_
