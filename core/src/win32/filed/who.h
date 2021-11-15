/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
// Kern Sibbald, August 2007

#ifndef BAREOS_WIN32_FILED_WHO_H_
#define BAREOS_WIN32_FILED_WHO_H_

#define APP_NAME "Bareos-fd"
#define LC_APP_NAME "bareos-fd"
#define APP_DESC "Bareos File Backup Service"
#define SERVICE_DESC \
  "Provides file backup and restore services (bareos client)."

#define TerminateApp(x) filedaemon::TerminateFiled(x)
namespace filedaemon {
void TerminateFiled(int sig);
} /* namespace filedaemon */

#endif  // BAREOS_WIN32_FILED_WHO_H_
