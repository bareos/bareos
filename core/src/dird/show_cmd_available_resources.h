/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_DIRD_SHOW_CMD_AVAILABLE_RESOURCES_H_
#define BAREOS_SRC_DIRD_SHOW_CMD_AVAILABLE_RESOURCES_H_

#include <include/bareos.h>
#include "dird/dird_conf.h"

namespace directordaemon {

static struct {
  const char* res_name;
  int type;
} show_cmd_available_resources[] = {{NT_("directors"), R_DIRECTOR},
                                    {NT_("clients"), R_CLIENT},
                                    {NT_("counters"), R_COUNTER},
                                    {NT_("jobs"), R_JOB},
                                    {NT_("jobdefs"), R_JOBDEFS},
                                    {NT_("storages"), R_STORAGE},
                                    {NT_("catalogs"), R_CATALOG},
                                    {NT_("schedules"), R_SCHEDULE},
                                    {NT_("filesets"), R_FILESET},
                                    {NT_("pools"), R_POOL},
                                    {NT_("messages"), R_MSGS},
                                    {NT_("profiles"), R_PROFILE},
                                    {NT_("consoles"), R_CONSOLE},
                                    {NT_("users"), R_USER},
                                    {NT_("all"), -1},
                                    {NT_("help"), -2},
                                    {NULL, 0}};


}  // namespace directordaemon

#endif  // BAREOS_SRC_DIRD_SHOW_CMD_AVAILABLE_RESOURCES_H_
