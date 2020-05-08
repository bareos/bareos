/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_FILED_PYTHON_FD_H_
#define BAREOS_PLUGINS_FILED_PYTHON_FD_H_ 1

#define PYTHON_MODULE_NAME bareosfd
#define PYTHON_MODULE_NAME_QUOTED "bareosfd"

/* common code for all python plugins */
#include "plugins/python_plugins_common.h"
#include "plugins/filed/fd_common.h"

#include "structmember.h"

namespace filedaemon {

/**
 * This defines the arguments that the plugin parser understands.
 */
enum plugin_argument_type
{
  argument_none,
  argument_module_path,
  argument_module_name
};

struct plugin_argument {
  const char* name;
  enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[] = {
    {"module_path", argument_module_path},
    {"module_name", argument_module_name},
    {NULL, argument_none}};

} /* namespace filedaemon */
#endif /* BAREOS_PLUGINS_FILED_PYTHON_FD_H_ */
