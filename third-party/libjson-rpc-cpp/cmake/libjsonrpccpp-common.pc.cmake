Name: libjsonrpccpp-common
Description: Common libraries for libjson-rpc-cpp
Version: ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}
Libs: -L${CMAKE_INSTALL_FULL_LIBDIR} -ljsoncpp
Cflags: -I${CMAKE_INSTALL_FULL_INCLUDEDIR}
