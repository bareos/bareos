/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2021 Bareos GmbH & Co. KG

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
#if !defined(HAVE_WIN32)
  struct sigaction sig = {};
  sig.sa_handler = SIG_IGN;
  sigaction(SIGUSR2, &sig, nullptr);
  sigaction(SIGPIPE, &sig, nullptr);
#endif
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
  InitMsg(NULL, NULL);
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
