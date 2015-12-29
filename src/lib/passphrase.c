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
 * passphrase.c Generate semi random ASCII passphrases
 *
 * Marco van Wieringen, March 2012
 */

#include "bareos.h"

#if defined(HAVE_OPENSSL) || defined(HAVE_GNUTLS)

#ifdef HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#endif

/*
 * Generate a semi random passphrase using normal ASCII chars
 * using the openssl rand_bytes() function of the length given
 * in the length argument. The returned argument should be
 * freed by the caller.
 */
char *generate_crypto_passphrase(uint16_t length)
{
#ifdef HAVE_GNUTLS
   int error;
#endif
   uint16_t vc_len, cnt, c;
   char *passphrase;
   unsigned char *rand_bytes;
   char valid_chars[] = "abcdefghijklmnopqrstuvwxyz"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "0123456789"
                        "!@#$%^&*()-_=+|[]{};:,.<>?/~";

   rand_bytes = (unsigned char *)malloc(length);
   passphrase = (char *)malloc(length);

#ifdef HAVE_OPENSSL
   if (RAND_bytes(rand_bytes, length) != 1) {
      unsigned long error;

      error = ERR_get_error();
      Emsg1(M_ERROR, 0,
            _("Failed to get random bytes from RAND_bytes for passphrase: ERR=%s\n"),
            ERR_lib_error_string(error));

      free(rand_bytes);
      free(passphrase);

      return NULL;
   }
#endif

#ifdef HAVE_GNUTLS
   error = gnutls_rnd(GNUTLS_RND_RANDOM, rand_bytes, length);
   if (error != GNUTLS_E_SUCCESS) {
      Emsg1(M_ERROR, 0,
            _("Failed to get random bytes from gnutls_rnd for passphrase: ERR=%s\n"),
            gnutls_strerror(error));

      free(rand_bytes);
      free(passphrase);

      return NULL;
   }
#endif

   /*
    * Convert the random bytes into a readable string.
    *
    * This conversion gives reasonable good passphrases with 32 bytes.
    * Tested was around 300k passphrases without one duplicate.
    */
   vc_len = strlen(valid_chars);
   for (cnt = 0; cnt < length; cnt++) {
      c = rand_bytes[cnt] % vc_len;
      passphrase[cnt] = valid_chars[c];
   }

   free(rand_bytes);

   return passphrase;
}
#else
/*
 * Generate a semi random passphrase using normal ASCII chars
 * using the srand and rand libc functions of the length given
 * in the length argument. The returned argument should be
 * freed by the caller.
 */
char *generate_crypto_passphrase(uint16_t length)
{
   uint16_t vc_len, cnt, c;
   int *rand_bytes;
   char *passphrase;
   char valid_chars[] = "abcdefghijklmnopqrstuvwxyz"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "0123456789"
                        "!@#$%^&*()-_=+|[]{};:,.<>?/~";

   rand_bytes = (int *)malloc(length * sizeof(int));
   passphrase = (char *)malloc(length);
   srand(time(NULL));
   for (cnt = 0; cnt < length; cnt++) {
      rand_bytes[cnt] = rand();
   }

   /*
    * Convert the random bytes into a readable string.
    * Due to the limited randomness of srand and rand this gives
    * more duplicates then the version using the openssl RAND_bytes
    * function. If available please use the openssl version.
    */
   vc_len = strlen(valid_chars);
   for (cnt = 0; cnt < length; cnt++) {
      c = rand_bytes[cnt] % vc_len;
      passphrase[cnt] = valid_chars[c];
   }

   free(rand_bytes);

   return passphrase;
}
#endif /* HAVE_OPENSSL || HAVE_GNUTLS*/
