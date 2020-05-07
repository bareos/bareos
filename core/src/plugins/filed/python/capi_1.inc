/* C API functions */

/* static bRC newPlugin(PluginContext* bareos_plugin_ctx); */
#define Bareosfd_newPlugin_NUM 0
#define Bareosfd_newPlugin_RETURN bRC
#define Bareosfd_newPlugin_PROTO (PluginContext* bareos_plugin_ctx)

/* static bRC freePlugin(PluginContext* bareos_plugin_ctx); */
#define Bareosfd_freePlugin_NUM 1
#define Bareosfd_freePlugin_RETURN bRC
#define Bareosfd_freePlugin_PROTO (PluginContext* bareos_plugin_ctx)

/* static bRC getPluginValue(PluginContext* bareos_plugin_ctx, pVariable var, void* value); */
#define Bareosfd_getPluginValue_NUM 2
#define Bareosfd_getPluginValue_RETURN bRC
#define Bareosfd_getPluginValue_PROTO (PluginContext* bareos_plugin_ctx, pVariable var, void* value)

/* static bRC setPluginValue(PluginContext* bareos_plugin_ctx, pVariable var, void* value); */
#define Bareosfd_setPluginValue_NUM 3
#define Bareosfd_setPluginValue_RETURN bRC
#define Bareosfd_setPluginValue_PROTO (PluginContext* bareos_plugin_ctx, pVariable var, void* value)

/* static bRC handlePluginEvent(PluginContext* bareos_plugin_ctx, bEvent* event, void* value); */
#define Bareosfd_handlePluginEvent_NUM 4
#define Bareosfd_handlePluginEvent_RETURN bRC
#define Bareosfd_handlePluginEvent_PROTO (PluginContext* bareos_plugin_ctx, bEvent* event, void* value)

/* static bRC startBackupFile(PluginContext* bareos_plugin_ctx, struct save_pkt* sp); */
#define Bareosfd_startBackupFile_NUM 5
#define Bareosfd_startBackupFile_RETURN bRC
#define Bareosfd_startBackupFile_PROTO (PluginContext* bareos_plugin_ctx, struct save_pkt* sp)

/* static bRC endBackupFile(PluginContext* bareos_plugin_ctx); */
#define Bareosfd_endBackupFile_NUM 6
#define Bareosfd_endBackupFile_RETURN bRC
#define Bareosfd_endBackupFile_PROTO (PluginContext* bareos_plugin_ctx)

/* static bRC pluginIO(PluginContext* bareos_plugin_ctx, struct io_pkt* io); */
#define Bareosfd_pluginIO_NUM 7
#define Bareosfd_pluginIO_RETURN bRC
#define Bareosfd_pluginIO_PROTO (PluginContext* bareos_plugin_ctx, struct io_pkt* io)

/* static bRC startRestoreFile(PluginContext* bareos_plugin_ctx, const char* cmd); */
#define Bareosfd_startRestoreFile_NUM 8
#define Bareosfd_startRestoreFile_RETURN bRC
#define Bareosfd_startRestoreFile_PROTO (PluginContext* bareos_plugin_ctx, const char* cmd)

/* static bRC endRestoreFile(PluginContext* bareos_plugin_ctx); */
#define Bareosfd_endRestoreFile_NUM 9
#define Bareosfd_endRestoreFile_RETURN bRC
#define Bareosfd_endRestoreFile_PROTO (PluginContext* bareos_plugin_ctx)

/* static bRC createFile(PluginContext* bareos_plugin_ctx, struct restore_pkt* rp); */
#define Bareosfd_createFile_NUM 10
#define Bareosfd_createFile_RETURN bRC
#define Bareosfd_createFile_PROTO (PluginContext* bareos_plugin_ctx, struct restore_pkt* rp)

/* static bRC setFileAttributes(PluginContext* bareos_plugin_ctx, struct restore_pkt* rp); */
#define Bareosfd_setFileAttributes_NUM 11
#define Bareosfd_setFileAttributes_RETURN bRC
#define Bareosfd_setFileAttributes_PROTO (PluginContext* bareos_plugin_ctx, struct restore_pkt* rp)

/* static bRC checkFile(PluginContext* bareos_plugin_ctx, char* fname); */
#define Bareosfd_checkFile_NUM 12
#define Bareosfd_checkFile_RETURN bRC
#define Bareosfd_checkFile_PROTO (PluginContext* bareos_plugin_ctx, char* fname)

/* static bRC getAcl(PluginContext* bareos_plugin_ctx, acl_pkt* ap); */
#define Bareosfd_getAcl_NUM 13
#define Bareosfd_getAcl_RETURN bRC
#define Bareosfd_getAcl_PROTO (PluginContext* bareos_plugin_ctx, acl_pkt* ap)

/* static bRC setAcl(PluginContext* bareos_plugin_ctx, acl_pkt* ap); */
#define Bareosfd_setAcl_NUM 14
#define Bareosfd_setAcl_RETURN bRC
#define Bareosfd_setAcl_PROTO (PluginContext* bareos_plugin_ctx, acl_pkt* ap)

/* static bRC getXattr(PluginContext* bareos_plugin_ctx, xattr_pkt* xp); */
#define Bareosfd_getXattr_NUM 15
#define Bareosfd_getXattr_RETURN bRC
#define Bareosfd_getXattr_PROTO (PluginContext* bareos_plugin_ctx, xattr_pkt* xp)

/* static bRC setXattr(PluginContext* bareos_plugin_ctx, xattr_pkt* xp); */
#define Bareosfd_setXattr_NUM 16
#define Bareosfd_setXattr_RETURN bRC
#define Bareosfd_setXattr_PROTO (PluginContext* bareos_plugin_ctx, xattr_pkt* xp)

/* static bRC parse_plugin_definition(PluginContext* bareos_plugin_ctx, void* value, PoolMem& plugin_options); */
#define Bareosfd_parse_plugin_definition_NUM 17
#define Bareosfd_parse_plugin_definition_RETURN bRC
#define Bareosfd_parse_plugin_definition_PROTO (PluginContext* bareos_plugin_ctx, void* value, PoolMem& plugin_options)

/* static bRC PyLoadModule(PluginContext* bareos_plugin_ctx, void* value); */
#define Bareosfd_PyLoadModule_NUM 18
#define Bareosfd_PyLoadModule_RETURN bRC
#define Bareosfd_PyLoadModule_PROTO (PluginContext* bareos_plugin_ctx, void* value)

/* static bRC PyParsePluginDefinition(PluginContext* bareos_plugin_ctx, void* value); */
#define Bareosfd_PyParsePluginDefinition_NUM 19
#define Bareosfd_PyParsePluginDefinition_RETURN bRC
#define Bareosfd_PyParsePluginDefinition_PROTO (PluginContext* bareos_plugin_ctx, void* value)

/* static bRC PyGetPluginValue(PluginContext* bareos_plugin_ctx, pVariable var, void* value); */
#define Bareosfd_PyGetPluginValue_NUM 20
#define Bareosfd_PyGetPluginValue_RETURN bRC
#define Bareosfd_PyGetPluginValue_PROTO (PluginContext* bareos_plugin_ctx, pVariable var, void* value)

/* static bRC PySetPluginValue(PluginContext* bareos_plugin_ctx, pVariable var, void* value); */
#define Bareosfd_PySetPluginValue_NUM 21
#define Bareosfd_PySetPluginValue_RETURN bRC
#define Bareosfd_PySetPluginValue_PROTO (PluginContext* bareos_plugin_ctx, pVariable var, void* value)

/* static bRC PyHandlePluginEvent(PluginContext* bareos_plugin_ctx, bEvent* event, void* value); */
#define Bareosfd_PyHandlePluginEvent_NUM 22
#define Bareosfd_PyHandlePluginEvent_RETURN bRC
#define Bareosfd_PyHandlePluginEvent_PROTO (PluginContext* bareos_plugin_ctx, bEvent* event, void* value)

/* static bRC PyStartBackupFile(PluginContext* bareos_plugin_ctx, struct save_pkt* sp); */
#define Bareosfd_PyStartBackupFile_NUM 23
#define Bareosfd_PyStartBackupFile_RETURN bRC
#define Bareosfd_PyStartBackupFile_PROTO (PluginContext* bareos_plugin_ctx, struct save_pkt* sp)

/* static bRC PyEndBackupFile(PluginContext* bareos_plugin_ctx); */
#define Bareosfd_PyEndBackupFile_NUM 24
#define Bareosfd_PyEndBackupFile_RETURN bRC
#define Bareosfd_PyEndBackupFile_PROTO (PluginContext* bareos_plugin_ctx)

/* static bRC PyPluginIO(PluginContext* bareos_plugin_ctx, struct io_pkt* io); */
#define Bareosfd_PyPluginIO_NUM 25
#define Bareosfd_PyPluginIO_RETURN bRC
#define Bareosfd_PyPluginIO_PROTO (PluginContext* bareos_plugin_ctx, struct io_pkt* io)

/* static bRC PyStartRestoreFile(PluginContext* bareos_plugin_ctx, const char* cmd); */
#define Bareosfd_PyStartRestoreFile_NUM 26
#define Bareosfd_PyStartRestoreFile_RETURN bRC
#define Bareosfd_PyStartRestoreFile_PROTO (PluginContext* bareos_plugin_ctx, const char* cmd)

/* static bRC PyEndRestoreFile(PluginContext* bareos_plugin_ctx); */
#define Bareosfd_PyEndRestoreFile_NUM 27
#define Bareosfd_PyEndRestoreFile_RETURN bRC
#define Bareosfd_PyEndRestoreFile_PROTO (PluginContext* bareos_plugin_ctx)

/* static bRC PyCreateFile(PluginContext* bareos_plugin_ctx, struct restore_pkt* rp); */
#define Bareosfd_PyCreateFile_NUM 28
#define Bareosfd_PyCreateFile_RETURN bRC
#define Bareosfd_PyCreateFile_PROTO (PluginContext* bareos_plugin_ctx, struct restore_pkt* rp)

/* static bRC PySetFileAttributes(PluginContext* bareos_plugin_ctx, struct restore_pkt* rp); */
#define Bareosfd_PySetFileAttributes_NUM 29
#define Bareosfd_PySetFileAttributes_RETURN bRC
#define Bareosfd_PySetFileAttributes_PROTO (PluginContext* bareos_plugin_ctx, struct restore_pkt* rp)

/* static bRC PyCheckFile(PluginContext* bareos_plugin_ctx, char* fname); */
#define Bareosfd_PyCheckFile_NUM 30
#define Bareosfd_PyCheckFile_RETURN bRC
#define Bareosfd_PyCheckFile_PROTO (PluginContext* bareos_plugin_ctx, char* fname)

/* static bRC PyGetAcl(PluginContext* bareos_plugin_ctx, acl_pkt* ap); */
#define Bareosfd_PyGetAcl_NUM 31
#define Bareosfd_PyGetAcl_RETURN bRC
#define Bareosfd_PyGetAcl_PROTO (PluginContext* bareos_plugin_ctx, acl_pkt* ap)

/* static bRC PySetAcl(PluginContext* bareos_plugin_ctx, acl_pkt* ap); */
#define Bareosfd_PySetAcl_NUM 32
#define Bareosfd_PySetAcl_RETURN bRC
#define Bareosfd_PySetAcl_PROTO (PluginContext* bareos_plugin_ctx, acl_pkt* ap)

/* static bRC PyGetXattr(PluginContext* bareos_plugin_ctx, xattr_pkt* xp); */
#define Bareosfd_PyGetXattr_NUM 33
#define Bareosfd_PyGetXattr_RETURN bRC
#define Bareosfd_PyGetXattr_PROTO (PluginContext* bareos_plugin_ctx, xattr_pkt* xp)

/* static bRC PySetXattr(PluginContext* bareos_plugin_ctx, xattr_pkt* xp); */
#define Bareosfd_PySetXattr_NUM 34
#define Bareosfd_PySetXattr_RETURN bRC
#define Bareosfd_PySetXattr_PROTO (PluginContext* bareos_plugin_ctx, xattr_pkt* xp)

/* static bRC PyRestoreObjectData(PluginContext* bareos_plugin_ctx, struct restore_object_pkt* rop); */
#define Bareosfd_PyRestoreObjectData_NUM 35
#define Bareosfd_PyRestoreObjectData_RETURN bRC
#define Bareosfd_PyRestoreObjectData_PROTO (PluginContext* bareos_plugin_ctx, struct restore_object_pkt* rop)

/* static bRC PyHandleBackupFile(PluginContext* bareos_plugin_ctx, struct save_pkt* sp); */
#define Bareosfd_PyHandleBackupFile_NUM 36
#define Bareosfd_PyHandleBackupFile_RETURN bRC
#define Bareosfd_PyHandleBackupFile_PROTO (PluginContext* bareos_plugin_ctx, struct save_pkt* sp)

/* static bRC set_bareos_core_functions(BareosCoreFunctions* new_bareos_core_functions);  */
#define Bareosfd_set_bareos_core_functions_NUM 37
#define Bareosfd_set_bareos_core_functions_RETURN bRC
#define Bareosfd_set_bareos_core_functions_PROTO (BareosCoreFunctions* new_bareos_core_functions )

/*Total Number of C API function pointers */
#define Bareosfd_API_pointers 38
