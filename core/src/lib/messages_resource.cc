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
#include "lib/message_destination_info.h"

#include <algorithm>

pthread_mutex_t MessagesResource::mutex_ = PTHREAD_MUTEX_INITIALIZER;

void MessagesResource::ShallowCopyTo(BareosResource* p) const
{
  MessagesResource* r = dynamic_cast<MessagesResource*>(p);
  if (r) { *r = *this; }
}

MessagesResource::~MessagesResource()
{
  for (MessageDestinationInfo* d : dest_chain_) { delete d; }
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

std::vector<MessageDestinationInfo*> MessagesResource::DuplicateDestChain()
    const
{
  std::vector<MessageDestinationInfo*> temp_chain;

  for (MessageDestinationInfo* d : dest_chain_) {
    MessageDestinationInfo* dnew = new MessageDestinationInfo(*d);
    dnew->file_pointer_ = nullptr;
    dnew->mail_filename_.clear();
    temp_chain.push_back(dnew);
  }

  return temp_chain;
}

void MessagesResource::DuplicateResourceTo(MessagesResource& other) const
{
  other.dest_chain_ = DuplicateDestChain();
  memcpy(other.send_msg_types_, send_msg_types_, sizeof(send_msg_types_));
}

/*
 * Called only during parsing of the config file.
 *
 * Note, where in the case of dest_code FILE is a filename,
 * but in the case of MAIL is a space separated list of
 * email addresses, ...
 */
bool MessagesResource::AddToExistingChain(MessageDestinationCode dest_code,
                                          int msg_type,
                                          const std::string& where)
{
  auto pos = std::find_if(dest_chain_.begin(), dest_chain_.end(),
                          [&dest_code](MessageDestinationInfo* d) {
                            return d->dest_code_ == dest_code;
                          });

  if (pos != dest_chain_.end()) {
    MessageDestinationInfo* d = *pos;
    bool append = false;

    if (where.empty() && d->where_.empty()) {
      append = true;
    } else if (where == d->where_) {
      append = true;
    }

    if (append) {
      Dmsg4(850, "Add to existing d=%p msgtype=%d destcode=%d where=%s\n", pos,
            msg_type, dest_code, NSTDPRNT(where));
      SetBit(msg_type, d->msg_types_);
      SetBit(msg_type, send_msg_types_);
      return true;
    }
  }
  return false;
}

void MessagesResource::AddToNewChain(MessageDestinationCode dest_code,
                                     int msg_type,
                                     const std::string& where,
                                     const std::string& mail_cmd,
                                     const std::string& timestamp_format)
{
  MessageDestinationInfo* d;
  d = new MessageDestinationInfo;
  d->dest_code_ = dest_code;
  SetBit(msg_type, d->msg_types_);   /* Set type bit in structure */
  SetBit(msg_type, send_msg_types_); /* Set type bit in our local */

  d->where_ = where;
  d->mail_cmd_ = mail_cmd;
  d->timestamp_format_ = timestamp_format;

  dest_chain_.push_back(d);

  Dmsg6(850,
        "add new d=%p msgtype=%d destcode=%d where=%s mailcmd=%s "
        "timestampformat=%s\n",
        d, msg_type, dest_code, NSTDPRNT(where), NSTDPRNT(d->mail_cmd_),
        NSTDPRNT(d->timestamp_format_));
}

void MessagesResource::AddMessageDestination(
    MessageDestinationCode dest_code,
    int msg_type,
    const std::string& where,
    const std::string& mail_cmd,
    const std::string& timestamp_format)
{
  if (!AddToExistingChain(dest_code, msg_type, where)) {
    AddToNewChain(dest_code, msg_type, where, mail_cmd, timestamp_format);
  }
}

/*
 * Called only during parsing of the config file.
 */
void MessagesResource::RemoveMessageDestination(
    MessageDestinationCode dest_code,
    int msg_type,
    const std::string& where)
{
  for (MessageDestinationInfo* d : dest_chain_) {
    Dmsg2(850, "Remove_msg_dest d=%p where=%s\n", d, NSTDPRNT(d->where_));
    if (BitIsSet(msg_type, d->msg_types_) && (dest_code == d->dest_code_) &&
        ((where.empty() && d->where_.empty()) || (where == d->where_))) {
      Dmsg3(850, "Found for remove d=%p msgtype=%d destcode=%d\n", d, msg_type,
            dest_code);
      ClearBit(msg_type, d->msg_types_);
      Dmsg0(850, "Return RemoveMessageDestination\n");
      return;
    }
  }
}
