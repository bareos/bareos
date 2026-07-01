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
#include "lib/bauth.h"

#include "include/jcr.h"
#include "lib/bnet.h"
#include "lib/cram_md5.h"
#include "lib/tls_psk_credentials.h"
#include "lib/util.h"

#include <thread>

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
  if (BnetTlsClient(jcr, socket, std::move(tls))) {
    return true;
  }

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


bool Md5Authenticator::authenticate_outbound(OutboundArgs args)
{
  TlsPolicy remote_policy{kBnetTlsUnknown};
  TlsPolicy local_policy = args.target->GetPolicy();
  if (args.socket->tls_conn) { local_policy = kBnetTlsAuto; }
  if (!cram_md5_handshake(args.jcr, args.socket, cram_identity.c_str(),
                          args.target->password_.value, local_policy, false,
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

      if (args.target->authenticate_) {
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

bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   const std::string& qualified_name,
                   const TlsResource* res,
                   std::string_view hello_msg,
                   Authenticator* auth,
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

  if (!auth->authenticate_outbound({
          .jcr = jcr,
          .socket = socket,
          .target = res,
      })) {
    Emsg1(M_ERROR, 0, T_("Bad authentication from %s.\n"), socket->who());
    return false;
  }

  jcr->authenticated = true;
  return true;
}

struct TlsWrapper : public TlsConfigProvider {
  TlsWrapper(TlsConfigProvider* provider) : wrapped{provider} {}

  const TlsResource* get(global_resource::Type type,
                         std::string_view name) override
  {
    psk_res = wrapped->get(type, name);
    if (psk_res) {
      psk_type = type;
      psk_name.assign(name);
    }
    return psk_res;
  }

  TlsConfigProvider* wrapped;

  bool is_set() const { return psk_res; }

  global_resource::Type psk_type{};
  std::string psk_name{};
  const TlsResource* psk_res{};
};

std::optional<ParsedHello> BareosAccept(BareosSocket* socket,
                                        global_resource::Type my_type,
                                        const TlsResource* initial_tls,
                                        TlsConfigProvider* provider,
                                        Authenticator* auth)
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

  if (!auth) {
    Emsg1(M_ERROR, 0, "auth is NULL in BareosAccept.\n");
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

  const TlsResource* tls_resource{nullptr};
  if (!socket->recv() || socket->message_length < 0) {
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

  if (wrapper.is_set()) {
    if (wrapper.psk_name != parsed_hello->name
        || wrapper.psk_type != parsed_hello->type) {
      Emsg1(M_ERROR, 0, T_("tls/cram mismatch detected for %s!\n"),
            socket->who());
      return std::nullopt;
    }
  }

  tls_resource = wrapper.is_set()
                     ? wrapper.psk_res
                     : provider->get(parsed_hello->type, parsed_hello->name);
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

  if (!auth->authenticate_inbound({
          .socket = socket,
          .target = tls_resource,
      })) {
    Emsg1(M_ERROR, 0, T_("Bad authentication from %s.\n"), socket->who());
    return std::nullopt;
  }

  return parsed_hello;
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
