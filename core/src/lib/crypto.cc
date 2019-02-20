/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2011 Free Software Foundation Europe e.V.
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
 * crypto.c Encryption support functions
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 */

#include "include/bareos.h"

/*
 * BAREOS ASN.1 Syntax
 *
 * OID Allocation:
 * Prefix:
 * iso.org.dod.internet.private.enterprise.bareos.backup(1.3.6.1.4.1.41093.1)
 * Organization: Bareos GmbH & Co. KG
 * Contact Name: Marco van Wieringen
 * Contact E-mail: marco.van.wieringen@bareos.com
 *
 * Top Level Allocations - 1
 * 1 - Published Allocations
 *   1.1 - Bareos Encryption
 *
 * Bareos Encryption - 1.1.1
 * 1 - ASN.1 Modules
 *    1.1 - BareosCrypto
 * 2 - ASN.1 Object Identifiers
 *    2.1 - SignatureData
 *    2.2 - SignerInfo
 *    2.3 - CryptoData
 *    2.4 - RecipientInfo
 *
 * BareosCrypto { iso(1) identified-organization(3) usdod(6)
 *                internet(1) private(4) enterprises(1) bareos(41093)
 *                bareos(1) published(1) bareos-encryption(1)
 *                asn1-modules(1) bareos-crypto(1) }
 *
 * DEFINITIONS AUTOMATIC TAGS ::=
 * BEGIN
 *
 * SignatureData ::= SEQUENCE {
 *    version         Version DEFAULT v0,
 *    signerInfo      SignerInfo }
 *
 * CryptoData ::= SEQUENCE {
 *    version                     Version DEFAULT v0,
 *    contentEncryptionAlgorithm  ContentEncryptionAlgorithmIdentifier,
 *    iv                          InitializationVector,
 *    recipientInfo               RecipientInfo
 * }
 *
 * SignerInfo ::= SET OF SignerInfo
 * RecipientInfo ::= SET OF RecipientInfo
 *
 * Version ::= INTEGER { v0(0) }
 *
 * SignerInfo ::= SEQUENCE {
 *    version                 Version,
 *    subjectKeyIdentifier    SubjectKeyIdentifier,
 *    digestAlgorithm         DigestAlgorithmIdentifier,
 *    signatureAlgorithm      SignatureAlgorithmIdentifier,
 *    signature               SignatureValue }
 *
 * RecipientInfo ::= SEQUENCE {
 *    version                 Version
 *    subjectKeyIdentifier    SubjectKeyIdentifier
 *    keyEncryptionAlgorithm  KeyEncryptionAlgorithmIdentifier
 *    encryptedKey            EncryptedKey
 * }
 *
 * SubjectKeyIdentifier ::= OCTET STRING
 *
 * DigestAlgorithmIdentifier ::= AlgorithmIdentifier
 *
 * SignatureAlgorithmIdentifier ::= AlgorithmIdentifier
 *
 * KeyEncryptionAlgorithmIdentifier ::= AlgorithmIdentifier
 *
 * ContentEncryptionAlgorithmIdentifier ::= AlgorithmIdentifier
 *
 * InitializationVector ::= OCTET STRING
 *
 * SignatureValue ::= OCTET STRING
 *
 * EncryptedKey ::= OCTET STRING
 *
 * AlgorithmIdentifier ::= OBJECT IDENTIFIER
 *
 * END
 */

/* Shared Code */

/*
 * Default PEM encryption passphrase callback.
 * Returns an empty password.
 */
int CryptoDefaultPemCallback(char* buf, int size, const void* userdata)
{
  bstrncpy(buf, "", size);
  return (strlen(buf));
}

/*
 * Returns the ASCII name of the digest type.
 * Returns: ASCII name of digest type.
 */
const char* crypto_digest_name(crypto_digest_t type)
{
  switch (type) {
    case CRYPTO_DIGEST_MD5:
      return "MD5";
    case CRYPTO_DIGEST_SHA1:
      return "SHA1";
    case CRYPTO_DIGEST_SHA256:
      return "SHA256";
    case CRYPTO_DIGEST_SHA512:
      return "SHA512";
    case CRYPTO_DIGEST_NONE:
      return "None";
    default:
      return "Invalid Digest Type";
  }
}

/*
 * Given a stream type, returns the associated
 * crypto_digest_t value.
 */
crypto_digest_t CryptoDigestStreamType(int stream)
{
  switch (stream) {
    case STREAM_MD5_DIGEST:
      return CRYPTO_DIGEST_MD5;
    case STREAM_SHA1_DIGEST:
      return CRYPTO_DIGEST_SHA1;
    case STREAM_SHA256_DIGEST:
      return CRYPTO_DIGEST_SHA256;
    case STREAM_SHA512_DIGEST:
      return CRYPTO_DIGEST_SHA512;
    default:
      return CRYPTO_DIGEST_NONE;
  }
}

/*
 *  * Given a crypto_error_t value, return the associated
 *   * error string
 *    */
const char* crypto_strerror(crypto_error_t error)
{
  switch (error) {
    case CRYPTO_ERROR_NONE:
      return _("No error");
    case CRYPTO_ERROR_NOSIGNER:
      return _("Signer not found");
    case CRYPTO_ERROR_NORECIPIENT:
      return _("Recipient not found");
    case CRYPTO_ERROR_INVALID_DIGEST:
      return _("Unsupported digest algorithm");
    case CRYPTO_ERROR_INVALID_CRYPTO:
      return _("Unsupported encryption algorithm");
    case CRYPTO_ERROR_BAD_SIGNATURE:
      return _("Signature is invalid");
    case CRYPTO_ERROR_DECRYPTION:
      return _("Decryption error");
    case CRYPTO_ERROR_INTERNAL:
      /* This shouldn't happen */
      return _("Internal error");
    default:
      return _("Unknown error");
  }
}
