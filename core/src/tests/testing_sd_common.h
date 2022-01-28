#ifndef TESTING_SD_COMMON_H
#define TESTING_SD_COMMON_H

#include "testing_common.h"

#include "stored/stored_conf.h"
#include "stored/stored_globals.h"

void InitSdGlobals();

PConfigParser StoragePrepareResources(const std::string& path_to_config);

#endif // TESTING_SD_COMMON_H
