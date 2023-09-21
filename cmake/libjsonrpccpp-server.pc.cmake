Name: libjsonrpccpp-server
Description: A C++ server implementation of json-rpc.
Version: ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}
Libs: -L${CMAKE_INSTALL_FULL_LIBDIR} -ljsoncpp -ljsonrpccpp-common -ljsonrpccpp-server ${SERVER_LIBS}
Cflags: -I${CMAKE_INSTALL_FULL_INCLUDEDIR}
