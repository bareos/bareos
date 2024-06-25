/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

BAREOS_IMPORT ConfigurationParser* my_config;

BAREOS_IMPORT bool no_signals;
BAREOS_IMPORT bool backup_only_mode;
BAREOS_IMPORT bool restore_only_mode;

class ClientResource;
BAREOS_IMPORT ClientResource* me;
BAREOS_IMPORT void* start_heap;
BAREOS_IMPORT char* g_filed_configfile;

} /* namespace filedaemon */

#endif  // BAREOS_FILED_FILED_GLOBALS_H_
