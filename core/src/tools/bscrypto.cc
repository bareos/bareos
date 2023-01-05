/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2012-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
// Marco van Wieringen, March 2012
/**
 * @file
 * Load and clear an SCSI encryption key used by a tape drive
 * using a lowlevel SCSI interface.
 */

#include <unistd.h>
#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "lib/cli.h"
#include "lib/crypto_cache.h"
#include "lib/crypto_wrap.h"
#include "lib/passphrase.h"
#include "lib/scsi_crypto.h"
#include "lib/base64.h"

/**
 * Read the key bits from the keyfile.
 * - == stdin
 */
static void ReadKeyBits(const std::string& keyfile,
                        char* data,
                        size_t sizeof_data)
{
  int kfd = 0;
  if (bstrcmp(keyfile.c_str(), "-")) {
    kfd = 0;
    fprintf(stdout, T_("Enter Encryption Key (close with ^D): "));
    fflush(stdout);
  } else {
    kfd = open(keyfile.c_str(), O_RDONLY);
    if (kfd < 0) {
      fprintf(stderr, T_("Cannot open keyfile %s\n"), keyfile.c_str());
      exit(1);
    }
  }
  Dmsg1(5, "data size = %d\n", sizeof_data);
  if (read(kfd, data, sizeof_data) == 0) {
    fprintf(stderr, T_("Cannot read from keyfile %s\n"), keyfile.c_str());
    exit(1);
  }
  if (kfd > 0) { close(kfd); }
  StripTrailingJunk(data);
  Dmsg1(10, "Key data = %s\n", data);
}

int main(int argc, char* const* argv)
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  CLI::App bscrypto_app;
  InitCLIApp(bscrypto_app, "The Bareos encryption tool.");

  bool base64_transform = false;
  bscrypto_app.add_flag("-b,--base64", base64_transform,
                        "Perform base64 encoding of keydata.");

  bool clear_encryption = false;
  auto option_clear_encryption = bscrypto_app.add_flag(
      "-c,--clear-encryption", clear_encryption, "Clear encryption key.");

  bool dump_cache = false;
  std::string cache_file{};
  auto option_dump_cache
      = bscrypto_app
            .add_option(
                "-D,--dump-cache",
                [&dump_cache, &cache_file](std::vector<std::string> val) {
                  dump_cache = true;
                  cache_file = val[0];
                  return true;
                },
                "Dump content of given cachefile.")
            ->type_name("<cachefile>")
            ->excludes(option_clear_encryption);

  AddDebugOptions(bscrypto_app);

  bool drive_encryption_status = false;
  auto option_drive_encryption_status
      = bscrypto_app
            .add_flag("-e,--drive-encryption-status", drive_encryption_status,
                      "Show drive encryption status.")
            ->excludes(option_clear_encryption)
            ->excludes(option_dump_cache);

  bool generate_passphrase = false;
  std::string keyfile{};
  auto option_generate_pass
      = bscrypto_app
            .add_option(
                "-g,--generate-passphrase",
                [&generate_passphrase, &keyfile](std::vector<std::string> val) {
                  generate_passphrase = true;
                  keyfile = val[0];
                  return true;
                },
                "Generate new encryption passphrase in keyfile.")
            ->type_name("<keyfile>")
            ->excludes(option_clear_encryption)
            ->excludes(option_drive_encryption_status);

  bool show_keydata = false;
  auto option_show_keyfile
      = bscrypto_app
            .add_option(
                "-k,--show-keyfile",
                [&show_keydata, &keyfile](std::vector<std::string> val) {
                  show_keydata = true;
                  keyfile = val[0];
                  return true;
                },
                "Show content of keyfile.")
            ->type_name("<keyfile>")
            ->excludes(option_generate_pass)
            ->excludes(option_clear_encryption)
            ->excludes(option_drive_encryption_status);

  bool populate_cache = false;
  auto option_populate_cache
      = bscrypto_app
            .add_option(
                "-p,--populate-cachefile",
                [&populate_cache, &cache_file](std::vector<std::string> val) {
                  populate_cache = true;
                  cache_file = val[0];
                  return true;
                },
                "Populate given cachefile with crypto keys.")
            ->type_name("<cachefile>")
            ->excludes(option_clear_encryption)
            ->excludes(option_drive_encryption_status);

  bool reset_cache = false;
  auto option_reset_cache
      = bscrypto_app
            .add_option(
                "-r,--reset-expiry-time",
                [&reset_cache, &cache_file](std::vector<std::string> val) {
                  reset_cache = true;
                  cache_file = val[0];
                  return true;
                },
                "Reset expiry time for entries of given cachefile.")
            ->type_name("<cachefile>")
            ->excludes(option_clear_encryption)
            ->excludes(option_drive_encryption_status);

  bool set_encryption = false;
  auto option_set_encryption
      = bscrypto_app
            .add_option(
                "-s,--set-encryption-key",
                [&set_encryption, &keyfile](std::vector<std::string> val) {
                  set_encryption = true;
                  keyfile = val[0];
                  return true;
                },
                "Set encryption key loaded from keyfile.")
            ->type_name("<keyfile>")
            ->excludes(option_show_keyfile)
            ->excludes(option_clear_encryption)
            ->excludes(option_reset_cache)
            ->excludes(option_dump_cache)
            ->excludes(option_drive_encryption_status)
            ->excludes(option_generate_pass);

  bool volume_encryption_status = false;
  bscrypto_app
      .add_flag("-v,--volume-encryption-status", volume_encryption_status,
                "Show volume encryption status.")
      ->excludes(option_clear_encryption)
      ->excludes(option_set_encryption)
      ->excludes(option_generate_pass)
      ->excludes(option_show_keyfile)
      ->excludes(option_dump_cache)
      ->excludes(option_populate_cache)
      ->excludes(option_reset_cache);

  bool wrapped_keys = false;
  std::string wrap_keyfile{};
  bscrypto_app
      .add_option(
          "-w,--wrap-unwrap-key",
          [&wrapped_keys, &wrap_keyfile](std::vector<std::string> val) {
            wrapped_keys = true;
            wrap_keyfile = val[0];
            return true;
          },
          "Wrap/Unwrap the key using RFC3394 aes-(un)wrap using the key in "
          "keyfile as a Key Encryption.")
      ->type_name("<keyfile>")
      ->excludes(option_show_keyfile);

  std::string device_name{};
  bscrypto_app.add_option("device name", device_name, "Name of device.");

  CLI11_PARSE(bscrypto_app, argc, argv);

  if (!generate_passphrase && !show_keydata && !dump_cache && !populate_cache
      && !reset_cache && device_name.empty()) {
    fprintf(stderr, T_("Missing device_name argument for this option\n"));
    exit(1);
  }

  if (generate_passphrase && show_keydata) {
    fprintf(stderr, T_("Either use -g or -k not both\n"));
    exit(1);
  }

  if (clear_encryption && set_encryption) {
    fprintf(stderr, T_("Either use -c or -s not both\n"));
    exit(1);
  }

  if ((clear_encryption || set_encryption)
      && (drive_encryption_status || volume_encryption_status)) {
    fprintf(
        stderr,
        T_("Either set or clear the crypto key or ask for status not both\n"));
    exit(1);
  }

  if ((clear_encryption || set_encryption || drive_encryption_status
       || volume_encryption_status)
      && (generate_passphrase || show_keydata || dump_cache || populate_cache
          || reset_cache)) {
    fprintf(stderr, T_("Don't mix operations which are incompatible "
                       "e.g. generate/show vs set/clear etc.\n"));
    exit(1);
  }

  OSDependentInit();

  if (dump_cache) {
    // Load any keys currently in the cache.
    ReadCryptoCache(cache_file.c_str());

    // Dump the content of the cache.
    DumpCryptoCache(1);

    FlushCryptoCache();
    exit(0);
  }

  if (populate_cache) {
    char *VolumeName, *EncrKey;
    char new_cache_entry[256];

    // Load any keys currently in the cache.
    ReadCryptoCache(cache_file.c_str());

    /* Read new entries from stdin and parse them to update
     * the cache. */
    fprintf(stdout, T_("Enter cache entrie(s) (close with ^D): "));
    fflush(stdout);

    memset(new_cache_entry, 0, sizeof(new_cache_entry));
    while (read(1, new_cache_entry, sizeof(new_cache_entry)) > 0) {
      StripTrailingJunk(new_cache_entry);

      // Try to parse the entry.
      VolumeName = new_cache_entry;
      EncrKey = strchr(new_cache_entry, '\t');
      if (!EncrKey) { break; }

      *EncrKey++ = '\0';
      UpdateCryptoCache(VolumeName, EncrKey);
      memset(new_cache_entry, 0, sizeof(new_cache_entry));
    }

    // Write out the new cache entries.
    WriteCryptoCache(cache_file.c_str());

    FlushCryptoCache();
    exit(0);
  }

  if (reset_cache) {
    // Load any keys currently in the cache.
    ReadCryptoCache(cache_file.c_str());

    // Reset all entries.
    ResetCryptoCache();

    // Write out the new cache entries.
    WriteCryptoCache(cache_file.c_str());

    FlushCryptoCache();
    exit(0);
  }


  char keydata[64];
  char wrapdata[64];
  memset(keydata, 0, sizeof(keydata));
  memset(wrapdata, 0, sizeof(wrapdata));

  if (wrapped_keys) { ReadKeyBits(wrap_keyfile, wrapdata, sizeof(wrapdata)); }

  /* Generate a new passphrase allow it to be wrapped using the given wrapkey
   * and base64 if specified or when wrapped. */
  int length;
  if (generate_passphrase) {
    int cnt;
    char* passphrase;

    passphrase = generate_crypto_passphrase(DEFAULT_PASSPHRASE_LENGTH);
    if (!passphrase) { exit(1); }

    Dmsg1(10, T_("Generated passphrase = %s\n"), passphrase);

    // See if we need to wrap the passphrase.
    if (wrapped_keys) {
      char* wrapped_passphrase;

      length = DEFAULT_PASSPHRASE_LENGTH + 8;
      wrapped_passphrase = (char*)malloc(length);
      memset(wrapped_passphrase, 0, length);
      if (auto error = AesWrap(
              (unsigned char*)wrapdata, DEFAULT_PASSPHRASE_LENGTH / 8,
              (unsigned char*)passphrase, (unsigned char*)wrapped_passphrase)) {
        fprintf(stderr, T_("Cannot wrap passphrase ERR=%s\n"), error->c_str());
        free(passphrase);
        exit(1);
      }

      free(passphrase);
      passphrase = wrapped_passphrase;
    } else {
      length = DEFAULT_PASSPHRASE_LENGTH;
    }

    /* See where to write the key.
     * - == stdout */
    int kfd;
    if (bstrcmp(keyfile.c_str(), "-")) {
      kfd = 1;
    } else {
      kfd = open(keyfile.c_str(), O_WRONLY | O_CREAT, 0644);
      if (kfd < 0) {
        fprintf(stderr, T_("Cannot open keyfile %s\n"), keyfile.c_str());
        free(passphrase);
        exit(1);
      }
    }

    if (base64_transform || wrapped_keys) {
      cnt = BinToBase64(keydata, sizeof(keydata), passphrase, length, true);
      if (write(kfd, keydata, cnt) != cnt) {
        fprintf(stderr, T_("Failed to write %d bytes to keyfile %s\n"), cnt,
                keyfile.c_str());
      }
    } else {
      cnt = DEFAULT_PASSPHRASE_LENGTH;
      if (write(kfd, passphrase, cnt) != cnt) {
        fprintf(stderr, T_("Failed to write %d bytes to keyfile %s\n"), cnt,
                keyfile.c_str());
      }
    }

    Dmsg1(10, "Keydata = %s\n", keydata);

    if (kfd > 1) {
      close(kfd);
    } else {
      if (write(kfd, "\n", 1) == 0) {
        free(passphrase);
        exit(0);
      }
    }
    free(passphrase);
    exit(0);
  }

  if (show_keydata) {
    ReadKeyBits(keyfile, keydata, sizeof(keydata));
    char* passphrase;

    // See if we need to unwrap the passphrase.
    if (wrapped_keys) {
      char* wrapped_passphrase;
      /* A wrapped key is base64 encoded after it was wrapped so first
       * convert it from base64 to bin. As we first go from base64 to bin
       * and the Base64ToBin has a check if the decoded string will fit
       * we need to alocate some more bytes for the decoded buffer to be
       * sure it will fit. */
      length = DEFAULT_PASSPHRASE_LENGTH + 12;
      wrapped_passphrase = (char*)malloc(length);
      memset(wrapped_passphrase, 0, length);
      if (Base64ToBin(wrapped_passphrase, length, keydata, strlen(keydata))
          == 0) {
        fprintf(stderr,
                T_("Failed to base64 decode the keydata read from %s, "
                   "aborting...\n"),
                keyfile.c_str());
        free(wrapped_passphrase);
        exit(0);
      }

      length = DEFAULT_PASSPHRASE_LENGTH;
      passphrase = (char*)malloc(length);
      memset(passphrase, 0, length);

      if (auto error = AesUnwrap((unsigned char*)wrapdata, length / 8,
                                 (unsigned char*)wrapped_passphrase,
                                 (unsigned char*)passphrase)) {
        fprintf(stderr,
                T_("Failed to aes unwrap the keydata read from %s using the "
                   "wrap data from %s ERR=%s, aborting...\n"),
                keyfile.c_str(), wrap_keyfile.c_str(), error->c_str());
        free(wrapped_passphrase);
        exit(0);
      }

      free(wrapped_passphrase);
    } else {
      if (base64_transform) {
        /* As we first go from base64 to bin and the Base64ToBin has a check
         * if the decoded string will fit we need to alocate some more bytes
         * for the decoded buffer to be sure it will fit. */
        length = DEFAULT_PASSPHRASE_LENGTH + 4;
        passphrase = (char*)malloc(length);
        memset(passphrase, 0, length);

        Base64ToBin(passphrase, length, keydata, strlen(keydata));
      } else {
        length = DEFAULT_PASSPHRASE_LENGTH;
        passphrase = (char*)malloc(length);
        memset(passphrase, 0, length);
        bstrncpy(passphrase, keydata, length);
      }
    }

    Dmsg1(10, "Unwrapped passphrase = %s\n", passphrase);
    fprintf(stdout, T_("%s\n"), passphrase);

    free(passphrase);
    exit(0);
  }

  // Clear the loaded encryption key of the given drive.
  if (clear_encryption) {
    if (ClearScsiEncryptionKey(-1, device_name.c_str())) {
      exit(0);
    } else {
      exit(1);
    }
  }

  // Get the drive encryption status of the given drive.
  if (drive_encryption_status) {
    POOLMEM* encryption_status = GetPoolMemory(PM_MESSAGE);

    if (GetScsiDriveEncryptionStatus(-1, device_name.c_str(), encryption_status,
                                     0)) {
      fprintf(stdout, T_("%s"), encryption_status);
      FreePoolMemory(encryption_status);
    } else {
      FreePoolMemory(encryption_status);
      exit(1);
    }
  }

  // Load a new encryption key onto the given drive.
  if (set_encryption) {
    ReadKeyBits(keyfile, keydata, sizeof(keydata));

    if (SetScsiEncryptionKey(-1, device_name.c_str(), keydata)) {
      exit(0);
    } else {
      exit(1);
    }
  }

  /* Get the volume encryption status of volume currently loaded in the given
   * drive. */
  if (volume_encryption_status) {
    POOLMEM* encryption_status = GetPoolMemory(PM_MESSAGE);

    if (GetScsiVolumeEncryptionStatus(-1, device_name.c_str(),
                                      encryption_status, 0)) {
      fprintf(stdout, T_("%s"), encryption_status);
      FreePoolMemory(encryption_status);
    } else {
      FreePoolMemory(encryption_status);
      exit(1);
    }
  }

  exit(0);
}
