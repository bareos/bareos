/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2020 Bareos GmbH & Co. KG

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
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#endif

#include <algorithm>

#include "console/console_conf.h"
#include "console/console_globals.h"
#include "console/connect_to_director.h"
#include "dird/socket_server.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"

#include "lib/tls_openssl.h"
#include "lib/bsock.h"
#include "lib/bnet.h"
#include "lib/bstringlist.h"
#include "lib/watchdog.h"
#include "tests/init_openssl.h"

#include "include/jcr.h"
#include <signal.h>

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

static void InitSignalHandler()
{
  struct sigaction sig = {};
  sig.sa_handler = SIG_IGN;
  sigaction(SIGUSR2, &sig, nullptr);
  sigaction(SIGPIPE, &sig, nullptr);
}

static void InitGlobals()
{
  OSDependentInit();
#if HAVE_WIN32
  WSA_Init();
#endif
  directordaemon::my_config = nullptr;
  directordaemon::me = nullptr;
  console::my_config = nullptr;
  console::director_resource = nullptr;
  console::console_resource = nullptr;
  console::me = nullptr;
}

typedef std::unique_ptr<ConfigurationParser> PConfigParser;

static PConfigParser ConsolePrepareResources(const std::string& path_to_config)
{
  const char* console_configfile = path_to_config.c_str();
  PConfigParser console_config(
      console::InitConsConfig(console_configfile, M_INFO));
  console::my_config
      = console_config.get(); /* set the console global variable */

  EXPECT_NE(console_config.get(), nullptr);
  if (!console_config) { return nullptr; }

  bool parse_console_config_ok = console_config->ParseConfig();
  EXPECT_TRUE(parse_console_config_ok) << "Could not parse console config";
  if (!parse_console_config_ok) { return nullptr; }

  console::director_resource = dynamic_cast<console::DirectorResource*>(
      console_config->GetNextRes(console::R_DIRECTOR, NULL));
  EXPECT_NE(console::director_resource, nullptr);

  console::console_resource = dynamic_cast<console::ConsoleResource*>(
      console_config->GetNextRes(console::R_CONSOLE, NULL));
  console::my_config->own_resource_ = console::console_resource;
  EXPECT_EQ(console::console_resource, nullptr);  // no console resource present

  return console_config;
}

static PConfigParser DirectorPrepareResources(const std::string& path_to_config)
{
  PConfigParser director_config(
      directordaemon::InitDirConfig(path_to_config.c_str(), M_INFO));
  directordaemon::my_config
      = director_config.get(); /* set the director global variable */

  EXPECT_NE(director_config.get(), nullptr);
  if (!director_config) { return nullptr; }

  bool parse_director_config_ok = director_config->ParseConfig();
  EXPECT_TRUE(parse_director_config_ok) << "Could not parse director config";
  if (!parse_director_config_ok) { return nullptr; }

  Dmsg0(200, "Start UA server\n");
  directordaemon::me
      = (directordaemon::DirectorResource*)director_config->GetNextRes(
          directordaemon::R_DIRECTOR, nullptr);
  directordaemon::my_config->own_resource_ = directordaemon::me;

  return director_config;
}

typedef std::unique_ptr<BareosSocket, std::function<void(BareosSocket*)>>
    PBareosSocket;

static PBareosSocket ConnectToDirector(JobControlRecord& jcr)
{
  BStringList args;
  uint32_t response_id = kMessageIdUnknown;

  PBareosSocket UA_sock(console::ConnectToDirector(jcr, 0, args, response_id),
                        [](BareosSocket* p) { delete p; });

  EXPECT_EQ(response_id, kMessageIdOk);

  EXPECT_NE(UA_sock.get(), nullptr);
  if (!UA_sock) { return nullptr; }
  return UA_sock;
}

static void CheckEncryption(const BareosSocket* UA_sock, TlsPolicy tls_policy)
{
  if (tls_policy == TlsPolicy::kBnetTlsNone) {
    EXPECT_FALSE(UA_sock->tls_conn.get());
    std::cout << " Cleartext" << std::endl;
  } else {
    EXPECT_TRUE(UA_sock->tls_conn.get());
    std::string cipher;
    UA_sock->GetCipherMessageString(cipher);
    cipher += '\n';
    std::cout << cipher;
  }
}

static bool do_connection_test(std::string path_to_config, TlsPolicy tls_policy)
{
  debug_level = 10;  // set debug level high enough so we can see error messages
  InitSignalHandler();
  InitGlobals();

  PConfigParser console_config(ConsolePrepareResources(path_to_config));
  if (!console_config) { return false; }

  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  if (!director_config) { return false; }

  bool start_socket_server_ok
      = directordaemon::StartSocketServer(directordaemon::me->DIRaddrs);
  EXPECT_TRUE(start_socket_server_ok) << "Could not start SocketServer";
  if (!start_socket_server_ok) { return false; }

  JobControlRecord jcr;

  PBareosSocket UA_sock(ConnectToDirector(jcr));
  if (!UA_sock) { return false; }

  CheckEncryption(UA_sock.get(), tls_policy);

  UA_sock->signal(BNET_TERMINATE);
  UA_sock->close();
  jcr.dir_bsock = nullptr;

  directordaemon::StopSocketServer();
  StopWatchdog();

  return true;
}

static void test_socket(int family, int port)
{
  int test_fd;
  int option = 1;

  EXPECT_TRUE((test_fd = socket(family, SOCK_STREAM, 0)) >= 0);

  switch (family) {
    case AF_INET:;
      struct sockaddr_in v4_address;
      v4_address.sin_family = family;
      v4_address.sin_port = htons(port);
      v4_address.sin_addr.s_addr = INADDR_ANY;


      EXPECT_FALSE(
          bind(test_fd, (struct sockaddr*)&v4_address, sizeof(v4_address))
          == 0);

      break;

#ifdef HAVE_IPV6
    case AF_INET6:
      struct sockaddr_in6 v6_address;
      v6_address.sin6_family = family;
      v6_address.sin6_port = htons(port);
      inet_pton(AF_INET6, "::", &v6_address.sin6_addr);

      EXPECT_TRUE(setsockopt(test_fd, IPPROTO_IPV6, IPV6_V6ONLY,
                             (sockopt_val_t)&option, sizeof(option))
                  == 0);

      EXPECT_FALSE(
          bind(test_fd, (struct sockaddr*)&v6_address, sizeof(v6_address))
          == 0);

      break;
#endif

    default:
      EXPECT_TRUE(false);
      break;
  }

  close(test_fd);
}

static bool do_binding_test(std::string path_to_config, int family, int port)
{
  debug_level = 10;  // set debug level high enough so we can see error messages
  InitSignalHandler();
  InitGlobals();

  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  if (!director_config) { return false; }

  bool start_socket_server_ok
      = directordaemon::StartSocketServer(directordaemon::me->DIRaddrs);
  EXPECT_TRUE(start_socket_server_ok) << "Could not start SocketServer";
  if (!start_socket_server_ok) { return false; }

  test_socket(family, port);

  directordaemon::StopSocketServer();
  StopWatchdog();

  return true;
}

static void check_addresses_list(std::string path_to_config,
                                 std::vector<std::string> expected_addresses)
{
  char buff[1024];
  IPADDR* addr;

  std::vector<std::string> director_addresses;

  debug_level = 10;  // set debug level high enough so we can see error messages
  InitSignalHandler();
  InitGlobals();

  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  EXPECT_TRUE(director_config);

  EXPECT_TRUE(directordaemon::me->DIRaddrs->size()
              == static_cast<int>(expected_addresses.size()));

  foreach_dlist (addr, directordaemon::me->DIRaddrs) {
    addr->build_address_str(buff, sizeof(buff), true);

    std::string theaddress = std::string(buff);
    theaddress.pop_back();

    director_addresses.push_back(theaddress);
  }

  std::sort(director_addresses.begin(), director_addresses.end());
  std::sort(expected_addresses.begin(), expected_addresses.end());
  EXPECT_EQ(director_addresses, expected_addresses);
}

TEST(port_and_addresses_setup, dir_v4address_set)
{
  std::string path_to_config = std::string(
      RELATIVE_PROJECT_SOURCE_DIR "/configs/dual_stacking/dir_v4address_set/");

  std::vector<std::string> expected_addresses{"host[ipv4;127.0.0.1;29998]"};

  check_addresses_list(path_to_config, expected_addresses);

  do_binding_test(std::string(RELATIVE_PROJECT_SOURCE_DIR
                              "/configs/dual_stacking/dir_v4address_set/"),
                  AF_INET, 29998);
}

TEST(port_and_addresses_setup, dir_v4address_and_port_set)
{
  std::string path_to_config
      = std::string(RELATIVE_PROJECT_SOURCE_DIR
                    "/configs/dual_stacking/dir_v4address_and_port_set/");

  std::vector<std::string> expected_addresses{"host[ipv4;127.0.0.1;30012]"};

  check_addresses_list(path_to_config, expected_addresses);

  do_binding_test(
      std::string(RELATIVE_PROJECT_SOURCE_DIR
                  "/configs/dual_stacking/dir_v4address_and_port_set/"),
      AF_INET, 30012);
}


TEST(bsock, console_director_connection_test_tls_psk)
{
  InitOpenSsl();
  do_connection_test(
      std::string(RELATIVE_PROJECT_SOURCE_DIR
                  "/configs/console-director/tls_psk_default_enabled/"),
      TlsPolicy::kBnetTlsEnabled);
}

TEST(bsock, console_director_connection_test_cleartext)
{
  InitOpenSsl();
  do_connection_test(std::string(RELATIVE_PROJECT_SOURCE_DIR
                                 "/configs/console-director/tls_disabled/"),
                     TlsPolicy::kBnetTlsNone);
}
