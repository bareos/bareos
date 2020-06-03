include_directories(${VIXDISKLIB_INCLUDE_DIRS})
get_filename_component(
  VIX_DISKLIB_LIBRARY_DIR ${VIXDISKLIB_LIBRARIES} DIRECTORY
)
link_directories(${VIX_DISKLIB_LIBRARY_DIR})

add_executable(bareos_vadp_dumper bareos_vadp_dumper.cc cbuf.cc copy_thread.cc)

target_link_libraries(bareos_vadp_dumper vixDiskLib pthread jansson)

install(TARGETS bareos_vadp_dumper DESTINATION "${CMAKE_INSTALL_SBINDIR}")
install(
  FILES bareos_vadp_dumper_wrapper.sh
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
              WORLD_READ WORLD_EXECUTE
  DESTINATION "${CMAKE_INSTALL_SBINDIR}"
)
