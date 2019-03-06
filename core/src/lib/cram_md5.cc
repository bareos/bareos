/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.
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
/*
 * Challenge Response Authentication Method using MD5 (CRAM-MD5)
 * cram-md5 is based on RFC2104.
 *
 * Kern E. Sibbald, May MMI.
 */

#include "include/bareos.h"
#include "lib/cram_md5.h"
#include "lib/bsock.h"


CramMd5Handshake::CramMd5Handshake(BareosSocket* bs,
                                   const char* password,
                                   TlsPolicy local_tls_policy)
    : bs_(bs), password_(password), local_tls_policy_(local_tls_policy)
{
  return;
}


/* Authorize other end
 * Codes that tls_local_need and tls_remote_need can take:
 *
 *   kBnetTlsNone     I cannot do tls
 *   BNET_TLS_CERTIFICATE_ALLOWED       I can do tls, but it is not required on
 * my end BNET_TLS_CERTIFICATE_REQUIRED  tls is required on my end
 *
 *   Returns: false if authentication failed
 *            true if OK
 */
bool CramMd5Handshake::CramMd5Challenge()
{
  PoolMem chal(PM_NAME), host(PM_NAME);

  InitRandom();

  host.check_size(MAXHOSTNAMELEN);
  if (!gethostname(host.c_str(), MAXHOSTNAMELEN)) { PmStrcpy(host, my_name); }

  /* Send challenge -- no hashing yet */
  Mmsg(chal, "<%u.%u@%s>", (uint32_t)random(), (uint32_t)time(NULL),
       host.c_str());

  Dmsg2(debuglevel_, "send: auth cram-md5 %s ssl=%d\n", chal.c_str(),
        local_tls_policy_);
  if (!bs_->fsend("auth cram-md5 %s ssl=%d\n", chal.c_str(),
                  local_tls_policy_)) {
    Dmsg1(debuglevel_, "Bnet send challenge comm error. ERR=%s\n",
          bs_->bstrerror());
    return false;
  }

  /* Read hashed response to challenge */
  if (bs_->WaitData(180) <= 0 || bs_->recv() <= 0) {
    Dmsg1(debuglevel_, "Bnet receive challenge response comm error. ERR=%s\n",
          bs_->bstrerror());
    Bmicrosleep(bs_->sleep_time_after_authentication_error, 0);
    return false;
  }

  uint8_t hmac[20];
  /* Attempt to duplicate hash with our password */
  hmac_md5((uint8_t*)chal.c_str(), strlen(chal.c_str()), (uint8_t*)password_,
           strlen(password_), hmac);
  BinToBase64(host.c_str(), MAXHOSTNAMELEN, (char*)hmac, 16, compatible_);
  bool ok = bstrcmp(bs_->msg, host.c_str());
  if (ok) {
    Dmsg1(debuglevel_, "Authenticate OK %s\n", host.c_str());
  } else {
    BinToBase64(host.c_str(), MAXHOSTNAMELEN, (char*)hmac, 16, false);
    ok = bstrcmp(bs_->msg, host.c_str());
    if (!ok) {
      Dmsg2(debuglevel_, "Authenticate NOT OK: wanted %s, got %s\n",
            host.c_str(), bs_->msg);
    }
  }
  if (ok) {
    bs_->fsend("1000 OK auth\n");
  } else {
    bs_->fsend(_("1999 Authorization failed.\n"));
    Bmicrosleep(bs_->sleep_time_after_authentication_error, 0);
  }
  return ok;
}

bool CramMd5Handshake::CramMd5Response()
{
  PoolMem chal(PM_NAME);
  uint8_t hmac[20];

  compatible_ = false;
  if (bs_->recv() <= 0) {
    Bmicrosleep(bs_->sleep_time_after_authentication_error, 0);
    return false;
  }

  Dmsg1(100, "cram-get received: %s", bs_->msg);
  chal.check_size(bs_->message_length);
  if (sscanf(bs_->msg, "auth cram-md5c %s ssl=%d", chal.c_str(),
             &remote_tls_policy_) == 2) {
    compatible_ = true;
  } else if (sscanf(bs_->msg, "auth cram-md5 %s ssl=%d", chal.c_str(),
                    &remote_tls_policy_) != 2) {
    if (sscanf(bs_->msg, "auth cram-md5 %s\n", chal.c_str()) != 1) {
      Dmsg1(debuglevel_, "Cannot scan challenge: %s", bs_->msg);
      bs_->fsend(_("1999 Authorization failed.\n"));
      Bmicrosleep(bs_->sleep_time_after_authentication_error, 0);
      return false;
    }
  }

  hmac_md5((uint8_t*)chal.c_str(), strlen(chal.c_str()), (uint8_t*)password_,
           strlen(password_), hmac);
  bs_->message_length =
      BinToBase64(bs_->msg, 50, (char*)hmac, 16, compatible_) + 1;
  if (!bs_->send()) {
    Dmsg1(debuglevel_, "Send challenge failed. ERR=%s\n", bs_->bstrerror());
    return false;
  }
  Dmsg1(99, "sending resp to challenge: %s\n", bs_->msg);
  if (bs_->WaitData(180) <= 0 || bs_->recv() <= 0) {
    Dmsg1(debuglevel_, "Receive challenge response failed. ERR=%s\n",
          bs_->bstrerror());
    Bmicrosleep(bs_->sleep_time_after_authentication_error, 0);
    return false;
  }
  if (bstrcmp(bs_->msg, "1000 OK auth\n")) { return true; }
  Dmsg1(debuglevel_, "Received bad response: %s\n", bs_->msg);
  Bmicrosleep(bs_->sleep_time_after_authentication_error, 0);
  return false;
}

bool CramMd5Handshake::DoHandshake(bool initiated_by_remote)
{
  if (initiated_by_remote) {
    if (CramMd5Challenge()) {
      if (CramMd5Response()) { return true; }
    }
  } else {
    if (CramMd5Response()) {
      if (CramMd5Challenge()) { return true; }
    }
  }

  Dmsg1(debuglevel_, "cram-auth failed with %s\n", bs_->who());
  return false;
}

void CramMd5Handshake::InitRandom() const
{
  struct timeval t1;
  struct timeval t2;
  struct timezone tz;

  gettimeofday(&t1, &tz);
  for (int i = 0; i < 4; i++) { gettimeofday(&t2, &tz); }
  srandom((t1.tv_sec & 0xffff) * (t2.tv_usec & 0xff));
}
