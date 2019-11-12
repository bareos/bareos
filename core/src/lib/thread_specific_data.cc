/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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

#include "include/bareos.h"
#include "lib/thread_specific_data.h"
#include "lib/thread_specific_data_key.h"
#include "include/jcr.h"

JobControlRecord* GetJcrFromThreadSpecificData()
{
  return static_cast<JobControlRecord*>(
      pthread_getspecific(ThreadSpecificDataKey::Key()));
}

void SetJcrInThreadSpecificData(JobControlRecord* jcr)
{
  int status = pthread_setspecific(ThreadSpecificDataKey::Key(), jcr);
  if (status != 0) {
    BErrNo be;
    Jmsg1(jcr, M_ABORT, 0, _("pthread_setspecific failed: ERR=%s\n"),
          be.bstrerror(status));
  }
}

void RemoveJcrFromThreadSpecificData(JobControlRecord* jcr)
{
  if (jcr == GetJcrFromThreadSpecificData()) {
    SetJcrInThreadSpecificData(nullptr);
  }
}

uint32_t GetJobIdFromThreadSpecificData()
{
  JobControlRecord* jcr = GetJcrFromThreadSpecificData();
  return (jcr ? jcr->JobId : 0);
}
