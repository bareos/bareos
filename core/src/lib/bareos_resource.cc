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

#include "lib/bareos_resource.h"

BareosResource::BareosResource()
    : next_(nullptr)
    , resource_name_(nullptr)
    , description_(nullptr)
    , rcode_(0)
    , refcnt_(0)
    , item_present_{0}
    , inherit_content_{0}
{
  return;
}

BareosResource::BareosResource(const BareosResource& other)
{
  /* do not copy next_ because that is part of the resource chain */
  next_ = nullptr;
  resource_name_ =
      other.resource_name_ ? bstrdup(other.resource_name_) : nullptr;
  description_ = other.description_ ? bstrdup(other.description_) : nullptr;
  rcode_ = other.rcode_;
  refcnt_ = other.refcnt_;
  ::memcpy(item_present_, other.item_present_, MAX_RES_ITEMS);
  ::memcpy(inherit_content_, other.inherit_content_, MAX_RES_ITEMS);
}

BareosResource& BareosResource::operator=(const BareosResource& rhs)
{
  next_ = rhs.next_;
  resource_name_ = rhs.resource_name_;
  description_ = rhs.description_;
  rcode_ = rhs.rcode_;
  refcnt_ = rhs.refcnt_;
  ::memcpy(item_present_, rhs.item_present_, MAX_RES_ITEMS);
  ::memcpy(inherit_content_, rhs.inherit_content_, MAX_RES_ITEMS);
  return *this;
}
