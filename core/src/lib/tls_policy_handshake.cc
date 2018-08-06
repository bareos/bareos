/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

static bool TlsReceivePolicy(BareosSocket *bs, uint32_t *tls_remote_policy)
{
   if (bs->recv() <= 0) {
      Bmicrosleep(5, 0);
      return false;
   }
   int n = sscanf(bs->msg, "ssl=%d", tls_remote_policy);
   Dmsg1(100, "ssl received: %s", bs->msg);
   return n==1;
}

static bool TlsSendPolicy(BareosSocket *bs, uint32_t tls_local_policy)
{
   Dmsg1(100, "send: ssl=%d\n", tls_local_policy);
   if (!bs->fsend("ssl=%d\n", tls_local_policy)) {
      Dmsg1(100, "Bnet send tls need. ERR=%s\n", bs->bstrerror());
      return false;
   }
   return true;
}

bool TlsOpenSsl::TlsPolicyHandshake(BareosSocket *bs, bool initiated_by_remote,
                          uint32_t local, uint32_t *remote)
{
   if (initiated_by_remote) {
      if (TlsSendPolicy(bs, local)) {
         if (TlsReceivePolicy(bs, remote)) {
            return true;
         }
      }
   } else {
      if (TlsReceivePolicy(bs, remote)) {
         if (TlsSendPolicy(bs, local)) {
            return true;
         }
      }
   }
   return false;
}
