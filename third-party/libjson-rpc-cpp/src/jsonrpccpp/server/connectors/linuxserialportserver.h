/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    linuxserialportserver.h
 * @date    01.11.2019
 * @author  Billy Araujo
 * @license See attached LICENSE.txt
 ************************************************************************/

#ifndef JSONRPC_CPP_LINUXSERIALPORTSERVERCONNECTOR_H_
#define JSONRPC_CPP_LINUXSERIALPORTSERVERCONNECTOR_H_

#include "../../common/streamreader.h"
#include "../../common/streamwriter.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../abstractthreadedserver.h"

namespace jsonrpc {
  /**
   * This class is the Linux/UNIX implementation of TCPSocketServer.
   * It uses the POSIX socket API and POSIX thread API to performs its job.
   * Each client request is handled in a new thread.
   */
  class LinuxSerialPortServer : public AbstractThreadedServer {
  public:
    /**
     * @brief LinuxSerialPortServer, constructor of the Linux/UNIX
     * implementation of class TcpSocketServer
     * @param deviceName The ipv4 address on which the server should
     */
    LinuxSerialPortServer(const std::string &deviceName, size_t threads = 1);

    virtual ~LinuxSerialPortServer();

    virtual bool InitializeListener();
    virtual int CheckForConnection();
    virtual void HandleConnection(int connection);

  protected:
    std::string deviceName;
    int serial_fd;

    StreamReader reader;
    StreamWriter writer;

    // For select operation
    fd_set read_fds;
    struct timeval timeout;
  };

} /* namespace jsonrpc */
#endif /* JSONRPC_CPP_LINUXSERIALPORTSERVERCONNECTOR_H_ */
