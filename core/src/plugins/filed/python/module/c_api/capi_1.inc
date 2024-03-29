/* autogenerated file by ../../../../scripts/create_api_includes.sh */
/* bareos-check-sources:disable-copyright-check */

/* C API functions */

/* static bRC PyParsePluginDefinition(PluginContext* bareos_plugin_ctx, void* value); */
#define Bareosfd_PyParsePluginDefinition_NUM 0
#define Bareosfd_PyParsePluginDefinition_RETURN bRC
#define Bareosfd_PyParsePluginDefinition_PROTO (PluginContext* bareos_plugin_ctx, void* value)

/* static bRC PyGetPluginValue(PluginContext* bareos_plugin_ctx, pVariable var, void* value); */
#define Bareosfd_PyGetPluginValue_NUM 1
#define Bareosfd_PyGetPluginValue_RETURN bRC
#define Bareosfd_PyGetPluginValue_PROTO (PluginContext* bareos_plugin_ctx, pVariable var, void* value)

/* static bRC PySetPluginValue(PluginContext* bareos_plugin_ctx, pVariable var, void* value); */
#define Bareosfd_PySetPluginValue_NUM 2
#define Bareosfd_PySetPluginValue_RETURN bRC
#define Bareosfd_PySetPluginValue_PROTO (PluginContext* bareos_plugin_ctx, pVariable var, void* value)

/* static bRC PyHandlePluginEvent(PluginContext* bareos_plugin_ctx, bEvent* event, void* value); */
#define Bareosfd_PyHandlePluginEvent_NUM 3
#define Bareosfd_PyHandlePluginEvent_RETURN bRC
#define Bareosfd_PyHandlePluginEvent_PROTO (PluginContext* bareos_plugin_ctx, bEvent* event, void* value)

/* static bRC PyStartBackupFile(PluginContext* bareos_plugin_ctx, save_pkt* sp); */
#define Bareosfd_PyStartBackupFile_NUM 4
#define Bareosfd_PyStartBackupFile_RETURN bRC
#define Bareosfd_PyStartBackupFile_PROTO (PluginContext* bareos_plugin_ctx, save_pkt* sp)

/* static bRC PyEndBackupFile(PluginContext* bareos_plugin_ctx); */
#define Bareosfd_PyEndBackupFile_NUM 5
#define Bareosfd_PyEndBackupFile_RETURN bRC
#define Bareosfd_PyEndBackupFile_PROTO (PluginContext* bareos_plugin_ctx)

/* static bRC PyPluginIO(PluginContext* bareos_plugin_ctx, io_pkt* io); */
#define Bareosfd_PyPluginIO_NUM 6
#define Bareosfd_PyPluginIO_RETURN bRC
#define Bareosfd_PyPluginIO_PROTO (PluginContext* bareos_plugin_ctx, io_pkt* io)

/* static bRC PyStartRestoreFile(PluginContext* bareos_plugin_ctx, const char* cmd); */
#define Bareosfd_PyStartRestoreFile_NUM 7
#define Bareosfd_PyStartRestoreFile_RETURN bRC
#define Bareosfd_PyStartRestoreFile_PROTO (PluginContext* bareos_plugin_ctx, const char* cmd)

/* static bRC PyEndRestoreFile(PluginContext* bareos_plugin_ctx); */
#define Bareosfd_PyEndRestoreFile_NUM 8
#define Bareosfd_PyEndRestoreFile_RETURN bRC
#define Bareosfd_PyEndRestoreFile_PROTO (PluginContext* bareos_plugin_ctx)

/* static bRC PyCreateFile(PluginContext* bareos_plugin_ctx, restore_pkt* rp); */
#define Bareosfd_PyCreateFile_NUM 9
#define Bareosfd_PyCreateFile_RETURN bRC
#define Bareosfd_PyCreateFile_PROTO (PluginContext* bareos_plugin_ctx, restore_pkt* rp)

/* static bRC PySetFileAttributes(PluginContext* bareos_plugin_ctx, restore_pkt* rp); */
#define Bareosfd_PySetFileAttributes_NUM 10
#define Bareosfd_PySetFileAttributes_RETURN bRC
#define Bareosfd_PySetFileAttributes_PROTO (PluginContext* bareos_plugin_ctx, restore_pkt* rp)

/* static bRC PyCheckFile(PluginContext* bareos_plugin_ctx, char* fname); */
#define Bareosfd_PyCheckFile_NUM 11
#define Bareosfd_PyCheckFile_RETURN bRC
#define Bareosfd_PyCheckFile_PROTO (PluginContext* bareos_plugin_ctx, char* fname)

/* static bRC PyGetAcl(PluginContext* bareos_plugin_ctx, acl_pkt* ap); */
#define Bareosfd_PyGetAcl_NUM 12
#define Bareosfd_PyGetAcl_RETURN bRC
#define Bareosfd_PyGetAcl_PROTO (PluginContext* bareos_plugin_ctx, acl_pkt* ap)

/* static bRC PySetAcl(PluginContext* bareos_plugin_ctx, acl_pkt* ap); */
#define Bareosfd_PySetAcl_NUM 13
#define Bareosfd_PySetAcl_RETURN bRC
#define Bareosfd_PySetAcl_PROTO (PluginContext* bareos_plugin_ctx, acl_pkt* ap)

/* static bRC PyGetXattr(PluginContext* bareos_plugin_ctx, xattr_pkt* xp); */
#define Bareosfd_PyGetXattr_NUM 14
#define Bareosfd_PyGetXattr_RETURN bRC
#define Bareosfd_PyGetXattr_PROTO (PluginContext* bareos_plugin_ctx, xattr_pkt* xp)

/* static bRC PySetXattr(PluginContext* bareos_plugin_ctx, xattr_pkt* xp); */
#define Bareosfd_PySetXattr_NUM 15
#define Bareosfd_PySetXattr_RETURN bRC
#define Bareosfd_PySetXattr_PROTO (PluginContext* bareos_plugin_ctx, xattr_pkt* xp)

/* static bRC PyRestoreObjectData(PluginContext* bareos_plugin_ctx, restore_object_pkt* rop); */
#define Bareosfd_PyRestoreObjectData_NUM 16
#define Bareosfd_PyRestoreObjectData_RETURN bRC
#define Bareosfd_PyRestoreObjectData_PROTO (PluginContext* bareos_plugin_ctx, restore_object_pkt* rop)

/* static bRC PyHandleBackupFile(PluginContext* bareos_plugin_ctx, save_pkt* sp); */
#define Bareosfd_PyHandleBackupFile_NUM 17
#define Bareosfd_PyHandleBackupFile_RETURN bRC
#define Bareosfd_PyHandleBackupFile_PROTO (PluginContext* bareos_plugin_ctx, save_pkt* sp)

/* static bRC set_bareos_core_functions(CoreFunctions* new_bareos_core_functions); */
#define Bareosfd_set_bareos_core_functions_NUM 18
#define Bareosfd_set_bareos_core_functions_RETURN bRC
#define Bareosfd_set_bareos_core_functions_PROTO (CoreFunctions* new_bareos_core_functions)

/* static bRC set_plugin_context(PluginContext* new_plugin_context); */
#define Bareosfd_set_plugin_context_NUM 19
#define Bareosfd_set_plugin_context_RETURN bRC
#define Bareosfd_set_plugin_context_PROTO (PluginContext* new_plugin_context)

/*Total Number of C API function pointers */
#define Bareosfd_API_pointers 20
