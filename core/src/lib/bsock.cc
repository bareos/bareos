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
#include "lib/bauth.h"

#include <algorithm>
#include <thread>

static constexpr int debuglevel = 50;

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

  if (tls_conn) { return tls_conn->TlsBsockPeekn(this, buffer, count); }

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
