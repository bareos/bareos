#ifndef BJSONRPCSERVER_H
#define BJSONRPCSERVER_H

#include "jsonrpccxx/server.hpp"

namespace directordaemon {

class BJsonRpcServer : public jsonrpccxx::JsonRpc2Server {
 public:
  BJsonRpcServer();
};

}  // namespace directordaemon

#endif  // BJSONRPCSERVER_H
