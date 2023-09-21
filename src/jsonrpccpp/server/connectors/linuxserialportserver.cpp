/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    linuxserialportserver.cpp
 * @date    01.11.2019
 * @author  Billy Araujo
 * @license See attached LICENSE.txt
 ************************************************************************/

#include "linuxserialportserver.h"
#include "../../common/sharedconstants.h"
#include "../../common/streamreader.h"
#include "../../common/streamwriter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <errno.h>
#include <iostream>
#include <sstream>
#include <string>

using namespace jsonrpc;
using namespace std;

#define BUFFER_SIZE 1024
#ifndef DELIMITER_CHAR
#define DELIMITER_CHAR char(0x0A)
#endif

#define READ_TIMEOUT 0.001 // Set timeout in seconds

LinuxSerialPortServer::LinuxSerialPortServer(const std::string &deviceName, size_t threads)
    : AbstractThreadedServer(threads), deviceName(deviceName), reader(DEFAULT_BUFFER_SIZE) {}

LinuxSerialPortServer::~LinuxSerialPortServer() { close(this->serial_fd); }

bool LinuxSerialPortServer::InitializeListener() {

  serial_fd = open(deviceName.c_str(), O_RDWR);

  return serial_fd >= 0;
}

int LinuxSerialPortServer::CheckForConnection() {
  FD_SET(serial_fd, &read_fds);
  timeout.tv_sec = 0;
  timeout.tv_usec = (suseconds_t)(READ_TIMEOUT * 1000000);
  // Wait for something to read
  return select(serial_fd + 1, &read_fds, nullptr, nullptr, &timeout);
}

void LinuxSerialPortServer::HandleConnection(int connection) {
  (void)(connection);
  string request, response;
  reader.Read(request, serial_fd, DEFAULT_DELIMITER_CHAR);
  this->ProcessRequest(request, response);
  response.append(1, DEFAULT_DELIMITER_CHAR);
  writer.Write(response, serial_fd);
}
