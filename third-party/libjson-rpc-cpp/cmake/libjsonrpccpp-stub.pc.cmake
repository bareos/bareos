Name: libjsonrpccpp-stub
Description: library for stub-generation of libjson-rpc-cpp servers/clients.
Version: ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}
Libs: -L${CMAKE_INSTALL_FULL_LIBDIR} -ljsoncpp -ljsonrpccpp-common
Cflags: -I${CMAKE_INSTALL_FULL_INCLUDEDIR}
