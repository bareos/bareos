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
   char *configfile_;
   bool test_config_only_;
   bool export_config_;
   bool export_config_schema_;
   bool do_connection_test_only_;
   cl_opts () {
      configfile_ = nullptr;
      test_config_only_ = false;
      export_config_ = false;
      export_config_schema_ = false;
      do_connection_test_only_ = false;
   }
};

class MonitorItem;
class MonitorResource;

void refresh_item();
const MonitorResource* getMonitor();

#endif  /* TRAY_MONITOR_H */
