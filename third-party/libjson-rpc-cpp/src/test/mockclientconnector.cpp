/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    mockclientconnector.cpp
 * @date    10/9/2014
 * @author  Peter Spiess-Knafl <dev@spiessknafl.at>
 * @license See attached LICENSE.txt
 ************************************************************************/

#include "mockclientconnector.h"

using namespace jsonrpc;
using namespace std;

MockClientConnector::MockClientConnector() {}

string MockClientConnector::GetRequest() { return request; }

Json::Value MockClientConnector::GetJsonRequest() {
  Json::Value result;
  std::istringstream(request) >> result;
  return result;
}

void MockClientConnector::SetResponse(const std::string &response) { this->response = response; }

void MockClientConnector::SendRPCMessage(const string &message, string &result) {
  request = message;
  result = this->response;
}
