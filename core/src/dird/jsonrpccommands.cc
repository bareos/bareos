#include "jsonrpccommands.h"

namespace directordaemon {

JsonRpcCommands::JsonRpcCommands() {}

json NoUaListCmd(std::string item)
{
  json result;


  if (item == "client") {
    result = {{"clients", {{{"name", "client-1"}}, {{"name", "client-2"}}}}};
  }

  return result;
}

}  // namespace directordaemon
