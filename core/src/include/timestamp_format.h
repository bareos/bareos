/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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

#ifndef BAREOS_INCLUDE_TIMESTAMP_FORMAT_H_
#define BAREOS_INCLUDE_TIMESTAMP_FORMAT_H_

static constexpr char* kBareosDefaultTimestampFormat
    = (char*)"%Y-%m-%dT%H:%M:%S";
// %z is missing because it is not correctly implemented in windows :(
// As we optionally want to offer microseconds, we implement the %z ourselves on
// all platforms

// for the scheduler preview
static constexpr char* kBareosSchedPreviewTimestampFormat
    = (char*)"%a %d-%b-%Y %H:%M";


// for use in TO_CHAR database queries
static constexpr char* kBareosDatabaseDefaultTimestampFormat
    = (char*)"YYYY-MM-DD\"T\"HH24:MI:SSTZH:TZM";

// to create filenames, so only what is allowed in filenames also on windows
static constexpr char* kBareosFilenameTimestampFormat
    = (char*)"%Y-%m-%dT%H.%M.%S";

#endif  // BAREOS_INCLUDE_TIMESTAMP_FORMAT_H_
