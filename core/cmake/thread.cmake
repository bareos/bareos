include(CheckIncludeFiles)
include(CheckCSourceCompiles)
include(CMakePushCheckState)

# check for extra non-portable header-file
check_include_files("pthread.h;pthread_np.h" HAVE_PTHREAD_NP_H)

# pthread_attr_get_np - e.g. on FreeBSD
cmake_push_check_state()
set(CMAKE_REQUIRED_LIBRARIES ${PTHREAD_LIBRARIES})
if(HAVE_PTHREAD_NP_H)
  check_c_source_compiles("
    #include <pthread.h>
    #include <pthread_np.h>
    int main() { pthread_attr_t a; pthread_attr_get_np(pthread_self(), &a); }
    " HAVE_PTHREAD_ATTR_GET_NP)
else()
  check_c_source_compiles("
    #include <pthread.h>
    int main() { pthread_attr_t a; pthread_attr_get_np(pthread_self(), &a); }
    " HAVE_PTHREAD_ATTR_GET_NP)
endif()
cmake_pop_check_state()
