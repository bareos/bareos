/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
/**
 * Network Utility Routines
 *
 * Kern Sibbald
 */
#include "lib/bsock.h"

#include "include/baconfig.h"
#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/berrno.h"
#include "lib/bnet.h"
#include "lib/cram_md5.h"
#include "lib/global_resource.h"
#include "lib/s_password.h"
#include "lib/tls.h"
#include "lib/tls_conf.h"
#include "lib/tls_conf_cert.h"
#include "lib/util.h"
#include "lib/bstringlist.h"
#include "lib/parse_conf.h"
#include "lib/version.h"
#include "lib/tls_psk_credentials.h"
#include "lib/hello.h"

#include <algorithm>
#include <thread>

static constexpr int debuglevel = 50;

namespace {
void ParameterizeTlsCert(Tls* tls, const TlsConfigCert& tls_cert)
{
  tls->Setca_certfile_(tls_cert.ca_certfile_);
  tls->SetCaCertdir(tls_cert.ca_certdir_);
  tls->SetCrlfile(tls_cert.crlfile_);
  tls->SetCertfile(tls_cert.certfile_);
  tls->SetKeyfile(tls_cert.keyfile_);
  /*      tls->SetPemCallback(TlsPemCallback);
   * --> Feature not implemented: Console Callback */
  /*      tls->SetPemUserdata(tls_cert.pem_message_);
   * --> Feature not implemented: SetPemUserdata */
  tls->SetDhFile(tls_cert.dhfile_);
  tls->SetVerifyPeer(tls_cert.verify_peer_);
}

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
                              std::shared_ptr<Tls> tls,
                              const TlsConfigCert* local_tls_cert)
{
  std::vector<std::string> verify_list;

  if (local_tls_cert->verify_peer_) {
    verify_list = local_tls_cert->allowed_certificate_common_names_;
  }
  if (BnetTlsServer(socket, std::move(tls), verify_list)) { return true; }
  if (jcr && jcr->JobId != 0) {
    Jmsg(jcr, M_FATAL, 0, T_("TLS negotiation failed.\n"));
  }
  Dmsg0(debuglevel, "TLS negotiation failed.\n");
  return false;
}

bool DoTlsHandshakeWithServer(JobControlRecord* jcr,
                              BareosSocket* socket,
                              std::shared_ptr<Tls> tls,
                              const TlsConfigCert* local_tls_cert)
{
  if (BnetTlsClient(socket, std::move(tls), local_tls_cert->verify_peer_,
                    local_tls_cert->allowed_certificate_common_names_)) {
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
    TlsSecretProvider* data)
{
  ASSERT(tls_resource);
  auto result = Tls::CreateNewTlsContext(Tls::ImplementationType::kOpenSsl);
  if (!result) {
    Emsg0(M_ERROR, 0, T_("TLS connection initialization failed.\n"));
    return nullptr;
  }

  result->SetProtocol(tls_resource->protocol_);
  ParameterizeTlsCert(result.get(), tls_resource->tls_cert_);
  result->SetCipherList(tls_resource->cipherlist_);
  result->SetCipherSuites(tls_resource->ciphersuites_);
  result->SetTlsPskServerContext(data);

  if (!result->init()) {
    result.reset();
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

  auto result = Tls::CreateNewTlsContext(Tls::ImplementationType::kOpenSsl);
  if (!result) {
    Qmsg0(jcr, M_FATAL, 0, T_("TLS connection initialization failed.\n"));
    return nullptr;
  }

  result->SetProtocol(tls_resource->protocol_);
  ParameterizeTlsCert(result.get(), tls_resource->tls_cert_);
  result->SetCipherList(tls_resource->cipherlist_);
  result->SetCipherSuites(tls_resource->ciphersuites_);

  if (identity) {
    PskCredentials psk_cred{identity, password};
    result->SetTlsPskClientContext(psk_cred);
  } else {
    Dmsg2(200, "Tls is not configured %s\n", identity);
  }

  if (!result->init()) {
    result.reset();
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

}  // namespace


BareosSocket::BareosSocket()
    /* public */
    : fd_(kInvalidFiledescriptor)
    , read_seqno(0)
    , msg(GetPoolMemory(PM_BSOCK))
    , errmsg(GetPoolMemory(PM_MESSAGE))
    , spool_fd_(kInvalidFiledescriptor)
    , src_addr(nullptr)
    , in_msg_no(0)
    , out_msg_no(0)
    , message_length(0)
    , timer_start{0}
    , b_errno(0)
    , blocking_(1)
    , errors(0)
    , suppress_error_msgs_(false)
    , sleep_time_after_authentication_error(5)
    , client_addr{}
    , peer_addr{}

    /* protected: */
    , jcr_(nullptr)
    , who_(nullptr)
    , host_(nullptr)
    , port_(-1)
    , tid_(nullptr)
    , data_end_{0}
    , FileIndex_(0)
    , timed_out_(false)
    , terminated_(false)
    , cloned_(false)
    , spool_(false)
    , use_bursting_(false)
    , use_keepalive_(true)
    , bwlimit_(0)
    , nb_bytes_(0)
    , last_tick_{0}
{
  Dmsg0(100, "Construct BareosSocket\n");
}

BareosSocket::BareosSocket(const BareosSocket& other)
{
  Dmsg0(100, "Copy Constructor BareosSocket\n");

  fd_ = other.fd_;
  read_seqno = other.read_seqno;
  msg = other.msg;
  errmsg = other.errmsg;
  spool_fd_ = other.spool_fd_;
  src_addr = other.src_addr;
  in_msg_no = other.in_msg_no;
  out_msg_no = other.out_msg_no;
  message_length = other.message_length;
  timer_start = other.timer_start.load();
  b_errno = other.b_errno;
  blocking_ = other.blocking_;
  errors = other.errors.load();
  suppress_error_msgs_ = other.suppress_error_msgs_.load();
  sleep_time_after_authentication_error
      = other.sleep_time_after_authentication_error;
  client_addr = other.client_addr;
  peer_addr = other.peer_addr;
  tls_conn = other.tls_conn;

  /* protected: */
  jcr_ = other.jcr_;
  mutex_ = other.mutex_;
  who_ = other.who_;
  host_ = other.host_;
  port_ = other.port_;
  tid_ = other.tid_;
  data_end_ = other.data_end_;
  FileIndex_ = other.FileIndex_;
  timed_out_ = other.timed_out_;
  terminated_ = other.terminated_;
  cloned_ = other.cloned_;
  spool_ = other.spool_;
  use_bursting_ = other.use_bursting_;
  use_keepalive_ = other.use_keepalive_;
  bwlimit_ = other.bwlimit_;
  nb_bytes_ = other.nb_bytes_;
  last_tick_ = other.last_tick_;

  enable_ktls_ = other.enable_ktls_;
}

BareosSocket::~BareosSocket()
{
  // this line left intentionally blank
  Dmsg0(100, "Destruct BareosSocket\n");
}

void BareosSocket::CloseTlsConnectionAndFreeMemory()
{
  if (!cloned_) {
    LockMutex();
    if (tls_conn) {
      tls_conn->TlsBsockShutdown(this);
      tls_conn.reset();
    }
    UnlockMutex();
  }
}

// Copy the address from the configuration dlist that gets passed in
void BareosSocket::SetSourceAddress(dlist<IPADDR>* src_addr_list)
{
  char allbuf[256 * 10];
  IPADDR* addr = nullptr;

  Dmsg1(100, "All source addresses %s\n",
        BuildAddressesString(src_addr_list, allbuf, sizeof(allbuf)));

  // Delete the object we already have, if it's allocated
  if (src_addr) {
    free(src_addr);
    src_addr = nullptr;
  }

  if (src_addr_list) {
    addr = src_addr_list->first();
    src_addr = new IPADDR(*addr);
  }
}

bool BareosSocket::SetLocking()
{
  if (mutex_) { return true; }
  mutex_ = std::make_shared<std::mutex>();
  return true;
}

void BareosSocket::ClearLocking()
{
  if (!cloned_) {
    if (mutex_) { mutex_.reset(); }
  }
}

void BareosSocket::LockMutex()
{
  if (mutex_) { mutex_->lock(); }
}

void BareosSocket::UnlockMutex()
{
  if (mutex_) { mutex_->unlock(); }
}

// Send a signal
bool BareosSocket::signal(int signal)
{
  message_length = signal;
  if (signal == BNET_TERMINATE) { suppress_error_msgs_ = true; }
  return send();
}

// Despool spooled attributes
bool BareosSocket::despool(void UpdateAttrSpoolSize(ssize_t size),
                           ssize_t tsize)
{
  int32_t pktsiz;
  size_t nbytes;
  ssize_t last = 0, size = 0;
  int count = 0;
  JobControlRecord* jcr = get_jcr();

  if (lseek(spool_fd_, 0, SEEK_SET) == -1) {
    Qmsg(jcr, M_FATAL, 0, T_("attr spool I/O error.\n"));
    return false;
  }

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
  posix_fadvise(spool_fd_, 0, 0, POSIX_FADV_WILLNEED);
#endif

  while ((nbytes = read(spool_fd_, (char*)&pktsiz, sizeof(int32_t)))
         == sizeof(int32_t)) {
    size += sizeof(int32_t);
    message_length = ntohl(pktsiz);
    if (message_length > 0) {
      if (message_length > (int32_t)SizeofPoolMemory(msg)) {
        msg = ReallocPoolMemory(msg, message_length + 1);
      }

      nbytes = read(spool_fd_, msg, message_length);
      if (nbytes != (size_t)message_length) {
        BErrNo be;
        Dmsg2(400, "nbytes=%" PRIuz " message_length=%d\n", nbytes,
              message_length);
        Qmsg1(get_jcr(), M_FATAL, 0, T_("read attr spool error. ERR=%s\n"),
              be.bstrerror());
        UpdateAttrSpoolSize(tsize - last);
        return false;
      }

      size += nbytes;
      if ((++count & 0x3F) == 0) {
        UpdateAttrSpoolSize(size - last);
        last = size;
      }
    }

    send();
    if (jcr && jcr->IsJobCanceled()) { return false; }
  }
  UpdateAttrSpoolSize(tsize - last);

  return true;
}

/**
 * Return the string for the error that occurred
 * on the socket. Only the first error is retained.
 */
const char* BareosSocket::bstrerror()
{
  BErrNo be;
  if (errmsg == nullptr) { errmsg = GetPoolMemory(PM_MESSAGE); }
  PmStrcpy(errmsg, be.bstrerror(b_errno));
  return errmsg;
}

/**
 * Format and send a message
 * Returns: false on error
 *          true  on success
 */
bool BareosSocket::fsend(const char* fmt, ...)
{
  bool result = false;

  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  result = vfsend(fmt, arg_ptr);
  va_end(arg_ptr);

  return result;
}

bool BareosSocket::vfsend(const char* fmt, va_list ap)
{
  if (errors || IsTerminated()) { return false; }
  /* This probably won't work, but we vsnprintf, then if we
   * get a negative length or a length greater than our buffer
   * (depending on which library is used), the printf was truncated, so
   * get a bigger buffer and try again.
   */

  message_length = PmVFormat(msg, fmt, ap);

  if (message_length < 0) { return false; }

  return send();
}

/**
 * Send a message buffer
 * Returns: false on error
 *          true  on success
 */
bool BareosSocket::send(const char* msg_in, uint32_t nbytes)
{
  if (errors || IsTerminated()) { return false; }

  msg = CheckPoolMemorySize(msg, nbytes);
  memcpy(msg, msg_in, nbytes);

  message_length = nbytes;

  return send();
}

void BareosSocket::SetKillable(bool killable)
{
  if (jcr_) { jcr_->SetKillable(killable); }
}

ssize_t BareosSocket::peek(char* buffer, size_t count) const
{
  if (errors || IsTerminated()) { return -1; }
  return ::recv(fd_, buffer, count, MSG_PEEK);
}

std::string BareosSocket::GetCipherMessageString() const
{
  std::string cipher_string{" Encryption: "};
  if (tls_conn) {
    cipher_string += tls_conn->TlsCipherGetName();
  } else {
    cipher_string += "None";
  }
  return cipher_string;
}

// Try to limit the bandwidth of a network connection
void BareosSocket::ControlBwlimit(int bytes)
{
  btime_t now, temp;
  int64_t usec_sleep;

  // If nothing written or read nothing todo.
  if (bytes == 0) { return; }

  // See if this is the first time we enter here.
  now = GetCurrentBtime();
  if (last_tick_ == 0) {
    nb_bytes_ = bytes;
    last_tick_ = now;
    return;
  }

  // Calculate the number of microseconds since the last check.
  temp = now - last_tick_;

  // Less than 0.1ms since the last call, see the next time
  if (temp < 100) {
    nb_bytes_ += bytes;
    return;
  }

  // Keep track of how many bytes are written in this timeslice.
  nb_bytes_ += bytes;
  last_tick_ = now;
  if (debug_level >= 400) {
    Dmsg3(400,
          "ControlBwlimit: now = %" PRId64 ", since = %" PRId64
          ", nb_bytes = %" PRId64 "\n",
          now, temp, nb_bytes_);
  }

  // Take care of clock problems (>10s)
  if (temp > 10000000) { return; }

  // Remove what was authorised to be written in temp usecs.
  nb_bytes_ -= (int64_t)(temp * ((double)bwlimit_ / 1000000.0));
  if (nb_bytes_ < 0) {
    /* If more was authorized then used but bursting is not enabled
     * reset the counter as these bytes cannot be used later on when
     * we are exceeding our bandwidth. */
    if (!use_bursting_) { nb_bytes_ = 0; }
    return;
  }

  // What exceed should be converted in sleep time
  usec_sleep = (int64_t)(nb_bytes_ / ((double)bwlimit_ / 1000000.0));
  if (usec_sleep > 100) {
    if (debug_level >= 400) {
      Dmsg1(400, "ControlBwlimit: sleeping for %" PRId64 " usecs\n",
            usec_sleep);
    }

    // Sleep the right number of usecs.
    while (1) {
      Bmicrosleep(0, usec_sleep);
      now = GetCurrentBtime();

      // See if we slept enough or that Bmicrosleep() returned early.
      if ((now - last_tick_) < usec_sleep) {
        usec_sleep -= (now - last_tick_);
        continue;
      } else {
        last_tick_ = now;
        break;
      }
    }

    /* Subtract the number of bytes we could have sent during the sleep
     * time given the bandwidth limit set. We only do this when we are
     * allowed to burst e.g. use unused bytes from previous timeslices
     * to get an overall bandwidth limiting which may sometimes be below
     * the bandwidth and sometimes above it but the average will be near
     * the set bandwidth. */
    if (use_bursting_) {
      nb_bytes_ -= (int64_t)(usec_sleep * ((double)bwlimit_ / 1000000.0));
    } else {
      nb_bytes_ = 0;
    }
  }
}

void BareosSocket::InitBnetDump(std::string own_qualified_name)
{
  SetBnetDump(BnetDump::Create(own_qualified_name));
}

void BareosSocket::SetBnetDumpDestinationQualifiedName(
    std::string destination_qualified_name)
{
  if (bnet_dump_) {
    bnet_dump_->SetDestinationQualifiedName(destination_qualified_name);
  }
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

Md5Authenticator::Md5Authenticator(std::string name)
    : cram_identity{std::move(name)}
{
  BashSpaces(cram_identity.data());
}

bool Md5Authenticator::authenticate_outbound(OutboundArgs args)
{
  TlsPolicy remote_policy{kBnetTlsUnknown};
  TlsPolicy local_policy = args.target->GetPolicy();
  if (!args.cleartext) { local_policy = kBnetTlsAuto; }
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

      if (!DoTlsHandshakeWithServer(args.jcr, args.socket, std::move(tls),
                                    &args.target->tls_cert_)) {
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

      if (!DoTlsHandshakeWithClient(nullptr, args.socket, std::move(tls),
                                    &args.target->tls_cert_)) {
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

  std::string bashed = qualified_name;
  BashSpaces(bashed.data());

  auth_timer timer{socket};

  bool have_tls = false;
  if (res->IsTlsConfigured() && !cleartext_authentication) {
    auto tls = ParameterizeAndInitTlsConnectionAsAClient(
        jcr, res, qualified_name.c_str(), res->password_.value);

    if (!tls) {
      Jmsg(jcr, M_FATAL, 0, "Could initialize initial tls context for %s\n",
           socket->who());
      return false;
    }

    if (!DoTlsHandshakeWithServer(jcr, socket, tls, &res->tls_cert_)) {
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
    } else {
      have_tls = true;
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
          .cleartext = !have_tls,
          .target = res,
      })) {
    Emsg1(M_ERROR, 0, T_("Bad authentication from %s.\n"), socket->who());
    return false;
  }

  jcr->authenticated = true;
  return true;
}

bool BareosAccept(BareosSocket* socket,
                  const TlsResource* initial_tls,
                  TlsSecretProvider* provider,
                  ClientHelloParser* hello_parser,
                  Authenticator* auth)
{
  // provider is allowed to be NULL in case no tls-psk is wanted
  if (!socket) {
    Emsg1(M_ERROR, 0, "socket is NULL in BareosAccept.\n");
    return false;
  }

  if (!hello_parser) {
    Emsg1(M_ERROR, 0, "auth is NULL in BareosAccept.\n");
    return false;
  }

  if (!initial_tls) {
    Emsg1(M_ERROR, 0, "initial_tls is NULL in BareosAccept.\n");
    return false;
  }

  auth_timer timer{socket};

  bool have_tls = false;

  bool received_clear_text_handshake = false;
  if (!guess_whether_cleartext(socket, &received_clear_text_handshake)) {
    Emsg1(M_ERROR, 0, "Could not check for cleartext handshake with %s\n",
          socket->who());
    return false;
  }

  if (!received_clear_text_handshake) {
    auto tls = ParameterizeAndInitTlsConnectionAsAServer(initial_tls, provider);
    if (!tls) {
      Emsg1(M_ERROR, 0, "Could not initialize initial tls context for %s\n",
            socket->who());
      return false;
    }
    if (!DoTlsHandshakeWithClient(nullptr, socket, std::move(tls),
                                  &initial_tls->tls_cert_)) {
      Emsg1(M_ERROR, 0, "Could not complete tls handshake with %s\n",
            socket->who());
      return false;
    }

    if (initial_tls->authenticate_) {
      // cleanup tls
      socket->CloseTlsConnectionAndFreeMemory();
    } else {
      have_tls = true;
    }
  }

  TlsResource* tls_resource{nullptr};
  {
    if (!socket->recv() || socket->message_length < 0) {
      Emsg1(M_ERROR, 0, T_("Connection request from %s failed.\n"),
            socket->who());
      return false;
    }

    std::string_view hello{socket->msg,
                           static_cast<size_t>(socket->message_length)};


    // auto connection_parser = parse_hello(my_type, hello);
    // if (!connection_parser) {
    //   Emsg1(M_ERROR, 0, "Could not parse hello\n");
    //   return false;
    // }

    // if (GetConnectionSettingsFor()) {}

    tls_resource = hello_parser->parse(hello);
    if (!tls_resource) {
      Emsg1(M_ERROR, 0, T_("Received bad hello message from %s.\n"),
            socket->who());
      return false;
    }

    if (received_clear_text_handshake && tls_resource->tls_require_
        && tls_resource->tls_enable_) {
      // checking for only tls_require is not enough:
      // Nobody sets tls_require to false, when tls_enable is false

      Emsg1(M_ERROR, 0, T_("Received a cleartext hello from %s.\n"),
            socket->who());
      return false;
    }

    if (!auth->authenticate_inbound({
            .socket = socket,
            .cleartext = !have_tls,
            .target = tls_resource,
        })) {
      Emsg1(M_ERROR, 0, T_("Bad authentication from %s.\n"), socket->who());
      return false;
    }
  }

  return true;
}

bool BareosAccept(BareosSocket* socket,
                  const std::string& qualified_name,
                  const TlsResource* initial_tls,
                  TlsSecretProvider* provider,
                  ClientHelloParser* hello_parser)
{
  Md5Authenticator auth{qualified_name};
  return BareosAccept(socket, initial_tls, provider, hello_parser, &auth);
}


#include "lib/hello.h"
#include <fmt/format.h>

struct bashed_printer {
  std::string_view to_print;
};

template <>
struct fmt::formatter<bashed_printer> : fmt::formatter<std::string_view> {
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const bashed_printer& s, fmt::format_context& ctx) const
  {
    auto out = ctx.out();

    for (char c : s.to_print) { *out++ = c == ' ' ? 0x1 : c; }

    return out;
  }
};

using global_resource::Type;

std::string hello_formatter<Type::Director, Type::Storage>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello Director {} calling Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, version.Major, version.Minor,
                     version.Patch);
}
std::string hello_formatter<Type::Director, Type::Client>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello Director {} calling Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, version.Major, version.Minor,
                     version.Patch);
}

std::string hello_formatter<Type::Storage, Type::Storage>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello Start Storage Job {} Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, version.Major, version.Minor,
                     version.Patch);
}
std::string hello_formatter<Type::Storage, Type::Client>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format(
      "Hello Storage calling Start Job {} Version=\"{}.{}.{}\"\n",
      bashed_printer{name}, version.Major, version.Minor, version.Patch);
}

std::string hello_formatter<Type::Client, Type::Storage>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello Start Job {} Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, version.Major, version.Minor,
                     version.Patch);
}
std::string hello_formatter<Type::Client, Type::Director>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format(
      "Hello Client {} FdProtocolVersion=54 calling Version=\"{}.{}.{}\"\n",
      bashed_printer{name}, version.Major, version.Minor, version.Patch);
}

std::string hello_formatter<Type::Console, Type::Director>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello {} calling version {} Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, kBareosVersionStrings.Full,
                     version.Major, version.Minor, version.Patch);
}
