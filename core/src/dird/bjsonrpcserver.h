#ifndef BJSONRPCSERVER_H
#define BJSONRPCSERVER_H

#include "jsonrpccxx/server.hpp"

namespace directordaemon {

class JsonRpcServer {
 public:
  virtual ~JsonRpcServer() {}

  virtual std::string ProcessRequest(const std::string request) = 0;
};

class BJsonRpcServer
    : public JsonRpcServer
    , jsonrpccxx::JsonRpc2Server {
 public:
  BJsonRpcServer();

  std::string ProcessRequest(std::string request) override;
};

}  // namespace directordaemon

#endif  // BJSONRPCSERVER_H
