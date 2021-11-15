/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_DIRD_PYTHON_PLUGIN_PRIVATE_CONTEXT_H_
#define BAREOS_PLUGINS_DIRD_PYTHON_PLUGIN_PRIVATE_CONTEXT_H_
// Plugin private context
struct plugin_private_context {
  int64_t instance;     /* Instance number of plugin */
  bool python_loaded;   /* Plugin has python module loaded ? */
  bool python_path_set; /* Python plugin search path is set ? */
  char* module_path;    /* Plugin Module Path */
  char* module_name;    /* Plugin Module Name */
  PyThreadState*
      interpreter;   /* Python interpreter for this instance of the plugin */
  PyObject* pModule; /* Python Module entry point */
  PyObject* pyModuleFunctionsDict; /* Python Dictionary */
};

#endif  // BAREOS_PLUGINS_DIRD_PYTHON_PLUGIN_PRIVATE_CONTEXT_H_
