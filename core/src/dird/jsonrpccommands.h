#ifndef JSONRPCCOMMANDS_H
#define JSONRPCCOMMANDS_H

#include "nlohmann/json.hpp"

#include "dird/ua.h"

typedef nlohmann::json json;

namespace directordaemon {

class JsonRpcCommands {
 public:
  JsonRpcCommands();
};

class CommandResourceManager {
 public:
  CommandResourceManager();
  ~CommandResourceManager();

  static bool result_append(void* pm_string, const char* fmt, ...);

  JobControlRecord* jcr_;
  UaContext* ua_;

  std::string GetResponse() const;

 private:
  UaContext* new_ws_ua_context(JobControlRecord* jcr);
  std::string response_text_{};
};


json ListClient(std::string name);
json ListClients();

}  // namespace directordaemon

#endif  // JSONRPCCOMMANDS_H
