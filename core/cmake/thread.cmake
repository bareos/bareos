INCLUDE(CheckIncludeFiles)
INCLUDE(CheckCSourceCompiles)
INCLUDE(CMakePushCheckState)

# check for extra non-portable header-file
CHECK_INCLUDE_FILES("pthread.h;pthread_np.h" HAVE_PTHREAD_NP_H)

# pthread_attr_get_np - e.g. on FreeBSD
CMAKE_PUSH_CHECK_STATE()
SET(CMAKE_REQUIRED_LIBRARIES ${PTHREAD_LIBRARIES})
IF(HAVE_PTHREAD_NP_H)
  CHECK_C_SOURCE_COMPILES("
    #include <pthread.h>
    #include <pthread_np.h>
    int main() { pthread_attr_t a; pthread_attr_get_np(pthread_self(), &a); }
    " HAVE_PTHREAD_ATTR_GET_NP)
ELSE()
  CHECK_C_SOURCE_COMPILES("
    #include <pthread.h>
    int main() { pthread_attr_t a; pthread_attr_get_np(pthread_self(), &a); }
    " HAVE_PTHREAD_ATTR_GET_NP)
ENDIF()
CMAKE_POP_CHECK_STATE()
