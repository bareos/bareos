/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    serialportclient.cpp
 * @date    01.01.2019
 * @author  Billy Araujo
 * @license See attached LICENSE.txt
 ************************************************************************/

#include "linuxserialportclient.h"
#include "../../common/sharedconstants.h"
#include "../../common/streamreader.h"
#include "../../common/streamwriter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>

using namespace jsonrpc;
using namespace std;

LinuxSerialPortClient::LinuxSerialPortClient(const std::string &deviceName) : deviceName(deviceName) {}

LinuxSerialPortClient::~LinuxSerialPortClient() {}

void LinuxSerialPortClient::SendRPCMessage(const std::string &message, std::string &result) {
  int serial_fd = this->Connect();

  StreamWriter writer;
  string toSend = message + DEFAULT_DELIMITER_CHAR;
  if (!writer.Write(toSend, serial_fd)) {
    throw JsonRpcException(Errors::ERROR_CLIENT_CONNECTOR, "Could not write request");
  }

  StreamReader reader(DEFAULT_BUFFER_SIZE);
  if (!reader.Read(result, serial_fd, DEFAULT_DELIMITER_CHAR)) {
    throw JsonRpcException(Errors::ERROR_CLIENT_CONNECTOR, "Could not read response");
  }
  close(serial_fd);
}

int LinuxSerialPortClient::Connect() {

  int serial_fd = open(deviceName.c_str(), O_RDWR);

  return serial_fd;
}

int LinuxSerialPortClient::Connect(const string &deviceName) {

  int serial_fd;

  try {
    serial_fd = open(deviceName.c_str(), O_RDWR);
  } catch (const JsonRpcException &e) {
    serial_fd = -1;
  }

  return serial_fd;
}
