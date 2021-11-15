/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the
   Free Software Foundation, which is listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/**
 * @file
 * This defines the Python types in C++ and the callbacks from Python we
 * support.
 */

#ifndef BAREOS_PLUGINS_STORED_PYTHON_PYTHON_SD_H_
#define BAREOS_PLUGINS_STORED_PYTHON_PYTHON_SD_H_

#define PYTHON_MODULE_NAME bareossd
#define PYTHON_MODULE_NAME_QUOTED "bareossd"

/* common code for all python plugins */
#include "plugins/include/python_plugins_common.h"
#include "plugins/include/common.h"

namespace storagedaemon {

// This defines the arguments that the plugin parser understands.
enum plugin_argument_type
{
  argument_none,
  argument_instance,
  argument_module_path,
  argument_module_name
};

struct plugin_argument {
  const char* name;
  enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[]
    = {{"instance", argument_instance},
       {"module_path", argument_module_path},
       {"module_name", argument_module_name},
       {NULL, argument_none}};

} /* namespace storagedaemon*/
#endif  // BAREOS_PLUGINS_STORED_PYTHON_PYTHON_SD_H_
