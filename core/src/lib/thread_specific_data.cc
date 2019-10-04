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
#include "include/jcr.h"
#include "lib/berrno.h"

static bool jcr_key_initialized = false;

#ifdef HAVE_WIN32
static bool tsd_initialized = false;
static pthread_key_t jcr_key; /* Pointer to jcr for each thread */
#else
#ifdef PTHREAD_ONCE_KEY_NP
static pthread_key_t jcr_key = PTHREAD_ONCE_KEY_NP;
#else
static pthread_key_t jcr_key; /* Pointer to jcr for each thread */
static pthread_once_t key_once = PTHREAD_ONCE_INIT;
#endif
#endif

static void CreateThreadSpecificDataKey()
{
#ifdef PTHREAD_ONCE_KEY_NP
  int status = pthread_key_create_once_np(&jcr_key, nullptr);
#else
  int status = pthread_key_create(&jcr_key, nullptr);
#endif
  if (status != 0) {
    BErrNo be;
    Jmsg1(nullptr, M_ABORT, 0, _("pthread key create failed: ERR=%s\n"),
          be.bstrerror(status));
  } else {
    jcr_key_initialized = true;
  }
}

void SetupThreadSpecificDataKey()
{
#ifdef HAVE_WIN32
  LockJcrChain();
  if (!tsd_initialized) {
    CreateThreadSpecificDataKey();
    tsd_initialized = true;
  }
  UnlockJcrChain();
#else
#ifdef PTHREAD_ONCE_KEY_NP
  CreateThreadSpecificDataKey();
#else
  int status = pthread_once(&key_once, CreateThreadSpecificDataKey);
  if (status != 0) {
    BErrNo be;
    Jmsg1(nullptr, M_ABORT, 0, _("pthread_once failed. ERR=%s\n"),
          be.bstrerror(status));
  }
#endif
#endif
}

JobControlRecord* GetJcrFromThreadSpecificData()
{
  JobControlRecord* jcr = INVALID_JCR;
  if (jcr_key_initialized) {
    jcr = (JobControlRecord*)pthread_getspecific(jcr_key);
  }
  return jcr;
}

void SetJcrInThreadSpecificData(JobControlRecord* jcr)
{
  int status = pthread_setspecific(jcr_key, jcr);
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
