/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/bnet.h"
#include "lib/cram_md5.h"
#include "lib/tls.h"
#include "lib/util.h"
#include "lib/bstringlist.h"

static constexpr int debuglevel = 50;

BareosSocket::BareosSocket()
    /* public */
    : fd_(-1)
    , read_seqno(0)
    , msg(GetPoolMemory(PM_BSOCK))
    , errmsg(GetPoolMemory(PM_MESSAGE))
    , spool_fd_(-1)
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
    , client_addr{0}
    , peer_addr{0}

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
    , tls_established_(false)
{
  Dmsg0(100, "Contruct BareosSocket\n");
}

BareosSocket::BareosSocket(const BareosSocket &other)
{
  Dmsg0(100, "Copy Contructor BareosSocket\n");

  fd_                                   = other.fd_;
  read_seqno                            = other.read_seqno;
  msg                                   = other.msg;
  errmsg                                = other.errmsg;
  spool_fd_                             = other.spool_fd_;
  src_addr                              = other.src_addr;
  in_msg_no                             = other.in_msg_no;
  out_msg_no                            = other.out_msg_no;
  message_length                        = other.message_length;
  timer_start                           = other.timer_start;
  b_errno                               = other.b_errno;
  blocking_                             = other.blocking_;
  errors                                = other.errors;
  suppress_error_msgs_                  = other.suppress_error_msgs_;
  sleep_time_after_authentication_error = other.sleep_time_after_authentication_error;
  client_addr                           = other.client_addr;
  peer_addr                             = other.peer_addr;
  tls_conn                              = other.tls_conn;

  /* protected: */
  jcr_             = other.jcr_;
  mutex_           = other.mutex_;
  who_             = other.who_;
  host_            = other.host_;
  port_            = other.port_;
  tid_             = other.tid_;
  data_end_        = other.data_end_;
  FileIndex_       = other.FileIndex_;
  timed_out_       = other.timed_out_;
  terminated_      = other.terminated_;
  cloned_          = other.cloned_;
  spool_           = other.spool_;
  use_bursting_    = other.use_bursting_;
  use_keepalive_   = other.use_keepalive_;
  bwlimit_         = other.bwlimit_;
  nb_bytes_        = other.nb_bytes_;
  last_tick_       = other.last_tick_;
  tls_established_ = other.tls_established_;
}

BareosSocket::~BareosSocket() { Dmsg0(100, "Destruct BareosSocket\n"); }

void BareosSocket::CloseTlsConnectionAndFreeMemory()
{
  if (!cloned_) {
    LockMutex();
    if (tls_conn && !tls_conn_init) {
      tls_conn->TlsBsockShutdown(this);
      tls_conn.reset();
    } else if (tls_conn_init) {
      tls_conn_init->TlsBsockShutdown(this);
      tls_conn_init.reset();
    }
    UnlockMutex();
  }
}

/**
 * Copy the address from the configuration dlist that gets passed in
 */
void BareosSocket::SetSourceAddress(dlist *src_addr_list)
{
  char allbuf[256 * 10];
  IPADDR *addr = nullptr;

  Dmsg1(100, "All source addresses %s\n", BuildAddressesString(src_addr_list, allbuf, sizeof(allbuf)));

  /*
   * Delete the object we already have, if it's allocated
   */
  if (src_addr) {
    free(src_addr);
    src_addr = nullptr;
  }

  if (src_addr_list) {
    addr     = (IPADDR *)src_addr_list->first();
    src_addr = New(IPADDR(*addr));
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
    if (mutex_) {
      mutex_.reset();
    }
  }
}

void BareosSocket::LockMutex()
{
   if (mutex_) {
      mutex_->lock();
   }
}

void BareosSocket::UnlockMutex()
{
   if (mutex_) {
      mutex_->unlock();
   }
}

/**
 * Send a signal
 */
bool BareosSocket::signal(int signal)
{
  message_length = signal;
  if (signal == BNET_TERMINATE) { suppress_error_msgs_ = true; }
  return send();
}

/**
 * Despool spooled attributes
 */
bool BareosSocket::despool(void UpdateAttrSpoolSize(ssize_t size), ssize_t tsize)
{
  int32_t pktsiz;
  size_t nbytes;
  ssize_t last = 0, size = 0;
  int count             = 0;
  JobControlRecord *jcr = get_jcr();

  if (lseek(spool_fd_, 0, SEEK_SET) == -1) {
    Qmsg(jcr, M_FATAL, 0, _("attr spool I/O error.\n"));
    return false;
  }

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
  posix_fadvise(spool_fd_, 0, 0, POSIX_FADV_WILLNEED);
#endif

  while ((nbytes = read(spool_fd_, (char *)&pktsiz, sizeof(int32_t))) == sizeof(int32_t)) {
    size += sizeof(int32_t);
    message_length = ntohl(pktsiz);
    if (message_length > 0) {
      if (message_length > (int32_t)SizeofPoolMemory(msg)) {
        msg = ReallocPoolMemory(msg, message_length + 1);
      }

      nbytes = read(spool_fd_, msg, message_length);
      if (nbytes != (size_t)message_length) {
        BErrNo be;
        Dmsg2(400, "nbytes=%d message_length=%d\n", nbytes, message_length);
        Qmsg1(get_jcr(), M_FATAL, 0, _("read attr spool error. ERR=%s\n"), be.bstrerror());
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
    if (jcr && JobCanceled(jcr)) { return false; }
  }
  UpdateAttrSpoolSize(tsize - last);

  return true;
}

/**
 * Return the string for the error that occurred
 * on the socket. Only the first error is retained.
 */
const char *BareosSocket::bstrerror()
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
bool BareosSocket::fsend(const char *fmt, ...)
{
  va_list arg_ptr;
  int maxlen;

  if (errors || IsTerminated()) { return false; }
  /* This probably won't work, but we vsnprintf, then if we
   * get a negative length or a length greater than our buffer
   * (depending on which library is used), the printf was truncated, so
   * get a bigger buffer and try again.
   */
  for (;;) {
    maxlen = SizeofPoolMemory(msg) - 1;
    va_start(arg_ptr, fmt);
    message_length = Bvsnprintf(msg, maxlen, fmt, arg_ptr);
    va_end(arg_ptr);
    if (message_length >= 0 && message_length < (maxlen - 5)) { break; }
    msg = ReallocPoolMemory(msg, maxlen + maxlen / 2);
  }
  return send();
}

/**
 * Send a message buffer
 * Returns: false on error
 *          true  on success
 */
bool BareosSocket::send(const char *msg_in, uint32_t nbytes)
{
   if (errors || IsTerminated()) {
      return false;
   }

   msg = CheckPoolMemorySize(msg, nbytes);
   memcpy(msg, msg_in, nbytes);

   message_length = nbytes;

   return send();
}

void BareosSocket::SetKillable(bool killable)
{
  if (jcr_) { jcr_->SetKillable(killable); }
}

/** Commands sent to Director */
static char hello[] = "Hello %s calling\n";


bool BareosSocket::ConsoleAuthenticateWithDirector(JobControlRecord *jcr,
                                                   const char *identity,
                                                   s_password &password,
                                                   TlsResource *tls_resource,
                                                   BStringList &response_args,
                                                   uint32_t &response_id)
{
  char bashed_name[MAX_NAME_LENGTH];
  BareosSocket *dir = this; /* for readability */

  bstrncpy(bashed_name, identity, sizeof(bashed_name));
  BashSpaces(bashed_name);

  dir->StartTimer(60 * 5); /* 5 minutes */
  dir->fsend(hello, bashed_name);

  if (!AuthenticateOutboundConnection(jcr, "Director", identity, password, tls_resource)) {
    Dmsg0(100, "Authenticate outbound connection failed\n");
    dir->StopTimer();
    return false;
  }

  Dmsg1(6, ">dird: %s", dir->msg);

  uint32_t message_id;
  BStringList args;
  if (dir->ReceiveAndEvaluateResponseMessage(message_id, args)) {
    response_id = message_id;
    response_args = args;
    return true;
  }
  Dmsg0(100, "Wrong Message Protocol ID\n");
  return false;
}

/**
 * Depending on the initiate parameter perform one of the following:
 *
 * - First make him prove his identity and then prove our identity to the Remote.
 * - First prove our identity to the Remote and then make him prove his identity.
 */
bool BareosSocket::TwoWayAuthenticate(JobControlRecord *jcr,
                                      const char *what,
                                      const char *identity,
                                      s_password &password,
                                      TlsResource *tls_resource,
                                      bool initiated_by_remote)
{
  bool auth_success = false;

  if (jcr && JobCanceled(jcr)) {
    Dmsg0(debuglevel, "Failed, because job is canceled.\n");
  } else if (password.encoding != p_encoding_md5) {
    Jmsg(jcr, M_FATAL, 0,
         _("Password encoding is not MD5. You are probably restoring a NDMP Backup "
           "with a restore job not configured for NDMP protocol.\n"));
  } else {
    TlsPolicy local_tls_policy = tls_resource->GetPolicy();
    CramMd5Handshake cram_md5_handshake(this, password.value, local_tls_policy);

    btimer_t *tid = StartBsockTimer(this, AUTH_TIMEOUT);

    if (ConnectionReceivedTerminateSignal()) { return false; }

    auth_success = cram_md5_handshake.DoHandshake(initiated_by_remote);
    if (!auth_success) {
      Jmsg(jcr, M_FATAL, 0,
           _("Authorization key rejected by %s %s.\n"
             "Please see %s for help.\n"),
           what, identity, MANUAL_AUTH_URL);
    } else if (jcr && JobCanceled(jcr)) {
      Dmsg0(debuglevel, "Failed, because job is canceled.\n");
      auth_success = false;
    } else if (!DoTlsHandshake(cram_md5_handshake.RemoteTlsPolicy(), tls_resource, initiated_by_remote,
                               identity, password.value, jcr)) {
      auth_success = false;
    }
    if (tid) { StopBsockTimer(tid); }
  }

  if (jcr) { jcr->authenticated = auth_success; }

  return auth_success;
}

bool BareosSocket::DoTlsHandshakeAsAServer(ConfigurationParser *config, JobControlRecord *jcr)
{
  TlsResource *tls_resource = reinterpret_cast<TlsResource *>(config->GetNextRes(config->r_own_, nullptr));
  if (!tls_resource) {
    Dmsg1(100, "Could not get tls resource for %d.\n", config->r_own_);
    return false;
  }

  if (!ParameterizeAndInitTlsConnectionAsAServer(config)) { return false; }

  if (!DoTlsHandshakeWithClient(&tls_resource->tls_cert_, jcr)) { return false; }

  if (tls_resource->authenticate_) { /* tls authentication only? */
    CloseTlsConnectionAndFreeMemory(); /* yes, shutdown tls */
  }

  return true;
}

void BareosSocket::ParameterizeTlsCert(Tls *tls_conn_init, TlsResource *tls_resource)
{
  const std::string empty;
  tls_conn_init->Setca_certfile_(tls_resource->tls_cert_.ca_certfile_ ? *tls_resource->tls_cert_.ca_certfile_ : empty);
  tls_conn_init->SetCaCertdir(tls_resource->tls_cert_.ca_certdir_ ? *tls_resource->tls_cert_.ca_certdir_ : empty);
  tls_conn_init->SetCrlfile(tls_resource->tls_cert_.crlfile_ ? *tls_resource->tls_cert_.crlfile_ : empty);
  tls_conn_init->SetCertfile(tls_resource->tls_cert_.certfile_ ? *tls_resource->tls_cert_.certfile_ : empty);
  tls_conn_init->SetKeyfile(tls_resource->tls_cert_.keyfile_ ? *tls_resource->tls_cert_.keyfile_ : empty);
  //      tls_conn_init->SetPemCallback(TlsPemCallback); Ueb: --> Feature not implemented: Console Callback
  tls_conn_init->SetPemUserdata(tls_resource->tls_cert_.pem_message_);
  tls_conn_init->SetDhFile(tls_resource->tls_cert_.dhfile_ ? *tls_resource->tls_cert_.dhfile_ : empty);
  tls_conn_init->SetCipherList(tls_resource->cipherlist_ ? *tls_resource->cipherlist_ : empty);
  tls_conn_init->SetVerifyPeer(tls_resource->tls_cert_.verify_peer_);
}

bool BareosSocket::ParameterizeAndInitTlsConnectionAsAServer(ConfigurationParser *config)
{
  tls_conn_init.reset(Tls::CreateNewTlsContext(Tls::TlsImplementationType::kTlsOpenSsl));
  if (!tls_conn_init) {
    Qmsg0(BareosSocket::jcr(), M_FATAL, 0, _("TLS connection initialization failed.\n"));
    return false;
  }

  tls_conn_init->SetTcpFileDescriptor(fd_);

  TlsResource *tls_resource = reinterpret_cast<TlsResource *>(config->GetNextRes(config->r_own_, nullptr));
  if (!tls_resource) {
    Dmsg1(100, "Could not get tls resource for %d.\n", config->r_own_);
    return false;
  }

  ParameterizeTlsCert(tls_conn_init.get(), tls_resource);

  tls_conn_init->SetTlsPskServerContext(config, config->GetTlsPskByFullyQualifiedResourceName);

  if (!tls_conn_init->init()) {
    tls_conn_init.reset();
    return false;
  }
  return true;
}

bool BareosSocket::DoTlsHandshake(TlsPolicy remote_tls_policy,
                                  TlsResource *tls_resource,
                                  bool initiated_by_remote,
                                  const char *identity,
                                  const char *password,
                                  JobControlRecord *jcr)
{
  if (tls_conn) { return true; }

  int tls_policy = SelectTlsPolicy(tls_resource, remote_tls_policy);

  if (tls_policy == TlsPolicy::kBnetTlsDeny) { /* tls required but not configured */
    return false;
  }
  if (tls_policy != TlsPolicy::kBnetTlsNone) { /* no tls configuration is ok */

    if (!ParameterizeAndInitTlsConnection(tls_resource, identity, password, initiated_by_remote)) {
      return false;
    }

    if (initiated_by_remote) {
      if (!DoTlsHandshakeWithClient(&tls_resource->tls_cert_, jcr)) { return false; }
    } else {
      if (!DoTlsHandshakeWithServer(&tls_resource->tls_cert_, identity, password, jcr)) { return false; }
    }

    if (tls_resource->authenticate_) { /* tls authentication only */
      CloseTlsConnectionAndFreeMemory();
    }
  }
  if (!initiated_by_remote) {
    if (tls_conn) {
      tls_conn->TlsLogConninfo(jcr, host(), port(), who());
    } else {
      Qmsg(jcr, M_INFO, 0, _("Connected %s at %s:%d, encryption: None\n"), who(), host(), port());
    }
  }
  return true;
}

bool BareosSocket::ParameterizeAndInitTlsConnection(TlsResource *tls_resource,
                                                    const char *identity,
                                                    const char *password,
                                                    bool initiated_by_remote)
{
  if (!tls_resource->IsTlsConfigured()) { return true; }

  tls_conn_init.reset(Tls::CreateNewTlsContext(Tls::TlsImplementationType::kTlsOpenSsl));
  if (!tls_conn_init) {
    Qmsg0(BareosSocket::jcr(), M_FATAL, 0, _("TLS connection initialization failed.\n"));
    return false;
  }

  tls_conn_init->SetTcpFileDescriptor(fd_);

  ParameterizeTlsCert(tls_conn_init.get(), tls_resource);

  if (tls_resource->IsTlsConfigured()) {
    if (!initiated_by_remote) {
      const PskCredentials psk_cred(identity, password);
      tls_conn_init->SetTlsPskClientContext(psk_cred);
    }
  }

  if (!tls_conn_init->init()) {
    tls_conn_init.reset();
    return false;
  }
  return true;
}

bool BareosSocket::DoTlsHandshakeWithClient(TlsConfigCert *local_tls_cert, JobControlRecord *jcr)
{
  std::vector<std::string> verify_list;

  if (local_tls_cert->verify_peer_) {
    verify_list = local_tls_cert->AllowedCertificateCommonNames();
  }
  if (BnetTlsServer(this, verify_list)) {
    return true;
  }
  if (jcr && jcr->JobId != 0) {
    Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
  }
  Dmsg0(debuglevel, "TLS negotiation failed.\n");
  return false;
}

bool BareosSocket::DoTlsHandshakeWithServer(TlsConfigCert *local_tls_cert,
                                            const char *identity,
                                            const char *password,
                                            JobControlRecord *jcr)
{
  if (BnetTlsClient(this, local_tls_cert->verify_peer_,
                    local_tls_cert->AllowedCertificateCommonNames())) {
    return true;
  }
  if (jcr && jcr->JobId != 0) {
    Jmsg(jcr, M_FATAL, 0, _("TLS negotiation failed.\n"));
  }
  Dmsg0(debuglevel, "TLS negotiation failed.\n");
  return false;
}

bool BareosSocket::AuthenticateOutboundConnection(JobControlRecord *jcr,
                                                  const char *what,
                                                  const char *identity,
                                                  s_password &password,
                                                  TlsResource *tls_resource)
{
  return TwoWayAuthenticate(jcr, what, identity, password, tls_resource, false);
}

bool BareosSocket::AuthenticateInboundConnection(JobControlRecord *jcr,
                                                 const char *what,
                                                 const char *identity,
                                                 s_password &password,
                                                 TlsResource *tls_resource)
{
  return TwoWayAuthenticate(jcr, what, identity, password, tls_resource, true);
}

bool BareosSocket::EvaluateCleartextBareosHello(bool &cleartext_hello,
                                                std::string &client_name,
                                                std::string &r_code_str) const
{
  char buffer[256];
  memset(buffer, 0, sizeof(buffer));
  int ret = ::recv(fd_, buffer, 255, MSG_PEEK);
  if (ret >= 10) {
    std::string hello("Hello ");
    std::string received(&buffer[4]);
    cleartext_hello = received.compare(0, hello.size(), hello) == 0;
    if (cleartext_hello) {
      std::string name;
      std::string code;
      if (GetNameAndResourceTypeFromHello(received, name, code)) {
        client_name = name;
        r_code_str = code;
      }
    }
    return true;
  }
  return false;
}

void BareosSocket::GetCipherMessageString(std::string &str) const
{
   if (tls_conn) {
     std::string m;
     m = " Encryption: ";
     m += tls_conn->TlsCipherGetName();
     m += "\n";
     str = m;
   } else {
     str = " Encryption: None\n";
   }
}

void BareosSocket::OutputCipherMessageString(std::function<void(const char *)> output_cb)
{
   std::string str;
   GetCipherMessageString(str);
   output_cb(str.c_str());
}

/**
 * Try to limit the bandwidth of a network connection
 */
void BareosSocket::ControlBwlimit(int bytes)
{
  btime_t now, temp;
  int64_t usec_sleep;

  /*
   * If nothing written or read nothing todo.
   */
  if (bytes == 0) { return; }

  /*
   * See if this is the first time we enter here.
   */
  now = GetCurrentBtime();
  if (last_tick_ == 0) {
    nb_bytes_  = bytes;
    last_tick_ = now;
    return;
  }

  /*
   * Calculate the number of microseconds since the last check.
   */
  temp = now - last_tick_;

  /*
   * Less than 0.1ms since the last call, see the next time
   */
  if (temp < 100) {
    nb_bytes_ += bytes;
    return;
  }

  /*
   * Keep track of how many bytes are written in this timeslice.
   */
  nb_bytes_ += bytes;
  last_tick_ = now;
  if (debug_level >= 400) {
    Dmsg3(400, "ControlBwlimit: now = %lld, since = %lld, nb_bytes = %d\n", now, temp, nb_bytes_);
  }

  /*
   * Take care of clock problems (>10s)
   */
  if (temp > 10000000) { return; }

  /*
   * Remove what was authorised to be written in temp usecs.
   */
  nb_bytes_ -= (int64_t)(temp * ((double)bwlimit_ / 1000000.0));
  if (nb_bytes_ < 0) {
    /*
     * If more was authorized then used but bursting is not enabled
     * reset the counter as these bytes cannot be used later on when
     * we are exceeding our bandwidth.
     */
    if (!use_bursting_) { nb_bytes_ = 0; }
    return;
  }

  /*
   * What exceed should be converted in sleep time
   */
  usec_sleep = (int64_t)(nb_bytes_ / ((double)bwlimit_ / 1000000.0));
  if (usec_sleep > 100) {
    if (debug_level >= 400) { Dmsg1(400, "ControlBwlimit: sleeping for %lld usecs\n", usec_sleep); }

    /*
     * Sleep the right number of usecs.
     */
    while (1) {
      Bmicrosleep(0, usec_sleep);
      now = GetCurrentBtime();

      /*
       * See if we slept enough or that Bmicrosleep() returned early.
       */
      if ((now - last_tick_) < usec_sleep) {
        usec_sleep -= (now - last_tick_);
        continue;
      } else {
        last_tick_ = now;
        break;
      }
    }

    /*
     * Subtract the number of bytes we could have sent during the sleep
     * time given the bandwidth limit set. We only do this when we are
     * allowed to burst e.g. use unused bytes from previous timeslices
     * to get an overall bandwidth limiting which may sometimes be below
     * the bandwidth and sometimes above it but the average will be near
     * the set bandwidth.
     */
    if (use_bursting_) {
      nb_bytes_ -= (int64_t)(usec_sleep * ((double)bwlimit_ / 1000000.0));
    } else {
      nb_bytes_ = 0;
    }
  }
}
