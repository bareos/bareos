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

bool crypto_session_start(JobControlRecord *jcr, crypto_cipher_t cipher);
void crypto_session_end(JobControlRecord *jcr);
bool crypto_session_send(JobControlRecord *jcr, BareosSocket *sd);
bool verify_signature(JobControlRecord *jcr, r_ctx &rctx);
bool flush_cipher(JobControlRecord *jcr, BareosWinFilePacket *bfd, uint64_t *addr, char *flags, int32_t stream,
                  RestoreCipherContext *cipher_ctx);
void deallocate_cipher(r_ctx &rctx);
void deallocate_fork_cipher(r_ctx &rctx);
bool setup_encryption_context(b_ctx &bctx);
bool setup_decryption_context(r_ctx &rctx, RestoreCipherContext &rcctx);
bool encrypt_data(b_ctx *bctx, bool *need_more_data);
bool decrypt_data(JobControlRecord *jcr, char **data, uint32_t *length, RestoreCipherContext *cipher_ctx);

