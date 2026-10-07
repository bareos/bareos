/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#include <jansson.h>

#include "include/jcr.h"
#include "lib/bnet.h"
#include "lib/cram_md5.h"
#include "lib/global_resource.h"
#include "lib/tls_psk_credentials.h"
#include "lib/util.h"
#include "lib/base64.h"
#include "include/version_hex.h"
#include "lib/bauth/cram_md5.h"
#include "lib/bauth.h"
#include "lib/connect_accept.h"

#include <thread>
#include <optional>

static constexpr int debuglevel = 100;

namespace {
struct auth_timer {
  auth_timer(BareosSocket* socket)
      : timer{StartBsockTimer(socket, AUTH_TIMEOUT)}
  {
  }

  auth_timer(auth_timer&) = delete;
  auth_timer(const auth_timer&) = delete;

  ~auth_timer()
  {
    if (timer) { StopBsockTimer(timer); }
  }

  btimer_t* timer{nullptr};
};

bool DoTlsHandshakeWithClient(JobControlRecord* jcr,
                              BareosSocket* socket,
                              std::shared_ptr<Tls> tls)
{
  std::vector<std::string> verify_list;

  if (BnetTlsServer(socket, std::move(tls))) { return true; }
  if (jcr && jcr->JobId != 0) {
    Jmsg(jcr, M_FATAL, 0, T_("TLS negotiation failed.\n"));
  }
  Dmsg0(debuglevel, "TLS negotiation failed.\n");
  return false;
}

bool DoTlsHandshakeWithServer(JobControlRecord* jcr,
                              BareosSocket* socket,
                              std::shared_ptr<Tls> tls)
{
  if (BnetTlsClient(jcr, socket, std::move(tls))) { return true; }

  int message_type = 0;
  std::string message;

  if (jcr && jcr->is_passive_client_connection_probing) {
    /* connection try */
    message_type = M_INFO;
    message = T_("TLS negotiation failed (while probing client protocol)");
  } else {
    message_type = M_FATAL;
    message = T_("TLS negotiation failed");
  }

  if (jcr && jcr->JobId != 0) {
    Jmsg(jcr, message_type, 0, "%s\n", message.c_str());
  }
  Dmsg0(debuglevel, "%s\n", message.c_str());

  return false;
}

std::shared_ptr<Tls> ParameterizeAndInitTlsConnectionAsAServer(
    const TlsResource* tls_resource,
    TlsConfigProvider* data)
{
  ASSERT(tls_resource);
  auto result = Tls::CreateServerContext(Tls::ImplementationType::kOpenSsl,
                                         tls_resource, data);
  if (!result) {
    Emsg0(M_ERROR, 0, T_("TLS connection initialization failed.\n"));
    return nullptr;
  }

  return result;
}

std::shared_ptr<Tls> ParameterizeAndInitTlsConnectionAsAClient(
    JobControlRecord* jcr,
    const TlsResource* tls_resource,
    const char* identity,
    const char* password)
{
  ASSERT(tls_resource);
  ASSERT(tls_resource->IsTlsConfigured());

  PskCredentials psk_cred{}, *ptr{};
  if (identity) {
    psk_cred = PskCredentials{identity, password};
    ptr = &psk_cred;
  } else {
    Dmsg2(200, "Psk is not setup, as not identity was provided\n");
  }

  auto result = Tls::CreateClientContext(Tls::ImplementationType::kOpenSsl,
                                         tls_resource, ptr);
  if (!result) {
    Qmsg0(jcr, M_FATAL, 0, T_("TLS connection initialization failed.\n"));
    return nullptr;
  }

  return result;
}

bool guess_whether_cleartext(BareosSocket* socket, bool* is_cleartext)
{
  /* we check that we are about to receive a bnet message starting
   * with 'Hello ' as this means that this is actually an unencrypted
   * client-hello message.
   * Every bnet message starts with its 32bit length in big endian order, so
   * we need to check 10 bytes.
   */
  constexpr std::string_view hello_start = "Hello ";
  char peek_buffer[hello_start.size() + sizeof(uint32_t)];

  auto now = std::chrono::steady_clock::now;

  static constexpr auto max_wait_time = std::chrono::seconds(5);

  auto start = now();
  while (now() - start < max_wait_time) {
    if (socket->IsTimedOut()) { return false; }

    auto bytes_received = socket->peek(peek_buffer, sizeof(peek_buffer));

    if (bytes_received <= 0) {
      // either an error occured (bytes_received < 0)
      // or the connection was cut (bytes_received == 0)
      return false;
    }

    size_t bytes_in_buffer = bytes_received;

    uint32_t msg_size_network;
    if (bytes_in_buffer > sizeof(msg_size_network)) {
      memcpy(&msg_size_network, peek_buffer, sizeof(msg_size_network));

      uint32_t msg_size = ntohl(msg_size_network);

      if (msg_size < 10 || msg_size > 1000) {
        // this is definitely not a cleartext hello
        Dmsg0(150,
              "peek starts with bad header (%" PRIu32
              ") -> not cleartext hello\n",
              msg_size);
        *is_cleartext = false;
        return true;
      }
    } else {
      // give data some more time to arrive
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    size_t message_bytes = bytes_in_buffer - sizeof(msg_size_network);

    ASSERT(message_bytes <= hello_start.size());

    if (memcmp(hello_start.data(), peek_buffer + sizeof(msg_size_network),
               message_bytes)
        != 0) {
      Dmsg0(
          150,
          "message contains bad characters at start -> not cleartext hello\n");
      *is_cleartext = false;
      return true;
    }

    if (bytes_in_buffer == sizeof(peek_buffer)) {
      // we are happy with everything, so this looks like a cleartext hello

      *is_cleartext = true;
      return true;
    }

    // give data some more time to arrive
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return false;
}

struct CramIdentity {
  CramIdentity()
  {
    identity.resize(120);

    if (!MakeSessionKey(identity.data())) {
      Emsg1(M_ERROR_TERM, 0, "Could not generate default CRAM identity: %s\n",
            identity.c_str());
    }

    identity.resize(strlen(identity.c_str()));
  }

  const std::string& as_str() const { return identity; }

  std::string identity;
};

static const std::string& get_default_cram_identity()
{
  static CramIdentity identity;

  return identity.as_str();
}

bool cram_md5_handshake(JobControlRecord* jcr,
                        BareosSocket* socket,
                        const std::string& my_qualified_name,
                        const char* password,
                        TlsPolicy my_policy,
                        bool initiated_by_remote,
                        TlsPolicy* remote_policy)
{
  if (jcr && jcr->IsJobCanceled()) {
    const char* err_msg
        = T_("TwoWayAuthenticate failed, because job was canceled.");
    Jmsg(jcr, M_FATAL, 0, "%s\n", err_msg);
    Dmsg0(debuglevel, "%s\n", err_msg);

    return false;
  }

  CramMd5Handshake cram_md5_handshake(socket, password, my_policy,
                                      my_qualified_name);

  if (socket->ConnectionReceivedTerminateSignal()) {
    const char* err_msg = T_(
        "TwoWayAuthenticate failed, because connection was reset by "
        "destination peer.");
    Jmsg(jcr, M_FATAL, 0, "%s\n", err_msg);
    Dmsg0(debuglevel, "%s\n", err_msg);
    return false;
  }

  bool auth_success = cram_md5_handshake.DoHandshake(initiated_by_remote);

  if (!auth_success) {
    char ipaddr_str[MAXHOSTNAMELEN]{};
    SockaddrToAscii(&socket->client_addr, ipaddr_str, sizeof(ipaddr_str));

    switch (cram_md5_handshake.result) {
      case CramMd5Handshake::HandshakeResult::REPLAY_ATTACK: {
        const char* fmt
            = "Warning! Attack detected: "
              "I will not answer to my own challenge. "
              "Please check integrity of the host at IP address: %s\n";
        Jmsg(jcr, M_FATAL, 0, fmt, ipaddr_str);
        Dmsg1(debuglevel, fmt, ipaddr_str);
        break;
      }
      case CramMd5Handshake::HandshakeResult::NETWORK_ERROR:
        Jmsg(jcr, M_FATAL, 0, T_("Network error during CRAM MD5 with %s\n"),
             ipaddr_str);
        break;
      case CramMd5Handshake::HandshakeResult::WRONG_HASH:
        Jmsg(jcr, M_FATAL, 0, T_("Authorization key rejected by %s.\n"),
             ipaddr_str);
        break;
      case CramMd5Handshake::HandshakeResult::FORMAT_MISMATCH:
        Jmsg(jcr, M_FATAL, 0,
             T_("Wrong format of the CRAM challenge with %s.\n"), ipaddr_str);
        break;
      default:
        break;
    }
    socket->fsend(T_("1999 Authorization failed.\n"));
    Bmicrosleep(socket->sleep_time_after_authentication_error, 0);
  } else if (jcr && jcr->IsJobCanceled()) {
    const char* err_msg
        = T_("TwoWayAuthenticate failed, because job was canceled.");
    Jmsg(jcr, M_FATAL, 0, "%s\n", err_msg);
    Dmsg0(debuglevel, "%s\n", err_msg);
    auth_success = false;
  }

  if (auth_success) { *remote_policy = cram_md5_handshake.RemoteTlsPolicy(); }
  return auth_success;
}
}  // namespace


bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   const std::string& qualified_name,
                   const TlsResource* res,
                   auth::OutboundAuthenticator* auth,
                   std::string_view hello_msg,
                   bool cleartext_authentication)
{
  ASSERT(jcr);
  ASSERT(socket);
  ASSERT(res);

  auth_timer timer{socket};

  if (res->IsTlsConfigured() && !cleartext_authentication) {
    auto tls = ParameterizeAndInitTlsConnectionAsAClient(
        jcr, res, qualified_name.c_str(), res->password_.value);

    if (!tls) {
      Jmsg(jcr, M_FATAL, 0, "Could initialize initial tls context for %s\n",
           socket->who());
      return false;
    }

    if (!DoTlsHandshakeWithServer(jcr, socket, tls)) {
      Jmsg(jcr, M_FATAL, 0, "Could not complete tls handshake\n");
      return false;
    }

    tls->TlsLogConninfo(jcr, socket->host(), socket->port(), socket->who());

    if (res->authenticate_) {
      // cleanup tls
      Qmsg(jcr, M_INFO, 0,
           "Proceeding with UNENCRYPTED authentication with %s as 'Tls "
           "Authenticate = Yes' was set\n",
           socket->who());
      socket->CloseTlsConnectionAndFreeMemory();
    }
  } else {
    Qmsg(jcr, M_INFO, 0, T_("Connected %s at %s:%d, encryption: None\n"),
         socket->who(), socket->host(), socket->port());
  }

  if (!socket->send(hello_msg.data(), hello_msg.size())) {
    Jmsg(jcr, M_FATAL, 0, "Could not send hello\n");
    return false;
  }

  if (!auth->authenticate({.jcr = jcr, .socket = socket, .target = res})) {
    Emsg1(M_ERROR, 0, T_("Bad authentication from %s.\n"), socket->who());
    return false;
  }

  jcr->authenticated = true;
  return true;
}

struct TlsWrapper : public TlsConfigProvider {
  TlsWrapper(ConnectionInfoProvider* provider) : wrapped{provider} {}

  const TlsResource* get_tls_config_for(global_resource::Type type,
                                        std::string_view name) override
  {
    info = wrapped->get_info_for(type, name);
    if (!info) { return nullptr; }
    psk_type = type;
    psk_name.assign(name);
    return info->tls_settings();
  }

  struct result {
    ConnectionInfo* info{};
    std::optional<std::string> bad_name{};
    std::optional<global_resource::Type> bad_type{};
  };

  result get_or_fetch(global_resource::Type type, std::string_view name)
  {
    result res{};
    if (!info) {
      info = wrapped->get_info_for(type, name);
      psk_type = type;
      psk_name.assign(name);
      res.info = info.get();
    } else if (psk_type != type) {
      res.bad_type = psk_type;
    } else if (name != psk_name) {
      res.bad_name = psk_name;
    } else {
      res.info = info.get();
    }
    return res;
  }

 private:
  std::unique_ptr<ConnectionInfo> info;
  global_resource::Type psk_type{};
  std::string psk_name{};

  ConnectionInfoProvider* wrapped;
};


std::optional<ParsedHello> BareosAccept(BareosSocket* socket,
                                        global_resource::Type my_type,
                                        const TlsResource* initial_tls,
                                        ConnectionInfoProvider* provider)
{
  if (!socket) {
    Emsg1(M_ERROR, 0, "socket is NULL in BareosAccept.\n");
    return std::nullopt;
  }

  if (!initial_tls) {
    Emsg1(M_ERROR, 0, "initial_tls is NULL in BareosAccept.\n");
    return std::nullopt;
  }

  if (!provider) {
    Emsg1(M_ERROR, 0, "provider is NULL in BareosAccept.\n");
    return std::nullopt;
  }

  TlsWrapper wrapper{provider};

  auth_timer timer{socket};

  bool received_clear_text_handshake = false;
  if (!guess_whether_cleartext(socket, &received_clear_text_handshake)) {
    Emsg1(M_ERROR, 0, "Could not check for cleartext handshake with %s\n",
          socket->who());
    return std::nullopt;
  }

  if (!received_clear_text_handshake) {
    auto tls = ParameterizeAndInitTlsConnectionAsAServer(initial_tls, &wrapper);
    if (!tls) {
      Emsg1(M_ERROR, 0, "Could not initialize initial tls context for %s\n",
            socket->who());
      return std::nullopt;
    }
    if (!DoTlsHandshakeWithClient(nullptr, socket, std::move(tls))) {
      Emsg1(M_ERROR, 0, "Could not complete tls handshake with %s\n",
            socket->who());
      return std::nullopt;
    }

    if (initial_tls->authenticate_) {
      // cleanup tls
      socket->CloseTlsConnectionAndFreeMemory();
    }
  }

  if (socket->recv() <= 0 || socket->message_length < 0) {
    Emsg1(M_ERROR, 0, T_("Connection request from %s failed.\n"),
          socket->who());
    return std::nullopt;
  }

  std::string_view hello{socket->msg,
                         static_cast<size_t>(socket->message_length)};

  auto parsed_hello = parse_hello(my_type, hello);
  if (!parsed_hello) {
    Emsg1(M_ERROR, 0,
          T_("Connection request from %s failed: could not parse the hello\n"),
          socket->who());
    return std::nullopt;
  }

  auto found = wrapper.get_or_fetch(parsed_hello->type, parsed_hello->name);

  if (!found.info) {
    if (found.bad_name) {
      Emsg1(M_ERROR, 0,
            "tls/cram name mismatch detected for %s (tls: %s vs hello: %s)!\n",
            socket->who(), found.bad_name->c_str(), parsed_hello->name.c_str());
    } else if (found.bad_type) {
      Emsg1(M_ERROR, 0,
            "tls/cram type mismatch detected for %s! (tls: %s vs hello: %s)\n",
            socket->who(),
            std::string{global_resource::GetNameFromType(*found.bad_type)}
                .c_str(),
            std::string{global_resource::GetNameFromType(parsed_hello->type)}
                .c_str());
    } else {
      Emsg1(M_ERROR, 0, "Could not map %s/%s to connection info for %s.\n",
            std::string{global_resource::GetNameFromType(parsed_hello->type)}
                .c_str(),
            parsed_hello->name.c_str(), socket->who());
    }
    return std::nullopt;
  }

  auto* info = found.info;

  bool tls_established = false;
  bool tls_psk_used = false;

  if (socket->tls_conn) {
    tls_established = true;
    switch (socket->tls_conn->IsPskIdentityInUse(global_resource::QualifiedName(
        parsed_hello->type, parsed_hello->name))) {
      case Tls::PskIdentityStatus::IsInUse: {
        tls_psk_used = true;
      } break;
      case Tls::PskIdentityStatus::NoIdentityInUse: {
        tls_psk_used = false;
      } break;
      case Tls::PskIdentityStatus::DifferentIdentityInUse:
      default: {
        /* we should never get here, as we just checked that the name is the
         * last name fetched in the psk-tls identity callback */
        Emsg1(M_ERROR, 0, "Internal error occured for psk with %s\n",
              socket->who());
        return std::nullopt;
      } break;
    }
  }

  const TlsResource* tls_resource = info->tls_settings();

  if (!tls_resource) {
    Emsg1(M_ERROR, 0, T_("Could not map identity to tls resource for %s.\n"),
          socket->who());
    return std::nullopt;
  }

  if (received_clear_text_handshake) {
    if (parsed_hello->type == global_resource::Type::Client
        && !tls_resource->tls_require_) {
      Dmsg0(200, "Accepting cleartext handshake for client\n");
    } else if (tls_resource->tls_enable_) {
      Emsg1(M_ERROR, 0, T_("Received a cleartext hello from %s.\n"),
            socket->who());
      return std::nullopt;
    }
  }

  ConnectionType connection_type = ConnectionType::Insecure;
  if (tls_established) { connection_type = ConnectionType::Untrusted; }
  if (tls_psk_used) { connection_type = ConnectionType::Trusted; }

  auto ps = info->select_provers(connection_type);
  auto vs = info->select_verifiers(connection_type);

  auth::DefaultAuthenticator auth{};
  if (!auth.authenticate_inbound({
          .socket = socket,
          .remote_version = parsed_hello->bareos_version,
          .target = tls_resource,
          .provers = ps,
          .verifiers = vs,
      })) {
    Emsg1(M_ERROR, 0, T_("Bad authentication from %s.\n"), socket->who());
    return std::nullopt;
  }

  return parsed_hello;
}

namespace auth {
static constexpr int debuglevel = 100;

struct challenge {
  std::string name;
  std::string challenge;
};

bool ParseChallenges(std::vector<challenge>& challenges,
                     std::string_view challenges_str)
{
  json_error_t err = {};
  json_t* challenges_json
      = json_loadb(challenges_str.data(), challenges_str.size(), 0, &err);


  if (!challenges_json) {
    Dmsg0(100, "Could not parse challenges from: %.*s\n",
          static_cast<int>(challenges_str.size()), challenges_str.data());
    return false;
  }
  if (!json_is_array(challenges_json)) {
    Dmsg0(100, "Could not parse challenge array from: %.*s\n",
          static_cast<int>(challenges_str.size()), challenges_str.data());
    json_decref(challenges_json);
    return false;
  }

  json_int_t index;
  json_t* entry;
  json_array_foreach(challenges_json, index, entry)
  {
    const char* name;
    const char* challenge;
    if (json_unpack_ex(entry, &err, 0, "{s:s, s:s}", "name", &name, "challenge",
                       &challenge)
        < 0) {
      Dmsg0(100, "Could not parse %lldth challenge entry from: %.*s\n", index,
            static_cast<int>(challenges_str.size()), challenges_str.data());
      json_decref(challenges_json);
      return false;
    }

    challenges.emplace_back(name, challenge);
  }

  json_decref(challenges_json);
  return true;
}


bool Challenge(BareosSocket* socket,
               std::span<std::unique_ptr<Verifier>> verifiers)
{
  auto* possibilities = json_array();
  if (!possibilities) { return false; }
  for (auto& verifier : verifiers) {
    auto* challenge = json_object();
    if (!challenge) {
      json_decref(possibilities);
      return false;
    }

    auto name = verifier->name();
    auto chal = verifier->generate_challenge();

    std::string chal64;
    chal64.resize((chal.size() * 4 + 2) / 3);
    int length = BinToBase64(chal64.data(), chal64.size() + 1, chal.data(),
                             chal.size(), true);

    if (length < 0) {
      Dmsg0(100, "Could not transform challenge into b64\n");
      json_decref(challenge);
      json_decref(possibilities);
      return false;
    }

    chal64.resize(length);

    if (json_object_set_new(challenge, "name",
                            json_stringn(name.data(), name.size()))
            < 0
        || json_object_set_new(challenge, "challenge",
                               json_stringn(chal64.data(), chal64.size()))
               < 0) {
      json_decref(challenge);
      json_decref(possibilities);
      return false;
    }
    json_array_append_new(possibilities, challenge);
  }

  auto* json_str = json_dumps(possibilities, 0);
  json_decref(possibilities);
  if (!json_str) { return false; }
  auto json_str_size = strlen(json_str);
  std::string json_b64((json_str_size * 4 + 2) / 3, '\0');
  auto length = BinToBase64(json_b64.data(), json_b64.size() + 1, json_str,
                            json_str_size, true);

  free(json_str);
  if (length <= 0) { return false; }

  ASSERT(static_cast<std::size_t>(length) <= json_b64.size());
  json_b64.resize(length);
  if (!socket->fsend("auth: %s\n", json_b64.c_str())) { return false; }

  if (socket->recv() < 0) { return false; }

  std::string_view received{socket->msg,
                            static_cast<std::size_t>(socket->message_length)};

  auto name_end = received.find(':');
  if (name_end == received.npos) {
    Dmsg0(100, "received bad response (no COLON)\n");
    return false;
  }

  // if (received.size() == name_end + 1
  //    || received[name_end] != ' ') {
  //   Dmsg0(100, "received short response (no SPC after COLON)\n");
  //   return false;
  // }

  // if (received.back() != '\n') {
  //   Dmsg0(100, "received short response (no NL at end)\n");
  //   return false;
  // }

  auto received_name = received.substr(0, name_end);
  Dmsg0(100, "Searching for verifier %.*s\n",
        static_cast<int>(received_name.size()), received_name.data());

  auto* verifier = [&]() -> Verifier* {
    for (auto& candidate : verifiers) {
      auto name = candidate->name();
      if (received_name == name) { return candidate.get(); }
    }

    return nullptr;
  }();

  if (!verifier) {
    Dmsg0(100, "Requested Verifier\n");
    return false;
  }

  std::string response{received};
  std::string expected_prefix{verifier->name()};
  std::vector<char> data_out;
  std::string data_out_b64;
  std::vector<char> data_in;
  expected_prefix += ": ";
  for (;;) {
    if (!response.starts_with(expected_prefix)) {
      Dmsg0(100, "Response did not start with expected prefix: %.*s\n",
            static_cast<int>(expected_prefix.size()), response.c_str());
      return false;
    }

    if (response.back() != '\n') {
      Dmsg0(100, "Received short response (missing NL at end)\n");
      return false;
    }

    auto data_b64_len = response.size() - 1 - expected_prefix.size();
    auto data_b64 = std::string_view{response}.substr(expected_prefix.size(),
                                                      data_b64_len);

    data_in.resize(data_b64.size());
    if (!data_b64.empty()) {
      int data_in_length = Base64ToBin(data_in.data(), data_in.size(),
                                       data_b64.data(), data_b64.size());

      if (data_in_length < 0) {
        Dmsg0(100, "could not b64decode received data\n");
        return false;
      }

      data_in.resize(data_in_length);
    }
    data_out.clear();
    auto status = verifier->step(data_out, data_in);

    switch (status) {
      case InProgress: {
        data_out_b64.resize((data_out.size() * 4 + 2) / 3);
        int data_out_b64_length
            = BinToBase64(data_out_b64.data(), data_out_b64.size() + 1,
                          data_out.data(), data_out.size(), true);

        if (data_out_b64_length < 0) {
          Dmsg0(100, "Could not b64encode data_out\n");
          return false;
        }

        data_out_b64.resize(data_out_b64_length);

        Dmsg0(100, "verification continues\n");
        if (!socket->fsend("%s\n", data_out_b64.c_str())) {
          Dmsg0(100, "could not send extra request: %s\n", socket->bstrerror());
          return false;
        }

        if (socket->recv() <= 0) {
          Dmsg0(100, "could not receive extra answer: %s\n",
                socket->bstrerror());
          return false;
        }

        response.resize(socket->message_length);
        memcpy(response.data(), socket->msg, socket->message_length);
      } break;
      case Done: {
        Dmsg0(100, "verification done\n");
        return socket->fsend("2000 AUTH OK\n");
      } break;
      case Error: {
        Dmsg0(100, "verification failed: %s\n", verifier->err());
        return false;
      } break;
    }
  }
}

bool Respond(BareosSocket* socket, std::span<std::unique_ptr<Prover>> provers)
{
  if (socket->recv() <= 0) { return false; }

  std::string providers_json_b64;
  providers_json_b64.resize(socket->message_length);

  if (bsscanf(socket->msg, "auth: %s", providers_json_b64.data()) != 1) {
    return false;
  }

  providers_json_b64.resize(strlen(providers_json_b64.c_str()));

  std::string providers_json_str;
  providers_json_str.resize((providers_json_b64.size() * 4 + 2) / 3);
  auto length
      = Base64ToBin(providers_json_str.data(), providers_json_str.size() + 1,
                    providers_json_b64.data(), providers_json_b64.size());

  providers_json_str.resize(length);

  std::vector<challenge> challenges;
  if (!ParseChallenges(challenges, providers_json_str)) { return false; }

  auto [chosen, chall] = [&]() -> std::pair<Prover*, challenge*> {
    // provers are sorted by the client (our) preference
    // so we want to take the first one thats offered by the server
    for (auto& claim : provers) {
      for (auto& chal : challenges) {
        if (claim->name() == chal.name) {
          // we found the best fit!
          return {claim.get(), &chal};
        }
      }
    }

    return {nullptr, nullptr};
  }();

  if (!chosen) {
    std::stringstream lists;
    lists << "Client:\n";
    for (auto& claim : provers) { lists << " - " << claim->name() << "\n"; }
    lists << "Server:\n";
    for (auto& chal : challenges) { lists << " - " << chal.name << "\n"; }

    Dmsg0(100, "Could not find a single match for auth:\n%s\n\n",
          lists.str().c_str());
    return false;
  }

  ASSERT(chall);
  Dmsg0(100, "Trying challenge %s\n", chall->name.c_str());

  std::string request_b64{chall->challenge};

  std::vector<char> request{};

  std::vector<char> response;

  std::string response_b64;

  for (;;) {
    response.clear();
    if (request_b64 == "2000 AUTH OK") {
      Dmsg0(100, "Server is happy with %s\n", chall->name.c_str());
      break;
    } else {
      request.resize(request_b64.size() + 1);
      {
        auto request_length
            = Base64ToBin(request.data(), request.size(), request_b64.data(),
                          request_b64.size());

        if (request_length < 0) {
          Dmsg0(100, "Could not b64decode challenge\n");
          return false;
        }

        request.resize(request_length);
      }


      if (!chosen->step(response, request)) {
        Dmsg0(100, "%s step failed: %s\n", chall->name.c_str(), chosen->err());
        return false;
      }

      response_b64.resize((response.size() * 4 + 2) / 3);
      int response_b64_length
          = BinToBase64(response_b64.data(), response_b64.size() + 1,
                        response.data(), response.size(), true);

      if (response_b64_length < 0) {
        Dmsg0(100, "Could not base64 encode response for %s\n",
              chall->name.c_str());
        return false;
      }

      response_b64.resize(response_b64_length);

      if (!socket->fsend("%s: %s\n", chall->name.c_str(),
                         response_b64.c_str())) {
        Dmsg0(100, "Could not send response for %s: %s\n", chall->name.c_str(),
              socket->bstrerror());
        return false;
      }

      if (socket->recv() < 0) {
        Dmsg0(100, "Could not receive request for %s: %s\n",
              chall->name.c_str(), socket->bstrerror());
        return false;
      }

      request_b64.clear();
      request_b64.insert(request_b64.end(), socket->msg,
                         socket->msg + socket->message_length);

      if (request_b64.empty()) {
        Dmsg0(100, "empty request_b64 for %s\n", chall->name.c_str());
        return false;
      }

      if (request_b64.back() != '\n') {
        Dmsg0(100, "missing NL for %s\n", chall->name.c_str());
        return false;
      }

      request_b64.pop_back();
    }
  }

  if (!chosen->done()) {
    Dmsg0(100, "Server finished early for %s\n", chall->name.c_str());
    return false;
  }

  Dmsg0(100, "Finished %s auth successfully\n", chall->name.c_str());
  return true;
}

bool Md5Authenticator::authenticate_outbound(OutboundArgs args)
{
  auto* target = args.target;

  TlsPolicy remote_policy{kBnetTlsUnknown};
  TlsPolicy local_policy = target->GetPolicy();
  if (args.socket->tls_conn) { local_policy = kBnetTlsAuto; }
  if (!cram_md5_handshake(args.jcr, args.socket, cram_identity.c_str(),
                          target->password_.value, local_policy, false,
                          &remote_policy)) {
    return false;
  }

  if (args.socket->tls_conn) {
    // if we already established tls, then there is nothing left to do
    return true;
  }

  switch (select_tls_status(remote_policy, local_policy)) {
    default:
      [[fallthrough]];
    case TlsStatus::Error: {
      Jmsg1(args.jcr, M_ERROR, 0,
            T_("It was not possible to negotiate a shared tls policy with "
               "%s.\n"),
            args.socket->who());
      return false;
    } break;
    case TlsStatus::Disabled: {
      // nothing to do
    } break;
    case TlsStatus::Enabled: {
      // this tls connection does _not_ support tls-psk!
      auto tls = ParameterizeAndInitTlsConnectionAsAClient(
          args.jcr, args.target, nullptr, nullptr);

      if (!tls) {
        Jmsg(args.jcr, M_FATAL, 0,
             "Could initialize secondary tls context for %s\n",
             args.socket->who());
        return false;
      }

      if (!DoTlsHandshakeWithServer(args.jcr, args.socket, std::move(tls))) {
        return false;
      }

      if (target->authenticate_) {
        args.socket->CloseTlsConnectionAndFreeMemory();
      }
    } break;
  }

  return true;
}

bool Md5Authenticator::authenticate_inbound(InboundArgs args)
{
  TlsPolicy remote_policy{kBnetTlsUnknown};
  if (!cram_md5_handshake(nullptr, args.socket, cram_identity.c_str(),
                          args.target->password_.value,
                          args.target->GetPolicy(), true, &remote_policy)) {
    return false;
  }

  if (args.socket->tls_conn) {
    // if we already established tls, then there is nothing left to do
    return true;
  }

  switch (select_tls_status(remote_policy, args.target->GetPolicy())) {
    case TlsStatus::Error: {
      Emsg1(M_ERROR, 0,
            T_("It was not possible to negotiate a shared tls policy with "
               "%s.\n"),
            args.socket->who());
      return false;
    } break;
    case TlsStatus::Disabled: {
      // nothing to do here
    } break;
    case TlsStatus::Enabled: {
      // we do _not_ want tls-psk here, as this path is only used by
      // old clients that do not support tls-psk anyways
      auto tls
          = ParameterizeAndInitTlsConnectionAsAServer(args.target, nullptr);

      if (!tls) {
        Emsg1(M_ERROR, 0, "Could initialize secondary tls context for %s\n",
              args.socket->who());
        return false;
      }

      if (!DoTlsHandshakeWithClient(nullptr, args.socket, std::move(tls))) {
        return false;
      }

      if (args.target->authenticate_) {
        args.socket->CloseTlsConnectionAndFreeMemory();
      }
    } break;
  }

  return true;
}

Md5Authenticator::Md5Authenticator()
    : Md5Authenticator(get_default_cram_identity())
{
}
Md5Authenticator::Md5Authenticator(std::string identity)
    : cram_identity(std::move(identity))
{
  BashSpaces(cram_identity.data());
}


bool NewAuthenticator::authenticate_outbound(
    std::span<std::unique_ptr<Prover>> provers,
    std::span<std::unique_ptr<Verifier>> verifiers,
    OutboundArgs args)
{
  if (!Respond(args.socket, provers)) { return false; }
  return Challenge(args.socket, verifiers);
}
bool NewAuthenticator::authenticate_inbound(InboundArgs args)
{
  if (!Challenge(args.socket, args.verifiers)) { return false; }
  return Respond(args.socket, args.provers);
}

std::optional<Md5Authenticator> GetMd5(auto provers, auto verifiers)
{
  auth::CramMd5::Prover* md5_claim{};
  auth::CramMd5::Verifier* md5_verifier{};

  for (auto& claim : provers) {
    auto* as_md5 = dynamic_cast<auth::CramMd5::Prover*>(claim.get());
    if (as_md5) {
      md5_claim = as_md5;
      break;
    }
  }

  for (auto& verifier : verifiers) {
    auto* as_md5 = dynamic_cast<auth::CramMd5::Verifier*>(verifier.get());
    if (as_md5) {
      md5_verifier = as_md5;
      break;
    }
  }


  if (md5_claim && md5_verifier
      && md5_claim->cram_name() == md5_verifier->cram_name()) {
    return Md5Authenticator{md5_claim->cram_name()};
  }

  return {};
}

bool DefaultAuthenticator::authenticate_outbound(OutboundArgs args)
{
  auto* socket = args.socket;

  static constexpr std::string_view new_auth_prefix = "auth:";

  // messages start with a uint32_t length
  char buffer[sizeof(uint32_t) + new_auth_prefix.size()] = {};

  std::string_view prefix{buffer + sizeof(uint32_t), new_auth_prefix.size()};

  if (socket->peek(buffer, sizeof(buffer)) && prefix == new_auth_prefix) {
    NewAuthenticator auth{};
    auto result = auth.authenticate_outbound({}, {}, args);
    return result;
    // } else if (auto legacy_auth = GetMd5(args.provers, args.verifiers)) {
    //   return legacy_auth->authenticate_outbound(args);
  } else {
    return false;
  }
}
bool DefaultAuthenticator::authenticate_inbound(InboundArgs args)
{
  if (args.remote_version >= VERSION_HEX(26U, 0U, 0U)) {
    NewAuthenticator auth{};
    return auth.authenticate_inbound(args);
  } else if (auto legacy_auth = GetMd5(args.provers, args.verifiers)) {
    return legacy_auth->authenticate_inbound(args);
  } else {
    return false;
  }
}
}  // namespace auth

std::vector<std::unique_ptr<auth::Prover>>
DefaultConnectionInfo::select_provers(ConnectionType)
{
  std::vector<std::unique_ptr<auth::Prover>> res;
  res.emplace_back(std::make_unique<auth::CramMd5::Prover>(
      get_default_cram_identity(), tls.password_.value));
  return res;
}
std::vector<std::unique_ptr<auth::Verifier>>
DefaultConnectionInfo::select_verifiers(ConnectionType)
{
  std::vector<std::unique_ptr<auth::Verifier>> res;
  res.emplace_back(std::make_unique<auth::CramMd5::Verifier>(
      get_default_cram_identity(), tls.password_.value));
  return res;
}


bool Md5OutboundAuthenticator::authenticate(auth::OutboundArgs args)
{
  auto* socket = args.socket;

  static constexpr std::string_view new_auth_prefix = "auth:";

  // messages start with a uint32_t length
  char buffer[sizeof(uint32_t) + new_auth_prefix.size()] = {};

  std::string_view prefix{buffer + sizeof(uint32_t), new_auth_prefix.size()};

  if (socket->peek(buffer, sizeof(buffer)) && prefix == new_auth_prefix) {
    auth::Algorithms algs;

    auto identity = get_default_cram_identity();
    auto password = args.target->password_.value;

    algs.add<auth::CramMd5::Prover>(identity, password);
    algs.add<auth::CramMd5::Verifier>(identity, password);


    auth::NewAuthenticator auth{};
    auto result = auth.authenticate_outbound(algs.prover, algs.verifier, args);
    return result;
  } else {
    auth::Md5Authenticator auth;
    return auth.authenticate_outbound(args);
  }
}
