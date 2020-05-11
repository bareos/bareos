/* C API functions */

/* static bRC PyParsePluginDefinition(PluginContext* plugin_ctx, void* value); */
#define Bareosdir_PyParsePluginDefinition_NUM 0
#define Bareosdir_PyParsePluginDefinition_RETURN bRC
#define Bareosdir_PyParsePluginDefinition_PROTO (PluginContext* plugin_ctx, void* value)

/* static bRC PyGetPluginValue(PluginContext* plugin_ctx, pVariable var, void* value); */
#define Bareosdir_PyGetPluginValue_NUM 1
#define Bareosdir_PyGetPluginValue_RETURN bRC
#define Bareosdir_PyGetPluginValue_PROTO (PluginContext* plugin_ctx, pVariable var, void* value)

/* static bRC PySetPluginValue(PluginContext* plugin_ctx, pVariable var, void* value); */
#define Bareosdir_PySetPluginValue_NUM 2
#define Bareosdir_PySetPluginValue_RETURN bRC
#define Bareosdir_PySetPluginValue_PROTO (PluginContext* plugin_ctx, pVariable var, void* value)

/* static bRC PyHandlePluginEvent(PluginContext* plugin_ctx, bDirEvent* event, void* value); */
#define Bareosdir_PyHandlePluginEvent_NUM 3
#define Bareosdir_PyHandlePluginEvent_RETURN bRC
#define Bareosdir_PyHandlePluginEvent_PROTO (PluginContext* plugin_ctx, bDirEvent* event, void* value)

/* static bRC set_bareos_core_functions( DirectorCoreFunctions* new_bareos_core_functions); */
#define Bareosdir_set_bareos_core_functions_NUM 4
#define Bareosdir_set_bareos_core_functions_RETURN bRC
#define Bareosdir_set_bareos_core_functions_PROTO ( DirectorCoreFunctions* new_bareos_core_functions)

/* static bRC set_plugin_context(PluginContext* new_plugin_context); */
#define Bareosdir_set_plugin_context_NUM 5
#define Bareosdir_set_plugin_context_RETURN bRC
#define Bareosdir_set_plugin_context_PROTO (PluginContext* new_plugin_context)

/*Total Number of C API function pointers */
#define Bareosdir_API_pointers 6
