/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2023 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "dird/job_trigger.h"

std::string JobTriggerToString(JobTrigger trig)
{
  switch (trig) {
    case JobTrigger::kScheduler:
      return T_("Scheduler");
    case JobTrigger::kClient:
      return T_("Client");
    case JobTrigger::kUser:
      return T_("User");
    default:
    case JobTrigger::kUndefined:
      return T_("Unknown");
  }
}
