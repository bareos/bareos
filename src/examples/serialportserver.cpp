/**
 * @file serialportserver.cpp
 * @date 01.11.2019
 * @author Billy Araujo
 * @brief serialportserver.cpp
 */

#include <cstdlib>
#include <iostream>
#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/linuxserialportserver.h>
#include <stdio.h>
#include <string>

using namespace jsonrpc;
using namespace std;

class SampleServer : public AbstractServer<SampleServer> {
public:
  SampleServer(LinuxSerialPortServer &server) : AbstractServer<SampleServer>(server) {
    this->bindAndAddMethod(Procedure("sayHello", PARAMS_BY_NAME, JSON_STRING, "name", JSON_STRING, NULL), &SampleServer::sayHello);
    this->bindAndAddNotification(Procedure("notifyServer", PARAMS_BY_NAME, NULL), &SampleServer::notifyServer);
  }

  // method
  void sayHello(const Json::Value &request, Json::Value &response) { response = "Hello: " + request["name"].asString(); }

  // notification
  void notifyServer(const Json::Value &request) {
    (void)request;
    cout << "server received some Notification" << endl;
  }
};

int main(int argc, char **argv) {
  try {
    string deviceName;

    if (argc == 2) {
      deviceName = string(argv[1]);
    } else {
      deviceName = "tty0";
    }

    cout << "Params are :" << endl;
    cout << "\t deviceName: " << deviceName << endl;

    LinuxSerialPortServer server(deviceName);
    SampleServer serv(server);
    if (serv.StartListening()) {
      cout << "Server started successfully" << endl;
      getchar();
      serv.StopListening();
    } else {
      cout << "Error starting Server" << endl;
    }
  } catch (jsonrpc::JsonRpcException &e) {
    cerr << e.what() << endl;
  }
}
