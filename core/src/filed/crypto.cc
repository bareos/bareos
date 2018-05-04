/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, March MM
 * Extracted from other source files by Marco van Wieringen, June 2011
 */
/**
 * @file
 * Functions to handle cryptology
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "findlib/find_one.h"
#include "lib/edit.h"

#ifdef HAVE_SHA2
const bool have_sha2 = true;
#else
const bool have_sha2 = false;
#endif

static void unser_crypto_packet_len(RestoreCipherContext *ctx)
{
   unser_declare;
   if (ctx->packet_len == 0 && ctx->buf_len >= CRYPTO_LEN_SIZE) {
      UnserBegin(&ctx->buf[0], CRYPTO_LEN_SIZE);
      unser_uint32(ctx->packet_len);
      ctx->packet_len += CRYPTO_LEN_SIZE;
   }
}

bool CryptoSessionStart(JobControlRecord *jcr, crypto_cipher_t cipher)
{
   /**
    * Create encryption session data and a cached, DER-encoded session data
    * structure. We use a single session key for each backup, so we'll encode
    * the session data only once.
    */
   if (jcr->crypto.pki_encrypt) {
      uint32_t size = 0;

      /**
       * Create per-job session encryption context
       */
      jcr->crypto.pki_session = crypto_session_new(cipher, jcr->crypto.pki_recipients);
      if (!jcr->crypto.pki_session) {
         Jmsg(jcr, M_FATAL, 0, _("Cannot create a new crypto session probably unsupported cipher configured.\n"));
         return false;
      }

      /**
       * Get the session data size
       */
      if (!CryptoSessionEncode(jcr->crypto.pki_session, (uint8_t *)0, &size)) {
         Jmsg(jcr, M_FATAL, 0, _("An error occurred while encrypting the stream.\n"));
         return false;
      }

      /**
       * Allocate buffer
       */
      jcr->crypto.pki_session_encoded = GetMemory(size);

      /**
       * Encode session data
       */
      if (!CryptoSessionEncode(jcr->crypto.pki_session, (uint8_t *)jcr->crypto.pki_session_encoded, &size)) {
         Jmsg(jcr, M_FATAL, 0, _("An error occurred while encrypting the stream.\n"));
         return false;
      }

      /**
       * ... and store the encoded size
       */
      jcr->crypto.pki_session_encoded_size = size;

      /**
       * Allocate the encryption/decryption buffer
       */
      jcr->crypto.crypto_buf = GetMemory(CRYPTO_CIPHER_MAX_BLOCK_SIZE);
   }

   return true;
}

void CryptoSessionEnd(JobControlRecord *jcr)
{
   if (jcr->crypto.crypto_buf) {
      FreePoolMemory(jcr->crypto.crypto_buf);
      jcr->crypto.crypto_buf = NULL;
   }
   if (jcr->crypto.pki_session) {
      CryptoSessionFree(jcr->crypto.pki_session);
   }
   if (jcr->crypto.pki_session_encoded) {
      FreePoolMemory(jcr->crypto.pki_session_encoded);
      jcr->crypto.pki_session_encoded = NULL;
   }
}

bool CryptoSessionSend(JobControlRecord *jcr, BareosSocket *sd)
{
   POOLMEM *msgsave;

   /** Send our header */
   Dmsg2(100, "Send hdr fi=%ld stream=%d\n", jcr->JobFiles, STREAM_ENCRYPTED_SESSION_DATA);
   sd->fsend("%ld %d 0", jcr->JobFiles, STREAM_ENCRYPTED_SESSION_DATA);

   msgsave = sd->msg;
   sd->msg = jcr->crypto.pki_session_encoded;
   sd->msglen = jcr->crypto.pki_session_encoded_size;
   jcr->JobBytes += sd->msglen;

   Dmsg1(100, "Send data len=%d\n", sd->msglen);
   sd->send();
   sd->msg = msgsave;
   sd->signal(BNET_EOD);
   return true;
}

/**
 * Verify the signature for the last restored file
 * Return value is either true (signature correct)
 * or false (signature could not be verified).
 * TODO landonf: Implement without using FindOneFile and
 * without re-reading the file.
 */
bool VerifySignature(JobControlRecord *jcr, r_ctx &rctx)
{
   X509_KEYPAIR *keypair;
   DIGEST *digest = NULL;
   crypto_error_t err;
   uint64_t saved_bytes;
   crypto_digest_t signing_algorithm = have_sha2 ?
                                       CRYPTO_DIGEST_SHA256 : CRYPTO_DIGEST_SHA1;
   crypto_digest_t algorithm;
   SIGNATURE *sig = rctx.sig;

   if (!jcr->crypto.pki_sign) {
      /*
       * no signature OK
       */
      return true;
   }
   if (!sig) {
      if (rctx.type == FT_REGE || rctx.type == FT_REG || rctx.type == FT_RAW) {
         Jmsg1(jcr, M_ERROR, 0, _("Missing cryptographic signature for %s\n"),
               jcr->last_fname);
         goto bail_out;
      }
      return true;
   }

   /*
    * Iterate through the trusted signers
    */
   foreach_alist(keypair, jcr->crypto.pki_signers) {
      err = CryptoSignGetDigest(sig, jcr->crypto.pki_keypair, algorithm, &digest);
      switch (err) {
      case CRYPTO_ERROR_NONE:
         Dmsg0(50, "== Got digest\n");
         /*
          * We computed jcr->crypto.digest using signing_algorithm while writing
          * the file. If it is not the same as the algorithm used for
          * this file, punt by releasing the computed algorithm and
          * computing by re-reading the file.
          */
         if (algorithm != signing_algorithm) {
            if (jcr->crypto.digest) {
               CryptoDigestFree(jcr->crypto.digest);
               jcr->crypto.digest = NULL;
            }
         }
         if (jcr->crypto.digest) {
             /*
              * Use digest computed while writing the file to verify the signature
              */
            if ((err = CryptoSignVerify(sig, keypair, jcr->crypto.digest)) != CRYPTO_ERROR_NONE) {
               Dmsg1(50, "Bad signature on %s\n", jcr->last_fname);
               Jmsg2(jcr, M_ERROR, 0, _("Signature validation failed for file %s: ERR=%s\n"),
                     jcr->last_fname, crypto_strerror(err));
               goto bail_out;
            }
         } else {
            /*
             * Signature found, digest allocated.  Old method,
             * re-read the file and compute the digest
             */
            jcr->crypto.digest = digest;

            /*
             * Checksum the entire file
             * Make sure we don't modify JobBytes by saving and restoring it
             */
            saved_bytes = jcr->JobBytes;
            if (FindOneFile(jcr, jcr->ff, do_file_digest, jcr->last_fname, (dev_t)-1, 1) != 0) {
               Jmsg(jcr, M_ERROR, 0, _("Digest one file failed for file: %s\n"),
                    jcr->last_fname);
               jcr->JobBytes = saved_bytes;
               goto bail_out;
            }
            jcr->JobBytes = saved_bytes;

            /*
             * Verify the signature
             */
            if ((err = CryptoSignVerify(sig, keypair, digest)) != CRYPTO_ERROR_NONE) {
               Dmsg1(50, "Bad signature on %s\n", jcr->last_fname);
               Jmsg2(jcr, M_ERROR, 0, _("Signature validation failed for file %s: ERR=%s\n"),
                     jcr->last_fname, crypto_strerror(err));
               goto bail_out;
            }
            jcr->crypto.digest = NULL;
         }

         /*
          * Valid signature
          */
         Dmsg1(50, "Signature good on %s\n", jcr->last_fname);
         CryptoDigestFree(digest);
         return true;

      case CRYPTO_ERROR_NOSIGNER:
         /*
          * Signature not found, try again
          */
         if (digest) {
            CryptoDigestFree(digest);
            digest = NULL;
         }
         continue;
      default:
         /*
          * Something strange happened (that shouldn't happen!)...
          */
         Qmsg2(jcr, M_ERROR, 0, _("Signature validation failed for %s: %s\n"), jcr->last_fname, crypto_strerror(err));
         goto bail_out;
      }
   }

   /*
    * No signer
    */
   Dmsg1(50, "Could not find a valid public key for signature on %s\n", jcr->last_fname);

bail_out:
   if (digest) {
      CryptoDigestFree(digest);
   }
   return false;
}

/**
 * In the context of jcr, flush any remaining data from the cipher context,
 * writing it to bfd.
 * Return value is true on success, false on failure.
 */
bool FlushCipher(JobControlRecord *jcr, BareosWinFilePacket *bfd, uint64_t *addr, char *flags, int32_t stream, RestoreCipherContext *cipher_ctx)
{
   uint32_t decrypted_len = 0;
   char *wbuf;                        /* write buffer */
   uint32_t wsize;                    /* write size */
   char ec1[50];                      /* Buffer printing huge values */
   bool second_pass = false;

again:
   /*
    * Write out the remaining block and free the cipher context
    */
   cipher_ctx->buf = CheckPoolMemorySize(cipher_ctx->buf, cipher_ctx->buf_len +
                                            cipher_ctx->block_size);

   if (!CryptoCipherFinalize(cipher_ctx->cipher, (uint8_t *)&cipher_ctx->buf[cipher_ctx->buf_len],
        &decrypted_len)) {
      /*
       * Writing out the final, buffered block failed. Shouldn't happen.
       */
      Jmsg3(jcr, M_ERROR, 0, _("Decryption error. buf_len=%d decrypt_len=%d on file %s\n"),
            cipher_ctx->buf_len, decrypted_len, jcr->last_fname);
   }

   Dmsg2(130, "Flush decrypt len=%d buf_len=%d\n", decrypted_len, cipher_ctx->buf_len);
   /*
    * If nothing new was decrypted, and our output buffer is empty, return
    */
   if (decrypted_len == 0 && cipher_ctx->buf_len == 0) {
      return true;
   }

   cipher_ctx->buf_len += decrypted_len;

   unser_crypto_packet_len(cipher_ctx);
   Dmsg1(500, "Crypto unser block size=%d\n", cipher_ctx->packet_len - CRYPTO_LEN_SIZE);
   wsize = cipher_ctx->packet_len - CRYPTO_LEN_SIZE;
   /*
    * Decrypted, possibly decompressed output here.
    */
   wbuf = &cipher_ctx->buf[CRYPTO_LEN_SIZE];
   cipher_ctx->buf_len -= cipher_ctx->packet_len;
   Dmsg2(130, "Encryption writing full block, %u bytes, remaining %u bytes in buffer\n", wsize, cipher_ctx->buf_len);

   if (BitIsSet(FO_SPARSE, flags) || BitIsSet(FO_OFFSETS, flags)) {
      if (!SparseData(jcr, bfd, addr, &wbuf, &wsize)) {
         return false;
      }
   }

   if (BitIsSet(FO_COMPRESS, flags)) {
      if (!DecompressData(jcr, jcr->last_fname, stream, &wbuf, &wsize, false)) {
         return false;
      }
   }

   Dmsg0(130, "Call StoreData\n");
   if (!StoreData(jcr, bfd, wbuf, wsize, BitIsSet(FO_WIN32DECOMP, flags))) {
      return false;
   }
   jcr->JobBytes += wsize;
   Dmsg2(130, "Flush write %u bytes, JobBytes=%s\n", wsize, edit_uint64(jcr->JobBytes, ec1));

   /*
    * Move any remaining data to start of buffer
    */
   if (cipher_ctx->buf_len > 0) {
      Dmsg1(130, "Moving %u buffered bytes to start of buffer\n", cipher_ctx->buf_len);
      memmove(cipher_ctx->buf, &cipher_ctx->buf[cipher_ctx->packet_len],
         cipher_ctx->buf_len);
   }
   /*
    * The packet was successfully written, reset the length so that the next
    * packet length may be re-read by unser_crypto_packet_len()
    */
   cipher_ctx->packet_len = 0;

   if (cipher_ctx->buf_len >0 && !second_pass) {
      second_pass = true;
      goto again;
   }

   /*
    * Stop decryption
    */
   cipher_ctx->buf_len = 0;
   cipher_ctx->packet_len = 0;

   return true;
}

void DeallocateCipher(r_ctx &rctx)
{
   /*
    * Flush and deallocate previous stream's cipher context
    */
   if (rctx.cipher_ctx.cipher) {
      FlushCipher(rctx.jcr, &rctx.bfd, &rctx.fileAddr, rctx.flags, rctx.comp_stream, &rctx.cipher_ctx);
      CryptoCipherFree(rctx.cipher_ctx.cipher);
      rctx.cipher_ctx.cipher = NULL;
   }
}

void DeallocateForkCipher(r_ctx &rctx)
{
   /*
    * Flush and deallocate previous stream's fork cipher context
    */
   if (rctx.fork_cipher_ctx.cipher) {
      FlushCipher(rctx.jcr, &rctx.forkbfd, &rctx.fork_addr, rctx.fork_flags, rctx.comp_stream, &rctx.fork_cipher_ctx);
      CryptoCipherFree(rctx.fork_cipher_ctx.cipher);
      rctx.fork_cipher_ctx.cipher = NULL;
   }
}

/**
 * Setup a encryption context
 */
bool SetupEncryptionContext(b_ctx &bctx)
{
   uint32_t cipher_block_size;
   bool retval = false;

   if (BitIsSet(FO_ENCRYPT, bctx.ff_pkt->flags)) {
      if (BitIsSet(FO_SPARSE, bctx.ff_pkt->flags) ||
          BitIsSet(FO_OFFSETS, bctx.ff_pkt->flags)) {
         Jmsg0(bctx.jcr, M_FATAL, 0, _("Encrypting sparse or offset data not supported.\n"));
         goto bail_out;
      }
      /*
       * Allocate the cipher context
       */
      if ((bctx.cipher_ctx = crypto_cipher_new(bctx.jcr->crypto.pki_session, true,
                                               &cipher_block_size)) == NULL) {
         /*
          * Shouldn't happen!
          */
         Jmsg0(bctx.jcr, M_FATAL, 0, _("Failed to initialize encryption context.\n"));
         goto bail_out;
      }

      /*
       * Grow the crypto buffer, if necessary.
       * CryptoCipherUpdate() will buffer up to (cipher_block_size - 1).
       * We grow crypto_buf to the maximum number of blocks that
       * could be returned for the given read buffer size.
       * (Using the larger of either rsize or max_compress_len)
       */
      bctx.jcr->crypto.crypto_buf = CheckPoolMemorySize(bctx.jcr->crypto.crypto_buf,
           (MAX(bctx.jcr->buf_size + (int)sizeof(uint32_t), (int32_t)bctx.max_compress_len) +
            cipher_block_size - 1) / cipher_block_size * cipher_block_size);

      bctx.wbuf = bctx.jcr->crypto.crypto_buf; /* Encrypted, possibly compressed output here. */
   }

   retval = true;

bail_out:
   return retval;
}

/**
 * Setup a decryption context
 */
bool SetupDecryptionContext(r_ctx &rctx, RestoreCipherContext &rcctx)
{
   if (!rctx.cs) {
      Jmsg1(rctx.jcr, M_ERROR, 0, _("Missing encryption session data stream for %s\n"), rctx.jcr->last_fname);
      return false;
   }

   if ((rcctx.cipher = crypto_cipher_new(rctx.cs, false, &rcctx.block_size)) == NULL) {
      Jmsg1(rctx.jcr, M_ERROR, 0, _("Failed to initialize decryption context for %s\n"), rctx.jcr->last_fname);
      FreeSession(rctx);
      return false;
   }

   return true;
}

bool EncryptData(b_ctx *bctx, bool *need_more_data)
{
   bool retval = false;
   uint32_t initial_len = 0;

   /*
    * Note, here we prepend the current record length to the beginning
    *  of the encrypted data. This is because both sparse and compression
    *  restore handling want records returned to them with exactly the
    *  same number of bytes that were processed in the backup handling.
    *  That is, both are block filters rather than a stream.  When doing
    *  compression, the compression routines may buffer data, so that for
    *  any one record compressed, when it is decompressed the same size
    *  will not be obtained. Of course, the buffered data eventually comes
    *  out in subsequent CryptoCipherUpdate() calls or at least
    *  when CryptoCipherFinalize() is called.  Unfortunately, this
    *  "feature" of encryption enormously complicates the restore code.
    */
   ser_declare;

   if (BitIsSet(FO_SPARSE, bctx->ff_pkt->flags) ||
       BitIsSet(FO_OFFSETS, bctx->ff_pkt->flags)) {
         bctx->cipher_input_len += OFFSET_FADDR_SIZE;
   }

   /*
    * Encrypt the length of the input block
    */
   uint8_t packet_len[sizeof(uint32_t)];

   SerBegin(packet_len, sizeof(uint32_t));
   ser_uint32(bctx->cipher_input_len); /* store data len in begin of buffer */
   Dmsg1(20, "Encrypt len=%d\n", bctx->cipher_input_len);

   if (!CryptoCipherUpdate(bctx->cipher_ctx, packet_len, sizeof(packet_len),
                             (uint8_t *)bctx->jcr->crypto.crypto_buf, &initial_len)) {
      /*
       * Encryption failed. Shouldn't happen.
       */
      Jmsg(bctx->jcr, M_FATAL, 0, _("Encryption error\n"));
      goto bail_out;
   }

   /*
    * Encrypt the input block
    */
   if (CryptoCipherUpdate(bctx->cipher_ctx, bctx->cipher_input, bctx->cipher_input_len,
                            (uint8_t *)&bctx->jcr->crypto.crypto_buf[initial_len],
                            &bctx->encrypted_len)) {
      if ((initial_len + bctx->encrypted_len) == 0) {
         /*
          * No full block of data available, read more data
          */
         *need_more_data = true;
         goto bail_out;
      }

      Dmsg2(400, "encrypted len=%d unencrypted len=%d\n",
            bctx->encrypted_len, bctx->jcr->store_bsock->msglen);

      bctx->jcr->store_bsock->msglen = initial_len + bctx->encrypted_len; /* set encrypted length */
   } else {
      /*
       * Encryption failed. Shouldn't happen.
       */
      Jmsg(bctx->jcr, M_FATAL, 0, _("Encryption error\n"));
      goto bail_out;
   }

   retval = true;

bail_out:
   return retval;
}

bool DecryptData(JobControlRecord *jcr, char **data, uint32_t *length, RestoreCipherContext *cipher_ctx)
{
   uint32_t decrypted_len = 0; /* Decryption output length */

   ASSERT(cipher_ctx->cipher);

   /*
    * NOTE: We must implement block preserving semantics for the
    * non-streaming compression and sparse code.
    *
    * Grow the crypto buffer, if necessary.
    * CryptoCipherUpdate() will process only whole blocks,
    * buffering the remaining input.
    */
   cipher_ctx->buf = CheckPoolMemorySize(cipher_ctx->buf,
                     cipher_ctx->buf_len + *length + cipher_ctx->block_size);

   /*
    * Decrypt the input block
    */
   if (!CryptoCipherUpdate(cipher_ctx->cipher,
                             (const u_int8_t *)*data,
                             *length,
                             (u_int8_t *)&cipher_ctx->buf[cipher_ctx->buf_len],
                             &decrypted_len)) {
      /*
       * Decryption failed. Shouldn't happen.
       */
      Jmsg(jcr, M_FATAL, 0, _("Decryption error\n"));
      goto bail_out;
   }

   if (decrypted_len == 0) {
      /*
       * No full block of encrypted data available, write more data
       */
      *length = 0;
      return true;
   }

   Dmsg2(200, "decrypted len=%d encrypted len=%d\n", decrypted_len, *length);

   cipher_ctx->buf_len += decrypted_len;
   *data = cipher_ctx->buf;

   /*
    * If one full preserved block is available, write it to disk,
    * and then buffer any remaining data. This should be effecient
    * as long as Bareos's block size is not significantly smaller than the
    * encryption block size (extremely unlikely!)
    */
   unser_crypto_packet_len(cipher_ctx);
   Dmsg1(500, "Crypto unser block size=%d\n", cipher_ctx->packet_len - CRYPTO_LEN_SIZE);

   if (cipher_ctx->packet_len == 0 || cipher_ctx->buf_len < cipher_ctx->packet_len) {
      /*
       * No full preserved block is available.
       */
      *length = 0;
      return true;
   }

   /*
    * We have one full block, set up the filter input buffers
    */
   *length = cipher_ctx->packet_len - CRYPTO_LEN_SIZE;
   *data = &((*data)[CRYPTO_LEN_SIZE]); /* Skip the block length header */
   cipher_ctx->buf_len -= cipher_ctx->packet_len;
   Dmsg2(130, "Encryption writing full block, %u bytes, remaining %u bytes in buffer\n", *length, cipher_ctx->buf_len);

   return true;

bail_out:
   return false;
}
