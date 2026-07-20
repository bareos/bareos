/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#include "lib/crypto_wrap.h"
#include "lib/passphrase.h"
#include "cats/cats.h"
#include "dird_conf.h"
#include "dird_globals.h"
#include "ua.h"

#include "volume_key.h"
namespace directordaemon {
/*
 * Generate a new encryption key for use in volume encryption.
 * We don't ask the user for a encryption key but generate a semi
 * random passphrase of the wanted length which is way stronger.
 * When the config has a wrap key we use that to wrap the newly
 * created passphrase using RFC3394 aes wrap and always convert
 * the passphrase into a base64 encoded string. This key is
 * stored in the database and is passed to the storage daemon
 * when needed. The storage daemon has the same wrap key per
 * director so it can unwrap the passphrase for use.
 *
 * Changing the wrap key will render any previously created
 * crypto keys useless so only change the wrap key after initial
 * setting when you know what you are doing and always store
 * the old key somewhere save so you can use bscrypto to
 * convert them for the new wrap key.
 */

bool GenerateNewEncryptionKey(UaContext* ua, MediaDbRecord* mr)
{
  int length;
  char* passphrase;

  passphrase = generate_crypto_passphrase(DEFAULT_PASSPHRASE_LENGTH);
  if (!passphrase) {
    ua->ErrorMsg(
        T_("Failed to generate new encryption passphrase for Volume %s.\n"),
        mr->VolumeName);
    return false;
  }

  // See if we need to wrap the passphrase.
  if (me->keyencrkey.value) {
    char* wrapped_passphrase;

    length = DEFAULT_PASSPHRASE_LENGTH + 8;
    wrapped_passphrase = (char*)malloc(length);
    memset(wrapped_passphrase, 0, length);
    if (auto error = AesWrap(
            (unsigned char*)me->keyencrkey.value, DEFAULT_PASSPHRASE_LENGTH / 8,
            (unsigned char*)passphrase, (unsigned char*)wrapped_passphrase)) {
      ua->ErrorMsg(T_("Failed to wrap passphrase ERR=%s.\n"), error->c_str());
      free(passphrase);
      free(wrapped_passphrase);
      return false;
    }

    free(passphrase);
    passphrase = wrapped_passphrase;
  } else {
    length = DEFAULT_PASSPHRASE_LENGTH;
  }

  // The passphrase is always base64 encoded.
  BinToBase64(mr->EncrKey, sizeof(mr->EncrKey), passphrase, length, true);

  free(passphrase);
  return true;
}
}  // namespace directordaemon
