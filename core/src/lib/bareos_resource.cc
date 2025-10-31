/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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

const char* GetResourceName(const void* resource)
{
  const char* resource_name
      = static_cast<const BareosResource*>(resource)->resource_name_;
  // This should only be the case for devices that where multiplied, i.e. count
  // > 1 to avoid naming collision with its implicit autochanger.
  if (resource_name[0] == '$') { return resource_name + 1; }
  return resource_name;
}

BareosResource::BareosResource(const BareosResource& other)
{
  /* do not copy next_ because that is part of the resource chain */
  next_ = nullptr;
  resource_name_
      = other.resource_name_ ? strdup(other.resource_name_) : nullptr;
  description_ = other.description_ ? strdup(other.description_) : nullptr;
  rcode_ = other.rcode_;
  refcnt_ = other.refcnt_;
  item_present_ = other.item_present_;
  ::memcpy(inherit_content_, other.inherit_content_, MAX_RES_ITEMS);
}

bool BareosResource::Validate() { return true; }

BareosResource& BareosResource::operator=(const BareosResource& rhs)
{
  next_ = rhs.next_;
  resource_name_ = rhs.resource_name_;
  description_ = rhs.description_;
  rcode_ = rhs.rcode_;
  refcnt_ = rhs.refcnt_;
  item_present_ = rhs.item_present_;
  ::memcpy(inherit_content_, rhs.inherit_content_, MAX_RES_ITEMS);
  return *this;
}
