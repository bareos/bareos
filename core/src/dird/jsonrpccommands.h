#ifndef JSONRPCCOMMANDS_H
#define JSONRPCCOMMANDS_H

#include "nlohmann/json.hpp"

typedef nlohmann::json json;

namespace directordaemon {

class JsonRpcCommands {
 public:
  JsonRpcCommands();
};


json NoUaListCmd(std::string item);

}  // namespace directordaemon

#endif  // JSONRPCCOMMANDS_H
