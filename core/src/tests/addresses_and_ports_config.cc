/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2025 Bareos GmbH & Co. KG

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

#include "testing_dir_common.h"

#include "dird/socket_server.h"
#include "lib/address_conf.h"
#include "lib/watchdog.h"
#include "lib/berrno.h"
#include <algorithm>

#include <iostream>
#include <algorithm>

#ifdef HAVE_WIN32
#  define socketClose(fd) ::closesocket(fd)
#  define socketOk(fd) ((fd) != INVALID_SOCKET)
#else
#  define socketClose(fd) ::close(fd)
#  define socketOk(fd) ((fd) >= 0)
#endif

struct raii_socket {
  using socket_type = decltype(socket(0, 0, 0));
#ifdef HAVE_WIN32
  static constexpr socket_type invalid_socket = INVALID_SOCKET;
#else
  static constexpr socket_type invalid_socket = -1;
#endif
  constexpr raii_socket() = default;
  // explicitly implicit
  raii_socket(socket_type s) : sock_fd{s} {}
  raii_socket(const raii_socket&) = delete;
  raii_socket& operator=(const raii_socket&) = delete;
  raii_socket(raii_socket&& other) noexcept { *this = std::move(other); }
  raii_socket& operator=(raii_socket&& other) noexcept
  {
    std::swap(sock_fd, other.sock_fd);
    return *this;
  }

  socket_type get() const noexcept { return sock_fd; }

  operator socket_type() const noexcept { return sock_fd; }

  constexpr bool ok() const noexcept { return socketOk(sock_fd); }

  ~raii_socket()
  {
    if (socketOk(sock_fd)) { socketClose(sock_fd); }
  }

 private:
  socket_type sock_fd = invalid_socket;
};

static bool test_bind_v4socket(int port)
{
  int family = AF_INET;

  raii_socket test_fd = socket(AF_INET, SOCK_STREAM, 0);
  EXPECT_TRUE(socketOk(test_fd));

  struct sockaddr_in v4_address;
  v4_address.sin_family = family;
  v4_address.sin_port = htons(port);
  v4_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(test_fd, (struct sockaddr*)&v4_address, sizeof(v4_address)) == 0) {
    struct sockaddr_in result_addr = {};
    socklen_t namelen = sizeof(result_addr);

    auto sock_res = getsockname(test_fd, (sockaddr*)&result_addr, &namelen);
    EXPECT_EQ(sock_res, 0);
    EXPECT_EQ(namelen, sizeof(result_addr));

    char* ip = inet_ntoa(result_addr.sin_addr);

    printf("%s\n", ip);

    return true;
  } else {
#ifdef HAVE_WIN32
    auto i = WSAGetLastError();
    std::cout << "WSAError: " << i << std::endl;
#endif
    return false;
  }
}

static bool test_bind_v6socket(int port)
{
  int family = AF_INET6;

  raii_socket test_fd = socket(family, SOCK_STREAM, 0);
  EXPECT_TRUE(socketOk(test_fd));

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
    Emsg1(M_WARNING, 0, T_("Cannot set IPV6_V6ONLY on socket: %s\n"),
          be.bstrerror());
    return false;
  }

  if (bind(test_fd, (struct sockaddr*)&v6_address, sizeof(v6_address)) >= 0) {
    return true;
  } else {
    return false;
  }
}

// Only to check port binding, not full addresses
static bool test_sockets(int family, int port)
{
  bool result = true;

  if (family == 0) {
    bool result_v4 = test_bind_v4socket(port);
    bool result_v6 = test_bind_v6socket(port);
    result = result_v4 && result_v6;
  } else {
    switch (family) {
      case AF_INET:
        result = test_bind_v4socket(port);
        break;
      case AF_INET6:
        result = test_bind_v6socket(port);
        break;
      default:
        EXPECT_TRUE(false);
        break;
    }
  }

  return result;
}

static bool try_binding_director_port(std::string path_to_config,
                                      int family,
                                      int port)
{
  bool result = false;

  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  if (!director_config) { return false; }

  bool start_socket_server_ok
      = directordaemon::StartSocketServer(directordaemon::me->DIRaddrs);
  EXPECT_TRUE(start_socket_server_ok)
      << "Could not start SocketServer; "
      << "Is a local director blocking the port ?";
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

class AddressesAndPortsConfigurationSetup : public ::testing::Test {
  void SetUp() override { InitDirGlobals(); }
};

TEST_F(AddressesAndPortsConfigurationSetup, default_config_values)
{
  std::string path_to_config
      = std::string("configs/addresses-and-ports/default-dir-values/");

  std::vector<std::string> expected_addresses{"host[ipv6;::;9101]",
                                              "host[ipv4;0.0.0.0;9101]"};
  check_addresses_list(path_to_config, expected_addresses);

  EXPECT_FALSE(try_binding_director_port(path_to_config, 0, 9101));
}

TEST_F(AddressesAndPortsConfigurationSetup, OLD_STYLE_dir_port_set)
{
  std::string path_to_config
      = std::string("configs/addresses-and-ports/old-style/dir-port-set/");

  std::vector<std::string> expected_addresses{"host[ipv4;0.0.0.0;29998]",
                                              "host[ipv6;::;29998]"};

  check_addresses_list(path_to_config, expected_addresses);

  EXPECT_FALSE(try_binding_director_port(path_to_config, 0, 29998));
}

TEST_F(AddressesAndPortsConfigurationSetup, OLD_STYLE_dir_v4address_set)
{
  std::string path_to_config
      = std::string("configs/addresses-and-ports/old-style/dir-v4address-set/");

  std::vector<std::string> expected_addresses{"host[ipv4;127.0.0.1;9101]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST_F(AddressesAndPortsConfigurationSetup, OLD_STYLE_dir_v6address_set)
{
  std::string path_to_config
      = std::string("configs/addresses-and-ports/old-style/dir-v6address-set/");

  std::vector<std::string> expected_addresses{"host[ipv6;::1;9101]"};

  check_addresses_list(path_to_config, expected_addresses);
}


/*The next two tests are the same in terms of functionnality, but there is a
 slight difference in the order of directive setup (Address and Port).
 This comes because as per the current state of config parsing, behavior is
 different when Port comes before Address in the config file, and vice
 versa.*/

TEST_F(AddressesAndPortsConfigurationSetup,
       OLD_STYLE_dir_v4port_and_address_set)
{
  std::string path_to_config = std::string(
      "configs/addresses-and-ports/old-style/dir-v4port-and-address-set/");

  std::vector<std::string> expected_addresses{"host[ipv4;0.0.0.0;29997]"};

  check_addresses_list(path_to_config, expected_addresses);

  EXPECT_FALSE(try_binding_director_port(path_to_config, AF_INET, 29997));
}

TEST_F(AddressesAndPortsConfigurationSetup,
       OLD_STYLE_dir_v4address_and_port_set)
{
  std::string path_to_config = std::string(
      "configs/addresses-and-ports/old-style/dir-v4address-and-port-set/");

  std::vector<std::string> expected_addresses{"host[ipv4;127.0.0.1;29996]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST_F(AddressesAndPortsConfigurationSetup, NEW_STYLE_dir_v6_address_set)
{
  std::string path_to_config
      = std::string("configs/addresses-and-ports/new-style/dir-v6-address/");

  std::vector<std::string> expected_addresses{"host[ipv6;::;29995]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST_F(AddressesAndPortsConfigurationSetup, NEW_STYLE_dir_v6_and_v4_address_set)
{
  std::string path_to_config = std::string(
      "configs/addresses-and-ports/new-style/dir-v6-and-v4-addresses/");

  std::vector<std::string> expected_addresses{"host[ipv6;::;29994]",
                                              "host[ipv4;127.0.0.1;29994]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST_F(AddressesAndPortsConfigurationSetup, NEW_STYLE_dir_ip_v4_address_set)
{
  std::string path_to_config
      = std::string("configs/addresses-and-ports/new-style/dir-ip-v4-address/");

  std::vector<std::string> expected_addresses{"host[ipv4;0.0.0.0;29992]"};

  check_addresses_list(path_to_config, expected_addresses);
}

TEST_F(AddressesAndPortsConfigurationSetup, NEW_STYLE_dir_ip_v6_address_set)
{
  std::string path_to_config
      = std::string("configs/addresses-and-ports/new-style/dir-ip-v6-address/");

  std::vector<std::string> expected_addresses{"host[ipv6;::;29993]"};

  check_addresses_list(path_to_config, expected_addresses);
}
