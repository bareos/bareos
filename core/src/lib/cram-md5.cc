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

const int debuglevel = 50;

/* Authorize other end
 * Codes that tls_local_need and tls_remote_need can take:
 *
 *   BNET_TLS_NONE     I cannot do tls
 *   BNET_TLS_CERTIFICATE_ALLOWED       I can do tls, but it is not required on my end
 *   BNET_TLS_CERTIFICATE_REQUIRED  tls is required on my end
 *
 *   Returns: false if authentication failed
 *            true if OK
 */
bool cram_md5_challenge(BareosSocket *bs, const char *password, uint32_t tls_local_need, bool compatible)
{
   struct timeval t1;
   struct timeval t2;
   struct timezone tz;
   int i;
   bool ok;
   PoolMem chal(PM_NAME),
            host(PM_NAME);
   uint8_t hmac[20];

   gettimeofday(&t1, &tz);
   for (i=0; i<4; i++) {
      gettimeofday(&t2, &tz);
   }
   srandom((t1.tv_sec & 0xffff) * (t2.tv_usec & 0xff));

   host.check_size(MAXHOSTNAMELEN);
   if (!gethostname(host.c_str(), MAXHOSTNAMELEN)) {
      PmStrcpy(host, my_name);
   }

   /* Send challenge -- no hashing yet */
   Mmsg(chal, "<%u.%u@%s>", (uint32_t)random(), (uint32_t)time(NULL), host.c_str());

   Dmsg2(debuglevel, "send: auth cram-md5 %s ssl=%d\n", chal.c_str(), tls_local_need);
   if (!bs->fsend("auth cram-md5 %s ssl=%d\n", chal.c_str(), tls_local_need)) {
      Dmsg1(debuglevel, "Bnet send challenge comm error. ERR=%s\n", bs->bstrerror());
      return false;
   }

   /* Read hashed response to challenge */
   if (bs->WaitData(180) <= 0 || bs->recv() <= 0) {
      Dmsg1(debuglevel, "Bnet receive challenge response comm error. ERR=%s\n", bs->bstrerror());
      bmicrosleep(5, 0);
      return false;
   }

   /* Attempt to duplicate hash with our password */
   hmac_md5((uint8_t *)chal.c_str(), strlen(chal.c_str()), (uint8_t *)password, strlen(password), hmac);
   bin_to_base64(host.c_str(), MAXHOSTNAMELEN, (char *)hmac, 16, compatible);
   ok = bstrcmp(bs->msg, host.c_str());
   if (ok) {
      Dmsg1(debuglevel, "Authenticate OK %s\n", host.c_str());
   } else {
      bin_to_base64(host.c_str(), MAXHOSTNAMELEN, (char *)hmac, 16, false);
      ok = bstrcmp(bs->msg, host.c_str());
      if (!ok) {
         Dmsg2(debuglevel, "Authenticate NOT OK: wanted %s, got %s\n", host.c_str(), bs->msg);
      }
   }
   if (ok) {
      bs->fsend("1000 OK auth\n");
   } else {
      bs->fsend(_("1999 Authorization failed.\n"));
      bmicrosleep(5, 0);
   }
   return ok;
}

/* Respond to challenge from other end */
bool cram_md5_respond(BareosSocket *bs, const char *password, uint32_t *tls_remote_need, bool *compatible)
{
   PoolMem chal(PM_NAME);
   uint8_t hmac[20];

   *compatible = false;
   if (bs->recv() <= 0) {
      bmicrosleep(5, 0);
      return false;
   }

   Dmsg1(100, "cram-get received: %s", bs->msg);
   chal.check_size(bs->msglen);
   if (sscanf(bs->msg, "auth cram-md5c %s ssl=%d", chal.c_str(), tls_remote_need) == 2) {
      *compatible = true;
   } else if (sscanf(bs->msg, "auth cram-md5 %s ssl=%d", chal.c_str(), tls_remote_need) != 2) {
      if (sscanf(bs->msg, "auth cram-md5 %s\n", chal.c_str()) != 1) {
         Dmsg1(debuglevel, "Cannot scan challenge: %s", bs->msg);
         bs->fsend(_("1999 Authorization failed.\n"));
         bmicrosleep(5, 0);
         return false;
      }
   }

   hmac_md5((uint8_t *)chal.c_str(), strlen(chal.c_str()), (uint8_t *)password, strlen(password), hmac);
   bs->msglen = bin_to_base64(bs->msg, 50, (char *)hmac, 16, *compatible) + 1;
// Dmsg3(100, "get_auth: chal=%s pw=%s hmac=%s\n", chal.c_str(), password, bs->msg);
   if (!bs->send()) {
      Dmsg1(debuglevel, "Send challenge failed. ERR=%s\n", bs->bstrerror());
      return false;
   }
   Dmsg1(99, "sending resp to challenge: %s\n", bs->msg);
   if (bs->WaitData(180) <= 0 || bs->recv() <= 0) {
      Dmsg1(debuglevel, "Receive challenge response failed. ERR=%s\n", bs->bstrerror());
      bmicrosleep(5, 0);
      return false;
   }
   if (bstrcmp(bs->msg, "1000 OK auth\n")) {
      return true;
   }
   Dmsg1(debuglevel, "Received bad response: %s\n", bs->msg);
   bmicrosleep(5, 0);
   return false;
}
