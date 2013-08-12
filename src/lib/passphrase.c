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

#ifdef HAVE_CRYPTO /* Is encryption enabled? */
#ifdef HAVE_OPENSSL /* How about OpenSSL? */

#include <openssl/err.h>
#include <openssl/rand.h>

/*
 * Generate a semi random passphrase using normal ASCII chars
 * using the openssl rand_bytes() function of the length given
 * in the length argument. The returned argument should be
 * freed by the caller.
 */
char *generate_crypto_passphrase(int length)
{
   int c;
   int vc_len, cnt;
   char *passphrase;
   unsigned char *rand_bytes;
   char valid_chars[] = "abcdefghijklmnopqrstuvwxyz"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "0123456789"
                        "!@#$%^&*()-_=+|[]{};:,.<>?/~";

   rand_bytes = (unsigned char *)malloc(length);
   passphrase = (char *)malloc(length);
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

#else /* HAVE_OPENSSL */
#error No encryption library available
#endif /* HAVE_OPENSSL */

#else /* HAVE_CRYPTO */

/*
 * Generate a semi random passphrase using normal ASCII chars
 * using the srand and rand libc functions of the length given
 * in the length argument. The returned argument should be
 * freed by the caller.
 */
char *generate_crypto_passphrase(int length)
{
   char c;
   int vc_len, cnt;
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
#endif /* HAVE_CRYPTO */
