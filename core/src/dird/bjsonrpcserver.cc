#include "bjsonrpcserver.h"
#include "dird/jsonrpccommands.h"

typedef nlohmann::json json;

namespace directordaemon {

BJsonRpcServer::BJsonRpcServer()
{
  this->Add("list client", jsonrpccxx::GetHandle(&ListClient), {"name"});
  this->Add("list clients", jsonrpccxx::GetHandle(&ListClients));
}

std::string BJsonRpcServer::ProcessRequest(std::string request)
{
  return HandleRequest(request);
}

}  // namespace directordaemon
