/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
/*
 * Network Utility Routines
 *
 * by Kern Sibbald
 *
 * Adapted and enhanced for BAREOS, originally written
 * for inclusion in the Apcupsd package
 */
/**
 * @file
 * Network Utility Routines
 */


#include <map>

#include "include/bareos.h"
#include "jcr.h"
#include "lib/berrno.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/bsys.h"
#include "lib/ascii_control_characters.h"
#include "lib/bstringlist.h"

#include <netdb.h>
#include "lib/tls.h"

#ifndef INADDR_NONE
#  define INADDR_NONE -1
#endif

/**
 * Receive a message from the other end. Each message consists of
 * two packets. The first is a header that contains the size
 * of the data that follows in the second packet.
 * Returns number of bytes read (may return zero)
 * Returns -1 on signal (BNET_SIGNAL)
 * Returns -2 on hard end of file (BNET_HARDEOF)
 * Returns -3 on error  (BNET_ERROR)
 *
 *  Unfortunately, it is a bit complicated because we have these
 *    four return types:
 *    1. Normal data
 *    2. Signal including end of data stream
 *    3. Hard end of file
 *    4. Error
 *  Using IsBnetStop() and IsBnetError() you can figure this all out.
 */
int32_t BnetRecv(BareosSocket* bsock) { return bsock->recv(); }


/**
 * Return 1 if there are errors on this bsock or it is closed,
 *   i.e. stop communicating on this line.
 */
bool IsBnetStop(BareosSocket* bsock) { return bsock->IsStop(); }

// Return number of errors on socket
int IsBnetError(BareosSocket* bsock) { return bsock->IsError(); }

/**
 * Call here after error during closing to suppress error
 *  messages which are due to the other end shutting down too.
 */
void BnetSuppressErrorMessages(BareosSocket* bsock, bool flag)
{
  bsock->suppress_error_msgs_ = flag;
}

/**
 * Send a message over the network. The send consists of
 * two network packets. The first is sends a 32 bit integer containing
 * the length of the data packet which follows.
 *
 * Returns: false on failure
 *          true  on success
 */
bool BnetSend(BareosSocket* bsock) { return bsock->send(); }


/**
 * Establish a TLS connection -- server side
 *  Returns: true  on success
 *           false on failure
 */
bool BnetTlsServer(BareosSocket* bsock,
                   const std::vector<std::string>& verify_list)
{
  JobControlRecord* jcr = bsock->jcr();

  if (!bsock->tls_conn_init) {
    Dmsg0(100, "No TLS Connection: Cannot call TlsBsockAccept\n");
    goto err;
  }

  if (!bsock->tls_conn_init->TlsBsockAccept(bsock)) {
    Qmsg0(bsock->jcr(), M_FATAL, 0, T_("TLS Negotiation failed.\n"));
    goto err;
  }

  if (!verify_list.empty()) {
    if (!bsock->tls_conn_init->TlsPostconnectVerifyCn(jcr, verify_list)) {
      Qmsg1(bsock->jcr(), M_FATAL, 0,
            T_("TLS certificate verification failed."
               " Peer certificate did not match a required commonName\n"));
      goto err;
    }
  }

  bsock->LockMutex();
  bsock->tls_conn = std::move(bsock->tls_conn_init);
  bsock->UnlockMutex();

  Dmsg0(50, "TLS server negotiation established.\n");
  return true;

err:
  bsock->CloseTlsConnectionAndFreeMemory();
  return false;
}

/**
 * Establish a TLS connection -- client side
 * Returns: true  on success
 *          false on failure
 */
bool BnetTlsClient(BareosSocket* bsock,
                   bool VerifyPeer,
                   const std::vector<std::string>& verify_list)
{
  JobControlRecord* jcr = bsock->jcr();

  if (!bsock->tls_conn_init) {
    Dmsg0(100, "No TLS Connection: Cannot call TlsBsockConnect\n");
    goto err;
  }

  if (!bsock->tls_conn_init->TlsBsockConnect(bsock)) { goto err; }

  if (VerifyPeer) {
    /* If there's an Allowed CN verify list, use that to validate the remote
     * certificate's CN. Otherwise, we use standard host/CN matching. */
    if (!verify_list.empty()) {
      if (!bsock->tls_conn_init->TlsPostconnectVerifyCn(jcr, verify_list)) {
        Qmsg1(bsock->jcr(), M_FATAL, 0,
              T_("TLS certificate verification failed."
                 " Peer certificate did not match a required commonName\n"));
        goto err;
      }
    } else {
      if (!bsock->tls_conn_init->TlsPostconnectVerifyHost(jcr, bsock->host())) {
        Qmsg1(bsock->jcr(), M_FATAL, 0,
              T_("TLS host certificate verification failed. Host name \"%s\" "
                 "did not match presented certificate\n"),
              bsock->host());
        goto err;
      }
    }
  }

  bsock->LockMutex();
  bsock->tls_conn = std::move(bsock->tls_conn_init);
  bsock->UnlockMutex();

  Dmsg0(50, "TLS client negotiation established.\n");
  return true;

err:
  bsock->CloseTlsConnectionAndFreeMemory();
  return false;
}

/**
 * Wait for a specified time for data to appear on
 * the BareosSocket connection.
 *
 *   Returns: 1 if data available
 *            0 if timeout
 *           -1 if error
 */
int BnetWaitData(BareosSocket* bsock, int sec) { return bsock->WaitData(sec); }

// As above, but returns on interrupt
int BnetWaitDataIntr(BareosSocket* bsock, int sec)
{
  return bsock->WaitDataIntr(sec);
}

#ifndef NETDB_INTERNAL
#  define NETDB_INTERNAL -1 /* See errno. */
#endif
#ifndef NETDB_SUCCESS
#  define NETDB_SUCCESS 0 /* No problem. */
#endif
#ifndef HOST_NOT_FOUND
#  define HOST_NOT_FOUND 1 /* Authoritative Answer Host not found. */
#endif
#ifndef TRY_AGAIN
#  define TRY_AGAIN 2 /* Non-Authoritative Host not found, or SERVERFAIL. */
#endif
#ifndef NO_RECOVERY
#  define NO_RECOVERY 3 /* unrecoverable errors, FORMERR, REFUSED, NOTIMP. */
#endif
#ifndef NO_DATA
#  define NO_DATA 4 /* Valid name, no data record of requested type. */
#endif

const char* resolv_host(int family, const char* host, dlist<IPADDR>* addr_list)
{
  int res;
  struct addrinfo hints;
  struct addrinfo *ai, *rp;
  IPADDR* addr;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = family;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = 0;

  res = getaddrinfo(host, NULL, &hints, &ai);
  if (res != 0) { return gai_strerror(res); }

  for (rp = ai; rp != NULL; rp = rp->ai_next) {
    switch (rp->ai_addr->sa_family) {
      case AF_INET:
        addr = new IPADDR(rp->ai_addr->sa_family);
        addr->SetType(IPADDR::R_MULTIPLE);
        /* Some serious casting to get the struct in_addr *
         * rp->ai_addr == struct sockaddr
         * as this is AF_INET family we can cast that
         * to struct_sockaddr_in. Of that we need the
         * address of the sin_addr member which contains a
         * struct in_addr */
        addr->SetAddr4(&(((struct sockaddr_in*)rp->ai_addr)->sin_addr));
        break;
      case AF_INET6:
        addr = new IPADDR(rp->ai_addr->sa_family);
        addr->SetType(IPADDR::R_MULTIPLE);
        /* Some serious casting to get the struct in6_addr *
         * rp->ai_addr == struct sockaddr
         * as this is AF_INET6 family we can cast that
         * to struct_sockaddr_in6. Of that we need the
         * address of the sin6_addr member which contains a
         * struct in6_addr */
        addr->SetAddr6(&(((struct sockaddr_in6*)rp->ai_addr)->sin6_addr));
        break;
      default:
        continue;
    }
    addr_list->append(addr);
  }
  freeaddrinfo(ai);
  return NULL;
}

static IPADDR* add_any(int family)
{
  IPADDR* addr = new IPADDR(family);
  addr->SetType(IPADDR::R_MULTIPLE);
  addr->SetAddrAny();
  return addr;
}

// i host = 0 mean INADDR_ANY only ipv4
dlist<IPADDR>* BnetHost2IpAddrs(const char* host,
                                int family,
                                const char** errstr)
{
  struct in_addr inaddr;
  const char* errmsg;
  struct in6_addr inaddr6;

  dlist<IPADDR>* addr_list = new dlist<IPADDR>();
  if (!host || host[0] == '\0') {
    if (family != 0) {
      addr_list->append(add_any(family));
    } else {
      addr_list->append(add_any(AF_INET));
      addr_list->append(add_any(AF_INET6));
    }
  } else if (inet_aton(host, &inaddr)) { /* MA Bug 4 */
    IPADDR* addr = new IPADDR(AF_INET);
    addr->SetType(IPADDR::R_MULTIPLE);
    addr->SetAddr4(&inaddr);
    addr_list->append(addr);
  } else if (inet_pton(AF_INET6, host, &inaddr6) == 1) {
    IPADDR* addr = new IPADDR(AF_INET6);
    addr->SetType(IPADDR::R_MULTIPLE);
    addr->SetAddr6(&inaddr6);
    addr_list->append(addr);
  } else {
    if (family != 0) {
      errmsg = resolv_host(family, host, addr_list);
      if (errmsg) {
        *errstr = errmsg;
        FreeAddresses(addr_list);
        return 0;
      }
    } else {
      /* We try to resolv host for ipv6 and ipv4, the connection procedure
       * will try to reach the host for each protocols. We report only "Host
       * not found" ipv4 message (no need to have ipv6 and ipv4 messages).
       */
      resolv_host(AF_INET6, host, addr_list);
      errmsg = resolv_host(AF_INET, host, addr_list);

      if (addr_list->size() == 0) {
        *errstr = errmsg;
        FreeAddresses(addr_list);
        return 0;
      }
    }
  }
  return addr_list;
}

/**
 * Return the string for the error that occurred
 * on the socket. Only the first error is retained.
 */
const char* BnetStrerror(BareosSocket* bsock) { return bsock->bstrerror(); }

/**
 * Format and send a message
 *  Returns: false on error
 *           true  on success
 */
bool BnetFsend(BareosSocket* bs, const char* fmt, ...)
{
  va_list arg_ptr;

  if (bs->errors || bs->IsTerminated()) { return false; }

  va_start(arg_ptr, fmt);
  auto res = bs->vfsend(fmt, arg_ptr);
  va_end(arg_ptr);

  return res;
}

int BnetGetPeer(BareosSocket* bs, char* buf, socklen_t buflen)
{
  return bs->GetPeer(buf, buflen);
}

/**
 * Set the network buffer size, suggested size is in size.
 *  Actual size obtained is returned in bs->message_length
 *
 *  Returns: 0 on failure
 *           1 on success
 */
bool BnetSetBufferSize(BareosSocket* bs, uint32_t size, int rw)
{
  return bs->SetBufferSize(size, rw);
}

/**
 * Set socket non-blocking
 * Returns previous socket flag
 */
int BnetSetNonblocking(BareosSocket* bsock) { return bsock->SetNonblocking(); }

/**
 * Set socket blocking
 * Returns previous socket flags
 */
int BnetSetBlocking(BareosSocket* bsock) { return bsock->SetBlocking(); }

// Restores socket flags
void BnetRestoreBlocking(BareosSocket* bsock, int flags)
{
  bsock->RestoreBlocking(flags);
}

bool BnetSig(BareosSocket* bs, int signal) { return bs->signal(signal); }

/**
 * Convert a network "signal" code into
 * human readable ASCII.
 */

/* clang-format off */
std::map<int, std::pair<std::string, std::string>> bnet_signal_to_text {
    {BNET_EOD, {"BNET_EOD", "End of data stream, new data may follow"}},
    {BNET_EOD_POLL, {"BNET_EOD_POLL", "End of data and poll all in one "}},
    {BNET_STATUS, {"BNET_STATUS", "Send full status"}},
    {BNET_TERMINATE, {"BNET_TERMINATE", "Conversation terminated, doing close() "}},
    {BNET_POLL, {"BNET_POLL", "Poll request, I'm hanging on a read "}},
    {BNET_HEARTBEAT, {"BNET_HEARTBEAT", "Heartbeat Response requested "}},
    {BNET_HB_RESPONSE, {"BNET_HB_RESPONSE", "Only response permited to HB "}},
    {BNET_xxxxxxPROMPT, {"BNET_xxxxxxPROMPT", "No longer used -- Prompt for subcommand "}},
    {BNET_BTIME, {"BNET_BTIME", "Send UTC btime "}},
    {BNET_BREAK, {"BNET_BREAK", "Stop current command -- ctl-c "}},
    {BNET_START_SELECT, {"BNET_START_SELECT", "Start of a selection list "}},
    {BNET_END_SELECT, {"BNET_END_SELECT", "End of a select list "}},
    {BNET_INVALID_CMD, {"BNET_INVALID_CMD", "Invalid command sent "}},
    {BNET_CMD_FAILED, {"BNET_CMD_FAILED", "Command failed "}},
    {BNET_CMD_OK, {"BNET_CMD_OK", "Command succeeded "}},
    {BNET_CMD_BEGIN, {"BNET_CMD_BEGIN", "Start command execution "}},
    {BNET_MSGS_PENDING, {"BNET_MSGS_PENDING", "Messages pending "}},
    {BNET_MAIN_PROMPT, {"BNET_MAIN_PROMPT", "Server ready and waiting "}},
    {BNET_SELECT_INPUT, {"BNET_SELECT_INPUT", "Return selection input "}},
    {BNET_WARNING_MSG, {"BNET_WARNING_MSG", "Warning message "}},
    {BNET_ERROR_MSG, {"BNET_ERROR_MSG", "Error message -- command failed "}},
    {BNET_INFO_MSG, {"BNET_INFO_MSG", "Info message -- status line "}},
    {BNET_RUN_CMD, {"BNET_RUN_CMD", "Run command follows "}},
    {BNET_YESNO, {"BNET_YESNO", "Request yes no response "}},
    {BNET_START_RTREE, {"BNET_START_RTREE", "Start restore tree mode "}},
    {BNET_END_RTREE, {"BNET_END_RTREE", "End restore tree mode "}},
    {BNET_SUB_PROMPT, {"BNET_SUB_PROMPT", "Indicate we are at a subprompt "}},
    {BNET_TEXT_INPUT, {"BNET_TEXT_INPUT", "Get text input from user "}},
};
/* clang-format on */

std::string BnetSignalToString(int signal)
{
  if (bnet_signal_to_text.find(signal) != bnet_signal_to_text.end()) {
    return bnet_signal_to_text[signal].first;
  }
  return "Unknown sig " + std::to_string(signal);
}

std::string BnetSignalToString(const BareosSocket* bs)
{
  return BnetSignalToString(bs->message_length);
}

std::string BnetSignalToDescription(int signal)
{
  if (bnet_signal_to_text.find(signal) != bnet_signal_to_text.end()) {
    return bnet_signal_to_text[signal].second;
  }
  return "Unknown sig " + std::to_string(signal);
}

bool ReadoutCommandIdFromMessage(const BStringList& list_of_arguments,
                                 uint32_t& id_out)
{
  if (list_of_arguments.size() < 1) { return false; }

  uint32_t id = kMessageIdUnknown;

  try { /* "1000 OK: <director name> ..." */
    const std::string& first_argument = list_of_arguments.front();
    id = std::stoul(first_argument);
  } catch (const std::exception& e) {
    id_out = kMessageIdProtokollError;
    return false;
  }

  id_out = id;
  return true;
}

bool EvaluateResponseMessageId(const std::string& message,
                               uint32_t& id_out,
                               BStringList& args_out)
{
  BStringList list_of_arguments(message,
                                AsciiControlCharacters::RecordSeparator());
  uint32_t id = kMessageIdUnknown;

  bool ok = ReadoutCommandIdFromMessage(list_of_arguments, id);

  if (ok) { id_out = id; }
  args_out = list_of_arguments;

  return ok;
}

bool BareosSocket::ReceiveAndEvaluateResponseMessage(uint32_t& id_out,
                                                     BStringList& args_out)
{
  StartTimer(30);  // 30 seconds
  int ret = recv();
  StopTimer();

  if (ret <= 0) {
    Dmsg1(100, "Error while receiving response message: %s", msg);
    return false;
  }

  std::string message(msg);

  if (message.empty()) {
    Dmsg0(100, "Received message is empty\n");
    return false;
  }

  return EvaluateResponseMessageId(message, id_out, args_out);
}

bool BareosSocket::FormatAndSendResponseMessage(
    uint32_t id,
    const BStringList& list_of_arguments)
{
  std::string m = std::to_string(id);
  m += AsciiControlCharacters::RecordSeparator();
  m += list_of_arguments.Join(AsciiControlCharacters::RecordSeparator());

  StartTimer(30);  // 30 seconds
  if (!send(m.c_str(), m.size())) {
    Dmsg1(100, "Could not send response message: %s\n", m.c_str());
    StopTimer();
    return false;
  }
  StopTimer();
  return true;
}

bool BareosSocket::FormatAndSendResponseMessage(uint32_t id,
                                                const std::string& str)
{
  BStringList message;
  message << str;

  return FormatAndSendResponseMessage(id, message);
}
