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

bool CryptoSessionStart(JobControlRecord *jcr, crypto_cipher_t cipher);
void CryptoSessionEnd(JobControlRecord *jcr);
bool CryptoSessionSend(JobControlRecord *jcr, BareosSocket *sd);
bool VerifySignature(JobControlRecord *jcr, r_ctx &rctx);
bool FlushCipher(JobControlRecord *jcr, BareosWinFilePacket *bfd, uint64_t *addr, char *flags, int32_t stream,
                  RestoreCipherContext *cipher_ctx);
void DeallocateCipher(r_ctx &rctx);
void DeallocateForkCipher(r_ctx &rctx);
bool SetupEncryptionContext(b_ctx &bctx);
bool SetupDecryptionContext(r_ctx &rctx, RestoreCipherContext &rcctx);
bool EncryptData(b_ctx *bctx, bool *need_more_data);
bool DecryptData(JobControlRecord *jcr, char **data, uint32_t *length, RestoreCipherContext *cipher_ctx);

