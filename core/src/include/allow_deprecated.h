/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#ifndef BAREOS_INCLUDE_ALLOW_DEPRECATED_H_
#define BAREOS_INCLUDE_ALLOW_DEPRECATED_H_

#define ALLOW_DEPRECATED(deprecated_expression)                       \
  _Pragma("GCC diagnostic push")                                      \
      _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"") \
          deprecated_expression;                                      \
  _Pragma("GCC diagnostic pop")

#endif  // BAREOS_INCLUDE_ALLOW_DEPRECATED_H_
