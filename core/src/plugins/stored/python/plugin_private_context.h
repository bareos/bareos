
#ifndef BAREOS_CORE_SRC_PLUGINS_STORED_PYTHON_PLUGIN_PRIVATE_CONTEXT_H_
#define BAREOS_CORE_SRC_PLUGINS_STORED_PYTHON_PLUGIN_PRIVATE_CONTEXT_H_

/**
 * Plugin private context
 */
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


#endif  // BAREOS_CORE_SRC_PLUGINS_STORED_PYTHON_PLUGIN_PRIVATE_CONTEXT_H_
