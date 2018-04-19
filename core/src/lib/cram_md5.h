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
#ifndef LIB_CRAM_MD5_H_
#define LIB_CRAM_MD5_H_

DLL_IMP_EXP bool cram_md5_respond(BareosSocket *bs, const char *password, uint32_t *remote_tls_policy, bool *compatible);
DLL_IMP_EXP bool cram_md5_challenge(BareosSocket *bs, const char *password, uint32_t local_tls_policy, bool compatible);
DLL_IMP_EXP void hmac_md5(uint8_t *text, int text_len, uint8_t *key, int key_len, uint8_t *hmac);

#endif // LIB_CRAM_MD5_H_
