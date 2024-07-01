/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

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

#include "filed/filed_utils.h"
#include "filed/filed_globals.h"
#include "lib/parse_conf.h"


namespace filedaemon {

static bool InitPublicPrivateKeys(const char* configfile)
{
  bool OK = true;
  /* Load our keypair */
  me->pki_keypair = crypto_keypair_new();
  if (!me->pki_keypair) {
    Emsg0(M_FATAL, 0, T_("Failed to allocate a new keypair object.\n"));
    OK = false;
  } else {
    if (!CryptoKeypairLoadCert(me->pki_keypair, me->pki_keypair_file)) {
      Emsg2(M_FATAL, 0,
            T_("Failed to load public certificate for File"
               " daemon \"%s\" in %s.\n"),
            me->resource_name_, configfile);
      OK = false;
    }

    if (!CryptoKeypairLoadKey(me->pki_keypair, me->pki_keypair_file, nullptr,
                              nullptr)) {
      Emsg2(M_FATAL, 0,
            T_("Failed to load private key for File"
               " daemon \"%s\" in %s.\n"),
            me->resource_name_, configfile);
      OK = false;
    }
  }

  // Trusted Signers. We're always trusted.
  me->pki_signers = new alist<X509_KEYPAIR*>(10, not_owned_by_alist);
  if (me->pki_keypair) {
    me->pki_signers->append(crypto_keypair_dup(me->pki_keypair));
  }

  /* If additional signing public keys have been specified, load them up */
  if (me->pki_signing_key_files) {
    for (auto* filepath : *me->pki_signing_key_files) {
      X509_KEYPAIR* keypair;

      keypair = crypto_keypair_new();
      if (!keypair) {
        Emsg0(M_FATAL, 0, T_("Failed to allocate a new keypair object.\n"));
        OK = false;
      } else {
        if (CryptoKeypairLoadCert(keypair, filepath)) {
          me->pki_signers->append(keypair);

          /* Attempt to load a private key, if available */
          if (CryptoKeypairHasKey(filepath)) {
            if (!CryptoKeypairLoadKey(keypair, filepath, nullptr, nullptr)) {
              Emsg3(M_FATAL, 0,
                    T_("Failed to load private key from file %s for File"
                       " daemon \"%s\" in %s.\n"),
                    filepath, me->resource_name_, configfile);
              OK = false;
            }
          }

        } else {
          Emsg3(M_FATAL, 0,
                T_("Failed to load trusted signer certificate"
                   " from file %s for File daemon \"%s\" in %s.\n"),
                filepath, me->resource_name_, configfile);
          OK = false;
        }
      }
    }
  }

  /* Crypto recipients. We're always included as a recipient.
   * The symmetric session key will be encrypted for each of these readers. */
  me->pki_recipients = new alist<X509_KEYPAIR*>(10, not_owned_by_alist);
  if (me->pki_keypair) {
    me->pki_recipients->append(crypto_keypair_dup(me->pki_keypair));
  }


  /* If additional keys have been specified, load them up */
  if (me->pki_master_key_files) {
    for (auto* filepath : *me->pki_master_key_files) {
      X509_KEYPAIR* keypair;

      keypair = crypto_keypair_new();
      if (!keypair) {
        Emsg0(M_FATAL, 0, T_("Failed to allocate a new keypair object.\n"));
        OK = false;
      } else {
        if (CryptoKeypairLoadCert(keypair, filepath)) {
          me->pki_recipients->append(keypair);
        } else {
          Emsg3(M_FATAL, 0,
                T_("Failed to load master key certificate"
                   " from file %s for File daemon \"%s\" in %s.\n"),
                filepath, me->resource_name_, configfile);
          OK = false;
        }
      }
    }
  }

  return OK;
}

/**
 * Make a quick check to see that we have all the
 * resources needed.
 */
bool CheckResources()
{
  bool OK = true;
  DirectorResource* director;
  const std::string& configfile = my_config->get_base_config_path();

  ResLocker _{my_config};

  me = (ClientResource*)my_config->GetNextRes(R_CLIENT, nullptr);
  my_config->own_resource_ = me;
  if (!me) {
    Emsg1(M_FATAL, 0,
          T_("No File daemon resource defined in %s\n"
             "Without that I don't know who I am :-(\n"),
          configfile.c_str());
    OK = false;
  } else {
    if (my_config->GetNextRes(R_CLIENT, (BareosResource*)me) != nullptr) {
      Emsg1(M_FATAL, 0, T_("Only one Client resource permitted in %s\n"),
            configfile.c_str());
      OK = false;
    }
    MyNameIs(0, nullptr, me->resource_name_);
    if (!me->messages) {
      me->messages = (MessagesResource*)my_config->GetNextRes(R_MSGS, nullptr);
      if (!me->messages) {
        Emsg1(M_FATAL, 0, T_("No Messages resource defined in %s\n"),
              configfile.c_str());
        OK = false;
      }
    }
    if (me->pki_encrypt || me->pki_sign) {
#ifndef HAVE_CRYPTO
      Jmsg(
          nullptr, M_FATAL, 0,
          T_("PKI encryption/signing enabled but not compiled into Bareos.\n"));
      OK = false;
#endif
    }

    /* pki_encrypt implies pki_sign */
    if (me->pki_encrypt) { me->pki_sign = true; }

    if ((me->pki_encrypt || me->pki_sign) && !me->pki_keypair_file) {
      Emsg2(M_FATAL, 0,
            T_("\"PKI Key Pair\" must be defined for File"
               " daemon \"%s\" in %s if either \"PKI Sign\" or"
               " \"PKI Encrypt\" are enabled.\n"),
            me->resource_name_, configfile.c_str());
      OK = false;
    }

    if (OK && (me->pki_encrypt || me->pki_sign)) {
      OK = InitPublicPrivateKeys(configfile.c_str());
    }
  }


  /* Verify that a director record exists */
  director = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);

  if (!director) {
    Emsg1(M_FATAL, 0, T_("No Director resource defined in %s\n"),
          configfile.c_str());
    OK = false;
  }

  if (OK) {
    CloseMsg(nullptr);              /* close temp message handler */
    InitMsg(nullptr, me->messages); /* open user specified message handler */
    if (me->secure_erase_cmdline) {
      SetSecureEraseCmdline(me->secure_erase_cmdline);
    }
    if (me->log_timestamp_format) {
      SetLogTimestampFormat(me->log_timestamp_format);
    }
  }

  return OK;
}


}  // namespace filedaemon
