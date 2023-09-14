#ifndef COMMANDRESOURCEMANAGER_H
#define COMMANDRESOURCEMANAGER_H

#include "dird/ua.h"
#include "lib/bsock_tcp.h"
#include "cats/cats.h"

namespace directordaemon {

class CommandResourceManager {
 public:
  CommandResourceManager();
  ~CommandResourceManager();

  static bool result_append(void* pm_string, const char* fmt, ...);

  JobControlRecord* jcr_;
  UaContext* ua_;

  std::string GetResponse();

 private:
  UaContext* new_ws_ua_context(JobControlRecord* jcr, int api);

  std::string response_text_{};
};

class EmptySocket : public BareosSocketTCP {
 public:
  EmptySocket() {}
  ~EmptySocket() { destroy(); }

  bool send() override { return true; }
};

}  // namespace directordaemon

#endif  // COMMANDRESOURCEMANAGER_H
