/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the Free
   Software Foundation, which is listed in the file LICENSE.

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
 * This defines the Python types in C++ and the callbacks from Python we support.
 */

#ifndef BPYTHONDIR_H
#define BPYTHONDIR_H 1

/*
 * This defines the arguments that the plugin parser understands.
 */
enum plugin_argument_type {
   argument_none,
   argument_instance,
   argument_module_path,
   argument_module_name
};

struct plugin_argument {
   const char *name;
   enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[] = {
   { "instance", argument_instance },
   { "module_path", argument_module_path },
   { "module_name", argument_module_name },
   { NULL, argument_none }
};
/*
 * Callback methods from Python.
 */
static PyObject *PyBareosGetValue(PyObject *self, PyObject *args);
static PyObject *PyBareosSetValue(PyObject *self, PyObject *args);
static PyObject *PyBareosDebugMessage(PyObject *self, PyObject *args);
static PyObject *PyBareosJobMessage(PyObject *self, PyObject *args);
static PyObject *PyBareosRegisterEvents(PyObject *self, PyObject *args);
static PyObject *PyBareosUnRegisterEvents(PyObject *self, PyObject *args);
static PyObject *PyBareosGetInstanceCount(PyObject *self, PyObject *args);

static PyMethodDef BareosDIRMethods[] = {
   { "GetValue", PyBareosGetValue, METH_VARARGS, "Get a Plugin value" },
   { "SetValue", PyBareosSetValue, METH_VARARGS, "Set a Plugin value" },
   { "DebugMessage", PyBareosDebugMessage, METH_VARARGS, "Print a Debug message" },
   { "JobMessage", PyBareosJobMessage, METH_VARARGS, "Print a Job message" },
   { "RegisterEvents", PyBareosRegisterEvents, METH_VARARGS, "Register Plugin Events" },
   { "UnRegisterEvents", PyBareosUnRegisterEvents, METH_VARARGS, "Unregister Plugin Events" },
   { "GetInstanceCount", PyBareosGetInstanceCount, METH_VARARGS, "Get number of instances of current plugin" },
   { NULL, NULL, 0, NULL }
};

#endif /* BPYTHONDIR_H */
