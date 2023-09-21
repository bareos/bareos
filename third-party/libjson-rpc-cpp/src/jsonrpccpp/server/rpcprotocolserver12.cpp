/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    rpcprotocolserver12.cpp
 * @date    10/25/2014
 * @author  Peter Spiess-Knafl <dev@spiessknafl.at>
 * @license See attached LICENSE.txt
 ************************************************************************/

#include "rpcprotocolserver12.h"
#include <jsonrpccpp/common/jsonparser.h>

using namespace jsonrpc;
using namespace std;

RpcProtocolServer12::RpcProtocolServer12(IProcedureInvokationHandler &handler) : rpc1(handler), rpc2(handler) {}

void RpcProtocolServer12::AddProcedure(const Procedure &procedure) {
  this->rpc1.AddProcedure(procedure);
  this->rpc2.AddProcedure(procedure);
}

void RpcProtocolServer12::HandleRequest(const std::string &request, std::string &retValue) {
  Json::Value req;
  Json::Value resp;
  Json::StreamWriterBuilder wbuilder;
  wbuilder["indentation"] = "";

  try {
    istringstream(request) >> req;
    this->GetHandler(req).HandleJsonRequest(req, resp);
  } catch (const Json::Exception &e) {
    this->GetHandler(req).WrapError(Json::nullValue, Errors::ERROR_RPC_JSON_PARSE_ERROR, Errors::GetErrorMessage(Errors::ERROR_RPC_JSON_PARSE_ERROR), resp);
  }

  if (resp != Json::nullValue)
    retValue = Json::writeString(wbuilder, resp);
}

AbstractProtocolHandler &RpcProtocolServer12::GetHandler(const Json::Value &request) {
  if (request.isArray() || (request.isObject() && request.isMember("jsonrpc") && request["jsonrpc"].asString() == "2.0"))
    return rpc2;
  return rpc1;
}
