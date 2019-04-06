/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#include "lib/messages_resource.h"

pthread_mutex_t MessagesResource::mutex_ = PTHREAD_MUTEX_INITIALIZER;

MessagesResource::MessagesResource()
    : send_msg_types_{0}

    , in_use_(false)
    , closing_(false)
{
  return;
}

void MessagesResource::ShallowCopyTo(BareosResource* p) const
{
  MessagesResource* r = dynamic_cast<MessagesResource*>(p);
  if (r) { *r = *this; }
}

MessagesResource::~MessagesResource()
{
  for (DEST* d : dest_chain_) { delete d; }
}

void MessagesResource::lock() { P(mutex_); }

void MessagesResource::unlock() { V(mutex_); }

void MessagesResource::WaitNotInUse()
{
  /* leaves fides_mutex set */
  lock();
  while (in_use_ || closing_) {
    unlock();
    Bmicrosleep(0, 200);
    lock();
  }
}

void MessagesResource::ClearInUse()
{
  lock();
  in_use_ = false;
  unlock();
}
void MessagesResource::SetInUse()
{
  WaitNotInUse();
  in_use_ = true;
  unlock();
}

void MessagesResource::SetClosing() { closing_ = true; }

bool MessagesResource::GetClosing() { return closing_; }

void MessagesResource::ClearClosing()
{
  lock();
  closing_ = false;
  unlock();
}

bool MessagesResource::IsClosing()
{
  lock();
  bool rtn = closing_;
  unlock();
  return rtn;
}
