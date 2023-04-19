#ifndef BAREOS_INCLUDE_FCNTL_DEF_H_
#define BAREOS_INCLUDE_FCNTL_DEF_H_

#include <fcntl.h>

/* O_NOATIME is defined at fcntl.h when supported */
#ifndef O_NOATIME
#  define O_NOATIME 0
#endif

#endif  // BAREOS_INCLUDE_FCNTL_DEF_H_
