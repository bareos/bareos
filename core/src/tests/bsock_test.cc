/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#include "bsock_test.h"
#include "create_resource.h"
#include "gtest/gtest.h"
#include <iostream>
#include <fstream>
#include <string>

#include <thread>
#include <future>

#include "include/bareos.h"
#include "console/console.h"
#include "console/console_conf.h"
#include "stored/stored_conf.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"

#include "lib/tls_openssl.h"
#include "lib/bnet.h"

#include "include/jcr.h"

#define CLIENT_AS_A_THREAD  0

class UaContext {
  BareosSocket *UA_sock;
};
class StorageResource;
#include "dird/authenticate.h"

#define MIN_MSG_LEN 15
#define MAX_MSG_LEN 175 + 25
#define CONSOLENAME "testconsole"
#define CONSOLEPASSWORD "secret"

static std::string cipher_server, cipher_client;

static std::unique_ptr<directordaemon::ConsoleResource> dir_cons_config;
static std::unique_ptr<directordaemon::DirectorResource> dir_dir_config;
static std::unique_ptr<console::DirectorResource> cons_dir_config;
static std::unique_ptr<console::ConsoleResource> cons_cons_config;

void InitForTest()
{
  dir_cons_config.reset(directordaemon::CreateAndInitializeNewConsoleResource());
  dir_dir_config.reset(directordaemon::CreateAndInitializeNewDirectorResource());
  cons_dir_config.reset(console::CreateAndInitializeNewDirectorResource());
  cons_cons_config.reset(console::CreateAndInitializeNewConsoleResource());

  directordaemon::me = dir_dir_config.get();
  console::me = cons_cons_config.get();

  setlocale(LC_ALL, "");
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  debug_level = 0;
  SetTrace(0);

  working_directory = "/tmp";
  MyNameIs(0, NULL, "bsock_test");
  InitMsg(NULL, NULL);
}

static int create_listening_server_socket(int port)
{
  int server_fd;
  int opt = 1;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    return -1;
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt");
    return -1;
  }

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    return -1;
  }

  struct timeval timeout;
  timeout.tv_sec  = 1;  // after 1 seconds connect() will timeout
  timeout.tv_usec = 0;

  if (setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("setsockopt");
    return -1;
  }

  if (listen(server_fd, 3) < 0) {
    perror("listen");
    return -1;
  }
  return server_fd;
}

static int accept_server_socket(int listening_socket)
{
  int new_socket = accept(listening_socket, nullptr, nullptr);
  if (new_socket < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      perror("Socket accept timeout");
    }
    return -1;
  }
  return new_socket;
}

int create_accepted_server_socket(int port)
{
  int sock = create_listening_server_socket(port);
  if (sock > 0) {
    sock = accept_server_socket(sock);
  }
  return sock;
}

BareosSocket *create_new_bareos_socket(int fd)
{
  BareosSocket *bs;
  bs = New(BareosSocketTCP);
  bs->sleep_time_after_authentication_error = 0;
  bs->fd_ = fd;
  bs->SetWho(bstrdup("client"));
  memset(&bs->peer_addr, 0, sizeof(bs->peer_addr));
  return bs;
}

#if 0
static bool check_cipher(const TlsResource &tls, const std::string &cipher)
{
   bool success = false;
   if (tls.tls_cert.IsActivated() && !tls.tls_psk.IsActivated()) { /* cert && !psk */
      success = cipher.find("-RSA-") != std::string::npos;
   } else if (!tls.tls_cert.IsActivated() && tls.tls_psk.IsActivated()) { /* !cert && psk */
      success = cipher.find("-PSK-") != std::string::npos;
   }
   return success;
}
#endif

static void clone_a_server_socket(BareosSocket* bs)
{
   std::unique_ptr<BareosSocket> bs2(bs->clone());
   bs2->fsend("cloned-bareos-socket-0987654321");
   bs2->close();

   bs->fsend("bareos-socket-1234567890");
}

void start_bareos_server(std::promise<bool> *promise, std::string console_name,
                         std::string console_password, std::string server_address, int server_port)

{
  int newsockfd = create_accepted_server_socket(server_port);

  if (newsockfd < 0) {
     return;
  }

  std::unique_ptr<BareosSocket> bs(create_new_bareos_socket(newsockfd));

  char *name = (char *)console_name.c_str();
  s_password *password = new (s_password);
  password->encoding = p_encoding_md5;
  password->value = (char *)console_password.c_str();

  bool success = false;
  if (bs->recv() <= 0) {
    Dmsg1(10, _("Connection request from %s failed.\n"), bs->who());
  } else if (bs->message_length < MIN_MSG_LEN || bs->message_length > MAX_MSG_LEN) {
    Dmsg2(10, _("Invalid connection from %s. Len=%d\n"), bs->who(), bs->message_length);
  } else {
    Dmsg1(10, "Cons->Dir: %s", bs->msg);
    if (!bs->AuthenticateInboundConnection(NULL, "Console", name, *password, dir_cons_config.get())) {
      Dmsg0(10, "Server: inbound auth failed\n");
    } else {
      bs->fsend(_("1000 OK: %s Version: %s (%s)\n"), my_name, VERSION, BDATE);
      Dmsg0(10, "Server: inbound auth successful\n");
      std::string cipher;
      if (bs->tls_conn) {
         cipher = bs->tls_conn->TlsCipherGetName();
         Dmsg1(10, "Server used cipher: <%s>\n", cipher.c_str());
         cipher_server = cipher;
      }
      if (dir_cons_config->tls_psk.IsActivated() || dir_cons_config->tls_cert.IsActivated()) {
         Dmsg0(10, bs->TlsEstablished() ? "Tls enable\n" : "Tls failed to establish\n");
         success = bs->TlsEstablished();
      } else {
         Dmsg0(10, "Tls disabled by command\n");
         if (bs->TlsEstablished()) {
            Dmsg0(10, "bs->tls_established_ should be false but is true\n");
         }
         success = !bs->TlsEstablished();
      }
    }
  }
  if (success) {
    clone_a_server_socket(bs.get());
  }
  bs->close();
  promise->set_value(success);
}

void clone_a_client_socket(std::shared_ptr<BareosSocket> UA_sock)
{
   std::string received_msg;
   std::string orig_msg2("cloned-bareos-socket-0987654321");
   std::unique_ptr<BareosSocket> UA_sock2(UA_sock->clone());
   UA_sock2->recv();
   received_msg = UA_sock2->msg;
   EXPECT_STREQ(orig_msg2.c_str(), received_msg.c_str());
   UA_sock2->close();

   std::string orig_msg("bareos-socket-1234567890");
   UA_sock->recv();
   received_msg = UA_sock->msg;
   EXPECT_STREQ(orig_msg.c_str(), received_msg.c_str());
}

#if CLIENT_AS_A_THREAD
int connect_to_server(std::string console_name, std::string console_password,
                      std::string server_address, int server_port)
#else
bool connect_to_server(std::string console_name, std::string console_password,
                      std::string server_address, int server_port)
#endif
{
  utime_t heart_beat = 0;
  char errmsg[1024];
  int errmsg_len = sizeof(errmsg);

  JobControlRecord jcr;
  memset(&jcr, 0, sizeof(jcr));

  char *name = (char *)console_name.c_str();

  s_password *password = new (s_password);
  password->encoding = p_encoding_md5;
  password->value = (char *)console_password.c_str();

  std::shared_ptr<BareosSocketTCP> UA_sock(New(BareosSocketTCP));
  UA_sock->sleep_time_after_authentication_error = 0;
  jcr.dir_bsock = UA_sock.get();

  bool success = false;

  if (!UA_sock->connect(NULL, 1, 15, heart_beat, "Director daemon", (char *)server_address.c_str(),
                        NULL, server_port, false)) {
    Dmsg0(10, "socket connect failed\n");
  } else {
    Dmsg0(10, "socket connect OK\n");
    if (!UA_sock->AuthenticateWithDirector(&jcr, name, *password, errmsg, errmsg_len, cons_dir_config.get())) {
      Emsg0(M_ERROR, 0, "Authenticate Failed\n");
    } else {
      Dmsg0(10, "Authenticate Connect to Server successful!\n");
      std::string cipher;
      if (UA_sock->tls_conn) {
         cipher = UA_sock->tls_conn->TlsCipherGetName();
         Dmsg1(10, "Client used cipher: <%s>\n", cipher.c_str());
         cipher_client = cipher;
      }
      if (cons_dir_config->tls_psk.IsActivated() || cons_dir_config->tls_cert.IsActivated()) {
         Dmsg0(10, UA_sock->TlsEstablished() ? "Tls enable\n" : "Tls failed to establish\n");
         success = UA_sock->TlsEstablished();
      } else {
         Dmsg0(10, "Tls disabled by command\n");
         if (UA_sock->TlsEstablished()) {
            Dmsg0(10, "UA_sock->tls_established_ should be false but is true\n");
         }
         success = !UA_sock->TlsEstablished();
      }
    }
  }
  if (success) {
    clone_a_client_socket(UA_sock);
  }
  if (UA_sock) {
   UA_sock->close();
  }
  return success;
}

std::string client_cons_name;
std::string client_cons_password;

std::string server_cons_name;
std::string server_cons_password;

int port = BSOCK_TEST_PORT_NUMBER;

TEST(bsock, auth_works)
{
  port++;
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  client_cons_name = "clientname";
  client_cons_password = "verysecretpassword";

  server_cons_name = client_cons_name;
  server_cons_password = client_cons_password;

  InitForTest();

  cons_dir_config->tls_psk.enable_ = false;
  dir_cons_config->tls_psk.enable_ = false;

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, server_cons_name, server_cons_password,
                            HOST, port);

  Dmsg0(10, "connecting to server\n");
  EXPECT_TRUE(connect_to_server(client_cons_name, client_cons_password, HOST, port));

  server_thread.join();
  EXPECT_TRUE(future.get());
}


TEST(bsock, auth_works_with_different_names)
{
  port++;
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  client_cons_name = "clientname";
  client_cons_password = "verysecretpassword";

  server_cons_name = "differentclientname";
  server_cons_password = client_cons_password;

  InitForTest();

  cons_dir_config->tls_psk.enable_ = false;
  dir_cons_config->tls_psk.enable_ = false;

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, server_cons_name, server_cons_password,
                            HOST, port);

  Dmsg0(10, "connecting to server\n");
  EXPECT_TRUE(connect_to_server(client_cons_name, client_cons_password, HOST, port));

  server_thread.join();
  EXPECT_TRUE(future.get());
}

TEST(bsock, auth_fails_with_different_passwords)
{
  port++;
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  client_cons_name = "clientname";
  client_cons_password = "verysecretpassword";

  server_cons_name = client_cons_name;
  server_cons_password = "othersecretpassword";

  InitForTest();

  cons_dir_config->tls_psk.enable_ = false;
  dir_cons_config->tls_psk.enable_ = false;

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, server_cons_name, server_cons_password,
                            HOST, port);

  Dmsg0(10, "connecting to server\n");
  EXPECT_FALSE(connect_to_server(client_cons_name, client_cons_password, HOST, port));

  server_thread.join();
  EXPECT_FALSE(future.get());
}

TEST(bsock, auth_works_with_tls_cert)
{
  port++;
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  client_cons_name = "clientname";
  client_cons_password = "verysecretpassword";

  server_cons_name = client_cons_name;
  server_cons_password = client_cons_password;

  InitForTest();

  cons_dir_config->tls_psk.enable_ = true;
  cons_dir_config->tls_cert.enable_ = true;
  dir_cons_config->tls_cert.enable_ = true;

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, server_cons_name, server_cons_password,
                            HOST, port);

  Dmsg0(10, "connecting to server\n");

#if CLIENT_AS_A_THREAD
  std::thread client_thread(connect_to_server, client_cons_name, client_cons_password, HOST, port, cons_dir_config.get());
  client_thread.join();
#else
  EXPECT_TRUE(connect_to_server(client_cons_name, client_cons_password, HOST, port));
#endif

  server_thread.join();

  EXPECT_TRUE(cipher_server == cipher_client);
  EXPECT_TRUE(future.get());
}

#if 0
TEST(bsock, auth_works_with_tls_psk)
{
  port++;
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  InitForTest();

  cons_dir_config->tls_psk.enable = true;
  cons_dir_config->tls_cert.enable = true;
  dir_cons_config->tls_psk.enable = true;

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, dir_cons_config->hdr.name, dir_cons_config->password.value,
                            HOST, port);

  Dmsg0(10, "connecting to server\n");

#if CLIENT_AS_A_THREAD
  std::thread client_thread(connect_to_server, console::me->hdr.name, cons_dir_config->password.value, HOST, port, cons_dir_config.get());
  client_thread.join();
#else
  EXPECT_TRUE(connect_to_server(console::me->hdr.name, cons_dir_config->password.value, HOST, port));
#endif

  server_thread.join();

  EXPECT_TRUE(cipher_server == cipher_client);
  EXPECT_TRUE(future.get());
}

TEST(bsock, auth_fails_with_different_names_with_tls_psk)
{
  port++;
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  InitForTest();

  dir_cons_config->hdr.name = (char*)"differentclientname";
  dir_cons_config->password.value = (char*)"verysecretpassword";

  cons_dir_config->tls_psk.enable = true;
  cons_dir_config->tls_cert.enable = true;
  dir_cons_config->tls_psk.enable = true;

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, dir_cons_config->hdr.name, dir_cons_config->password.value,
                            HOST, port);

  Dmsg0(10, "connecting to server\n");

#if CLIENT_AS_A_THREAD
  std::thread client_thread(connect_to_server, console::me->hdr.name, cons_dir_config->password.value, HOST, port, cons_dir_config.get());
  client_thread.join();
#else
  EXPECT_FALSE(connect_to_server(console::me->hdr.name, cons_dir_config->password.value, HOST, port));
#endif

  server_thread.join();

  EXPECT_TRUE(cipher_server == cipher_client);
  EXPECT_FALSE(future.get());
}
#endif

bool create_connected_server_and_client_bareos_socket(BareosSocketTCP **client_socket_out,
                                               BareosSocket **server_socket_out)
{
  int newsockfd = create_listening_server_socket(BSOCK_TEST_PORT_NUMBER);

  if (newsockfd < 0) { return false; }

  std::unique_ptr<BareosSocketTCP> client_socket(New(BareosSocketTCP));
  client_socket->sleep_time_after_authentication_error = 0;

  std::string hostaddr(HOST);

  bool ok = client_socket->connect(NULL, 1, 1, 0, "Director daemon", hostaddr.c_str(),
                        NULL, BSOCK_TEST_PORT_NUMBER, false);
  EXPECT_EQ(ok, true);
  if (!ok) {
    Dmsg0(10, "socket connect failed\n");
    return false;
  }

  newsockfd = accept_server_socket(newsockfd);
  EXPECT_GT(newsockfd, 0);
  if (newsockfd <= 0) { return false; }

  std::unique_ptr<BareosSocket> server_socket(create_new_bareos_socket(newsockfd));

  *server_socket_out = server_socket.release();
  *client_socket_out = client_socket.release();
  return true;
}

TEST(BNet, FormatAndSendResponseMessage)
{
  BareosSocket *s;
  std::unique_ptr<BareosSocket> server_socket;
  BareosSocketTCP *c;
  std::unique_ptr<BareosSocketTCP> client_socket;

  bool ok = create_connected_server_and_client_bareos_socket(&c, &s);
  EXPECT_TRUE(ok);
  if (!ok) { return; }

  server_socket.reset(s);
  client_socket.reset(c);

  std::string m("Test123");

  FormatAndSendResponseMessage(client_socket.get(), kMessageIdOk, m);

  uint32_t id = kMessageIdUnknown;
  std::string m1;
  ok = ReceiveAndEvaluateResponseMessage(server_socket.get(), id, m1);

  EXPECT_TRUE(ok);
  EXPECT_EQ(id, kMessageIdOk);

  std::string test("1000 Test123 \n");
  EXPECT_STREQ(m1.c_str(), test.c_str());
}
