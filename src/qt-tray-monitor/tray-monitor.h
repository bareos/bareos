/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
/*
 * Includes specific to the tray monitor
 *
 * Nicolas Boichat, August MMIV
 */

#ifndef TRAY_MONITOR_H
#define TRAY_MONITOR_H

struct cl_opts
{
   char *configfile;
   bool test_config_only;
   bool export_config;
   bool export_config_schema;
   cl_opts () {
	  configfile = static_cast<char*>(0);
	  test_config_only = false;
	  export_config = false;
	  export_config_schema = false;
   }
};

class MonitorItem;
class MONITORRES;

void refresh_item();
const MONITORRES* getMonitor();

#endif  /* TRAY_MONITOR_H */
