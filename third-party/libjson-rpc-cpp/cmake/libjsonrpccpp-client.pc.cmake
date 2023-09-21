Name: libjsonrpccpp-client
Description: A C++ client implementation of json-rpc.
Version: ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}
Libs: -L${CMAKE_INSTALL_FULL_LIBDIR} -ljsoncpp -ljsonrpccpp-common -ljsonrpccpp-client ${CLIENT_LIBS}
Cflags: -I${CMAKE_INSTALL_FULL_INCLUDEDIR}
