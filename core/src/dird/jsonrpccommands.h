#ifndef BAREOS_DIRD_JSONRPCCOMMANDS_H_
#define BAREOS_DIRD_JSONRPCCOMMANDS_H_

#include "dird/bjsonrpcserver.h"

namespace directordaemon {

std::string ListClient(std::string name);
std::string ListClients();
std::string ListFileset(int jobid, const std::string& jobname);
std::string ListFilesets();

}  // namespace directordaemon

#endif  // BAREOS_DIRD_JSONRPCCOMMANDS_H_
