
#ifndef BAREOS_CORE_SRC_PLUGINS_FILED_PYTHON_PLUGIN_PRIVATE_CONTEXT_H_
#define BAREOS_CORE_SRC_PLUGINS_FILED_PYTHON_PLUGIN_PRIVATE_CONTEXT_H_

struct plugin_private_context {
  int32_t backup_level; /* Backup level e.g. Full/Differential/Incremental */
  utime_t since;        /* Since time for Differential/Incremental */
  bool python_loaded;   /* Plugin has python module loaded ? */
  bool python_path_set; /* Python plugin search path is set ? */
  char* plugin_options; /* Plugin Option string */
  char* module_path;    /* Plugin Module Path */
  char* module_name;    /* Plugin Module Name */
  char* fname;          /* Next filename to save */
  char* link;           /* Target symlink points to */
  char* object_name;    /* Restore Object Name */
  char* object;         /* Restore Object Content */
  PyThreadState*
      interpreter;   /* Python interpreter for this instance of the plugin */
  PyObject* pModule; /* Python Module entry point */
  PyObject* pyModuleFunctionsDict; /* Python Dictionary */
  BareosCoreFunctions*
      bareos_core_functions; /* pointer to bareos_core_functions */
};                           // namespace filedaemon


#endif  // BAREOS_CORE_SRC_PLUGINS_FILED_PYTHON_PLUGIN_PRIVATE_CONTEXT_H_
