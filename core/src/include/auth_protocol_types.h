/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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

#ifndef BAREOS_SRC_INCLUDE_AUTH_PROTOCOL_TYPES_H_
#define BAREOS_SRC_INCLUDE_AUTH_PROTOCOL_TYPES_H_

enum AuthenticationProtocolType
{
  APT_NATIVE = 0,
  APT_NDMPV2,
  APT_NDMPV3,
  APT_NDMPV4
};

#endif  // BAREOS_SRC_INCLUDE_AUTH_PROTOCOL_TYPES_H_
