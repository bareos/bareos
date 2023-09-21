/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    LinuxSerialPortClient.h
 * @date    01.11.2019
 * @author  Billy Araujo
 * @license See attached LICENSE.txt
 ************************************************************************/

#ifndef JSONRPC_CPP_LINUXSERIALPORTCLIENT_H_
#define JSONRPC_CPP_LINUXSERIALPORTCLIENT_H_

#include "../iclientconnector.h"
#include <jsonrpccpp/common/exception.h>
#include <string>

namespace jsonrpc {
  /**
   * This class is the Linux/Unix implementation of LinuxSerialPortClient.
   * It uses the POSIX socket API to performs its job.
   */
  class LinuxSerialPortClient : public IClientConnector {
  public:
    /**
     * @brief LinuxSerialPortClient, constructor of the Linux/UNIX implementation of class TcpSocketClient
     * @param deviceName The device name on which the client should try to connect
     */
    LinuxSerialPortClient(const std::string &deviceName);
    /**
     * @brief ~LinuxSerialPortClient, the destructor of LinuxSerialPortClient
     */
    virtual ~LinuxSerialPortClient();
    /**
     * @brief The real implementation of TcpSocketClient::SendRPCMessage method.
     * @param message The message to send
     * @param result The result of the call returned by the server
     * @throw JsonRpcException Thrown when an issue is encountered with socket manipulation (see message of exception for more information about what happened).
     */
    virtual void SendRPCMessage(const std::string &message, std::string &result);

  protected:
    int fd;
    std::string deviceName; /*!< The serial port device name on which the client should try to connect*/
    /**
     * @brief Connects to the serial port provided by constructor parameters.
     *
     * @returns A file descriptor to the successfully connected socket
     * @throw JsonRpcException Thrown when an issue is encountered while trying to connect (see message of exception for more information about what happened).
     */
    int Connect();
    /**
     * @param deviceName The device name to connect to
     * @returns A file descriptor to the successfully connected socket
     * @throw JsonRpcException Thrown when an issue is encountered while trying to connect (see message of exception for more information about what happened).
     */
    int Connect(const std::string &deviceName);
  };

} /* namespace jsonrpc */
#endif /* JSONRPC_CPP_LINUXSERIALPORTCLIENT_H_ */
