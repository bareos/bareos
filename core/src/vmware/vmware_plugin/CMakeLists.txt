get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)
if("${LIB64}" STREQUAL "TRUE")
  set(LIBSUFFIX 64)
else()
  set(LIBSUFFIX "")
endif()
set(INSTALL_LIB_DIR
    "lib${LIBSUFFIX}"
    CACHE PATH "Installation directory for libraries"
)
mark_as_advanced(INSTALL_LIB_DIR)

install(
  FILES BareosFdPluginVMware.py bareos-fd-vmware.py
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${INSTALL_LIB_DIR}/bareos/plugins"
)
install(DIRECTORY
        DESTINATION "${INSTALL_LIB_DIR}/bareos/plugins/vmware_plugin/"
)
install(
  # as this command is not handled by "INSTALL DESTINATION",
  # the target path must be evaluated manually.
  CODE "
        execute_process(COMMAND ln -sf \
            ../bareos-fd-vmware.py     \
            \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${INSTALL_LIB_DIR}/bareos/plugins/vmware_plugin/bareos-fd-vmware.py
        )
    "
)
