/**
 * @file serialportclient.cpp
 * @date 01.11.2019
 * @author Billy Araujo
 * @brief serialportclient.cpp
 */

#include <cstdlib>
#include <iostream>
#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/linuxserialportclient.h>

using namespace jsonrpc;
using namespace std;

int main(int argc, char **argv) {
  string deviceName;

  if (argc == 2) {
    deviceName = string(argv[1]);
  } else {
    deviceName = "tty0";
  }

  cout << "Params are :" << endl;
  cout << "\t deviceName: " << deviceName << endl;

  LinuxSerialPortClient client(deviceName);
  Client c(client);

  Json::Value params;
  params["name"] = "Peter";

  try {
    cout << c.CallMethod("sayHello", params) << endl;
  } catch (JsonRpcException &e) {
    cerr << e.what() << endl;
  }
}
