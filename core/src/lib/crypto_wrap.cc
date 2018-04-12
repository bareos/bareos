/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * crypto_wrap.c Encryption key wrapping support functions
 *
 * crypto_wrap.c was based on sample code used in multiple
 * other projects and has the following copyright:
 *
 * - AES Key Wrap Algorithm (128-bit KEK) (RFC3394)
 *
 * Copyright (c) 2003-2004, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * Adapted to BAREOS by Marco van Wieringen, March 2012
 */

#include "bareos.h"

#if defined(HAVE_OPENSSL) || defined(HAVE_GNUTLS)

#ifdef HAVE_OPENSSL
#include <openssl/aes.h>
#endif

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#endif

/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @plain: plaintext key to be wrapped, n * 64 bit
 * @cipher: wrapped key, (n + 1) * 64 bit
 */
void aes_wrap(uint8_t *kek, int n, uint8_t *plain, uint8_t *cipher)
{
   uint8_t *a, *r, b[16];
   int i, j;
#ifdef HAVE_OPENSSL
   AES_KEY key;
#endif
#ifdef HAVE_GNUTLS
   gnutls_cipher_hd_t key;
   gnutls_datum_t key_data;
#endif

   a = cipher;
   r = cipher + 8;

   /*
    * 1) Initialize variables.
    */
   memset(a, 0xa6, 8);
   memcpy(r, plain, 8 * n);

#ifdef HAVE_OPENSSL
   AES_set_encrypt_key(kek, 128, &key);
#endif
#ifdef HAVE_GNUTLS
   key_data.data = kek;
   key_data.size = strlen((char *)kek);
   gnutls_cipher_init(&key, GNUTLS_CIPHER_AES_128_CBC, &key_data, NULL);
#endif

   /*
    * 2) Calculate intermediate values.
    * For j = 0 to 5
    *     For i=1 to n
    *      B = AES(K, A | R[i])
    *      A = MSB(64, B) ^ t where t = (n*j)+i
    *      R[i] = LSB(64, B)
    */
   for (j = 0; j <= 5; j++) {
      r = cipher + 8;
      for (i = 1; i <= n; i++) {
         memcpy(b, a, 8);
         memcpy(b + 8, r, 8);
#ifdef HAVE_OPENSSL
         AES_encrypt(b, b, &key);
#endif
#ifdef HAVE_GNUTLS
         gnutls_cipher_encrypt(key, b, sizeof(b));
#endif
         memcpy(a, b, 8);
         a[7] ^= n * j + i;
         memcpy(r, b + 8, 8);
         r += 8;
      }
   }

   /* 3) Output the results.
    *
    * These are already in @cipher due to the location of temporary
    * variables.
    */
#ifdef HAVE_GNUTLS
   gnutls_cipher_deinit(key);
#endif
}

/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @cipher: wrapped key to be unwrapped, (n + 1) * 64 bit
 * @plain: plaintext key, n * 64 bit
 */
int aes_unwrap(uint8_t *kek, int n, uint8_t *cipher, uint8_t *plain)
{
   uint8_t a[8], *r, b[16];
   int i, j;
#ifdef HAVE_OPENSSL
   AES_KEY key;
#endif
#ifdef HAVE_GNUTLS
   gnutls_cipher_hd_t key;
   gnutls_datum_t key_data;
#endif

   /*
    * 1) Initialize variables.
    */
   memcpy(a, cipher, 8);
   r = plain;
   memcpy(r, cipher + 8, 8 * n);

#ifdef HAVE_OPENSSL
   AES_set_decrypt_key(kek, 128, &key);
#endif
#ifdef HAVE_GNUTLS
   key_data.data = kek;
   key_data.size = strlen((char *)kek);
   gnutls_cipher_init(&key, GNUTLS_CIPHER_AES_128_CBC, &key_data, NULL);
#endif

   /*
    * 2) Compute intermediate values.
    * For j = 5 to 0
    *     For i = n to 1
    *      B = AES-1(K, (A ^ t) | R[i]) where t = n*j+i
    *      A = MSB(64, B)
    *      R[i] = LSB(64, B)
    */
   for (j = 5; j >= 0; j--) {
      r = plain + (n - 1) * 8;
      for (i = n; i >= 1; i--) {
         memcpy(b, a, 8);
         b[7] ^= n * j + i;

         memcpy(b + 8, r, 8);
#ifdef HAVE_OPENSSL
         AES_decrypt(b, b, &key);
#endif
#ifdef HAVE_GNUTLS
         gnutls_cipher_decrypt(key, b, sizeof(b));
#endif
         memcpy(a, b, 8);
         memcpy(r, b + 8, 8);
         r -= 8;
      }
   }

   /*
    * 3) Output results.
    *
    * These are already in @plain due to the location of temporary
    * variables. Just verify that the IV matches with the expected value.
    */
   for (i = 0; i < 8; i++) {
      if (a[i] != 0xa6) {
         return -1;
      }
   }

#ifdef HAVE_GNUTLS
   gnutls_cipher_deinit(key);
#endif

   return 0;
}
#else
/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @plain: plaintext key to be wrapped, n * 64 bit
 * @cipher: wrapped key, (n + 1) * 64 bit
 */
void aes_wrap(uint8_t *kek, int n, uint8_t *plain, uint8_t *cipher)
{
   memcpy(cipher, plain, n * 8);
}

/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @cipher: wrapped key to be unwrapped, (n + 1) * 64 bit
 * @plain: plaintext key, n * 64 bit
 */
int aes_unwrap(uint8_t *kek, int n, uint8_t *cipher, uint8_t *plain)
{
   memcpy(cipher, plain, n * 8);
   return 0;
}
#endif /* HAVE_OPENSSL || HAVE_GNUTLS */
