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


#include "lib/parse_conf.h"
#include "dird/socket_server.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"

#include "lib/address_conf.h"
#include "lib/watchdog.h"
#include "lib/berrno.h"


namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

static void InitGlobals()
{
  OSDependentInit();
#if HAVE_WIN32
  WSA_Init();
#endif
  directordaemon::my_config = nullptr;
  directordaemon::me = nullptr;
  InitMsg(NULL, NULL);
}

typedef std::unique_ptr<ConfigurationParser> PConfigParser;

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

static bool create_and_bind_v4socket(int test_fd, int port)
{
  int family = AF_INET;

  EXPECT_TRUE((test_fd = socket(AF_INET, SOCK_STREAM, 0)) >= 0);

  struct sockaddr_in v4_address;
  v4_address.sin_family = family;
  v4_address.sin_port = htons(port);
  v4_address.sin_addr.s_addr = INADDR_ANY;


  if (bind(test_fd, (struct sockaddr*)&v4_address, sizeof(v4_address)) == 0) {
    return true;
  } else {
    return false;
  }
}

static bool create_and_bind_v6socket(int test_fd, int port)
{
  int family = AF_INET6;

  EXPECT_TRUE((test_fd = socket(family, SOCK_STREAM, 0)) >= 0);

  struct sockaddr_in6 v6_address;
  v6_address.sin6_family = family;
  v6_address.sin6_port = htons(port);
  inet_pton(family, "::", &v6_address.sin6_addr);

  int ipv6only_option_value = 1;
  socklen_t option_len = sizeof(int);

  if (setsockopt(test_fd, IPPROTO_IPV6, IPV6_V6ONLY,
                 (sockopt_val_t)&ipv6only_option_value, option_len)
      < 0) {
    BErrNo be;
    Emsg1(M_WARNING, 0, _("Cannot set IPV6_V6ONLY on socket: %s\n"),
          be.bstrerror());

    return false;
  }


  if (bind(test_fd, (struct sockaddr*)&v6_address, sizeof(v6_address)) == 0) {
    return true;
  } else {
    return false;
  }
}

// Only to check port binding, not full addresses
static bool test_sockets(int family, int port)
{
  int v4_fd = -1;
  int v6_fd = -1;
  bool result = true;

  if (family == 0) {
    result = create_and_bind_v4socket(v4_fd, port)
             && create_and_bind_v6socket(v6_fd, port);

  } else {
    switch (family) {
      case AF_INET:
        result = create_and_bind_v4socket(v4_fd, port);
        break;
      case AF_INET6:
        result = create_and_bind_v6socket(v6_fd, port);
        break;
      default:
        EXPECT_TRUE(false);
        break;
    }
  }

  close(v4_fd);
  close(v6_fd);
  return result;
}

static bool try_binding_director_port(std::string path_to_config,
                                      int family,
                                      int port)
{
  debug_level = 10;  // set debug level high enough so we can see error messages
  InitGlobals();

  bool result = false;

  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  if (!director_config) { return false; }

  bool start_socket_server_ok
      = directordaemon::StartSocketServer(directordaemon::me->DIRaddrs);
  EXPECT_TRUE(start_socket_server_ok) << "Could not start SocketServer";
  if (!start_socket_server_ok) { return false; }

  result = test_sockets(family, port);

  directordaemon::StopSocketServer();
  StopWatchdog();

  return result;
}

static void check_addresses_list(std::string path_to_config,
                                 std::vector<std::string> expected_addresses)
{
  char buff[1024];
  IPADDR* addr;

  std::vector<std::string> director_addresses;

  debug_level = 10;  // set debug level high enough so we can see error messages
  InitGlobals();

  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  EXPECT_TRUE(director_config);

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


TEST(addresses_and_ports_setup, default_config_values)
{
  InitGlobals();
  std::string path_to_config
      = std::string(RELATIVE_PROJECT_SOURCE_DIR
                    "/configs/addresses-and-ports/default-dir-values/");

  std::vector<std::string> expected_addresses{"host[ipv6;::;9101]",
                                              "host[ipv4;0.0.0.0;9101]"};
  check_addresses_list(path_to_config, expected_addresses);

  EXPECT_FALSE(try_binding_director_port(path_to_config, 0, 9101));
}

TEST(addresses_and_ports_setup, OLD_STYLE_dir_port_set)
{
  InitGlobals();
  std::string path_to_config
      = std::string(RELATIVE_PROJECT_SOURCE_DIR
                    "/configs/addresses-and-ports/old-style/dir-port-set/");

  std::vector<std::string> expected_addresses{"host[ipv4;0.0.0.0;29998]",
                                              "host[ipv6;::;29998]"};

  check_addresses_list(path_to_config, expected_addresses);

  EXPECT_FALSE(try_binding_director_port(path_to_config, 0, 29998));
}

TEST(addresses_and_ports_setup, OLD_STYLE_dir_v4address_set)
{
  InitGlobals();
  std::string path_to_config = std::string(
      RELATIVE_PROJECT_SOURCE_DIR
      "/configs/addresses-and-ports/old-style/dir-v4address-set/");

  std::vector<std::string> expected_addresses{"host[ipv4;127.0.0.1;9101]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST(addresses_and_ports_setup, OLD_STYLE_dir_v6address_set)
{
  InitGlobals();
  std::string path_to_config = std::string(
      RELATIVE_PROJECT_SOURCE_DIR
      "/configs/addresses-and-ports/old-style/dir-v6address-set/");

  std::vector<std::string> expected_addresses{"host[ipv6;::1;9101]"};

  check_addresses_list(path_to_config, expected_addresses);
}


/*The next two tests are the same in terms of functionnality, but there is a
 slight difference in the order of directive setup (DirAddress and DirPort).
 This comes because as per the current state of config parsing, behavior is
 different when DirPort comes before DirAddress in the config file, and vice
 versa.*/

TEST(addresses_and_ports_setup, OLD_STYLE_dir_v4port_and_address_set)
{
  InitGlobals();
  std::string path_to_config = std::string(
      RELATIVE_PROJECT_SOURCE_DIR
      "/configs/addresses-and-ports/old-style/dir-v4port-and-address-set/");

  std::vector<std::string> expected_addresses{"host[ipv4;127.0.0.1;29997]"};

  check_addresses_list(path_to_config, expected_addresses);

  EXPECT_FALSE(try_binding_director_port(path_to_config, AF_INET, 29997));
}

TEST(addresses_and_ports_setup, OLD_STYLE_dir_v4address_and_port_set)
{
  InitGlobals();
  std::string path_to_config = std::string(
      RELATIVE_PROJECT_SOURCE_DIR
      "/configs/addresses-and-ports/old-style/dir-v4address-and-port-set/");

  std::vector<std::string> expected_addresses{"host[ipv4;127.0.0.1;29996]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST(addresses_and_ports_setup, NEW_STYLE_dir_v6_address_set)
{
  InitGlobals();
  std::string path_to_config
      = std::string(RELATIVE_PROJECT_SOURCE_DIR
                    "/configs/addresses-and-ports/new-style/dir-v6-address/");

  std::vector<std::string> expected_addresses{"host[ipv6;::;29995]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST(addresses_and_ports_setup, NEW_STYLE_dir_v6_and_v4_address_set)
{
  InitGlobals();
  std::string path_to_config = std::string(
      RELATIVE_PROJECT_SOURCE_DIR
      "/configs/addresses-and-ports/new-style/dir-v6-and-v4-addresses/");

  std::vector<std::string> expected_addresses{"host[ipv6;::;29994]",
                                              "host[ipv4;127.0.0.1;29994]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST(addresses_and_ports_setup, NEW_STYLE_dir_ip_v4_address_set)
{
  InitGlobals();
  std::string path_to_config = std::string(
      RELATIVE_PROJECT_SOURCE_DIR
      "/configs/addresses-and-ports/new-style/dir-ip-v4-address/");

  std::vector<std::string> expected_addresses{"host[ipv4;0.0.0.0;29992]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST(addresses_and_ports_setup, NEW_STYLE_dir_ip_v6_address_set)
{
  InitGlobals();
  std::string path_to_config = std::string(
      RELATIVE_PROJECT_SOURCE_DIR
      "/configs/addresses-and-ports/new-style/dir-ip-v6-address/");

  std::vector<std::string> expected_addresses{"host[ipv6;::;29993]"};

  check_addresses_list(path_to_config, expected_addresses);
}
