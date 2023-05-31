/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_FILED_FILED_GLOBALS_H_
#define BAREOS_FILED_FILED_GLOBALS_H_

class ConfigurationParser;

namespace filedaemon {
 
__declspec(dllimport) extern ConfigurationParser* my_config;

__declspec(dllimport) extern bool no_signals;
__declspec(dllimport) extern bool backup_only_mode;
__declspec(dllimport) extern bool restore_only_mode;

class ClientResource;
__declspec(dllimport) extern ClientResource* me;
__declspec(dllimport) extern void* start_heap;
__declspec(dllimport) extern char* configfile;

} /* namespace filedaemon */

#endif  // BAREOS_FILED_FILED_GLOBALS_H_
