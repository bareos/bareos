include_directories(/usr/lib/vmware-vix-disklib/include)
link_directories(/usr/lib/vmware-vix-disklib/lib64)
add_executable(bareos_vadp_dumper
   bareos_vadp_dumper.cpp
   cbuf.cpp
   copy_thread.cpp
)
target_link_libraries(bareos_vadp_dumper
   vixDiskLib
   pthread
   jansson
)
INSTALL(TARGETS bareos_vadp_dumper DESTINATION "${CMAKE_INSTALL_SBINDIR}")
INSTALL(FILES bareos_vadp_dumper_wrapper.sh
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
        DESTINATION "${CMAKE_INSTALL_SBINDIR}"
)

