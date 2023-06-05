#include "bjsonrpcserver.h"
#include "dird/jsonrpccommands.h"

typedef nlohmann::json json;

namespace directordaemon {

BJsonRpcServer::BJsonRpcServer()
{
  this->Add("list", jsonrpccxx::GetHandle(&NoUaListCmd), {"itemtolist"});
}

std::string BJsonRpcServer::ProcessRequest(std::string request)
{
  return HandleRequest(request);
}

}  // namespace directordaemon
