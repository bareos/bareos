#ifndef FD_SENDFILESET_H
#define FD_SENDFILESET_H

#include "include/jcr.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"

namespace directordaemon {

bool SendIncludeExcludeItems(JobControlRecord* jcr, FilesetResource* fileset);

}

#endif  // FD_SENDFILESET_H
