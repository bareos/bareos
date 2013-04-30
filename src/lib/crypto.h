/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * crypto.h Encryption support functions
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 *
 * Version $Id$
 *
 * This file was contributed to the Bacula project by Landon Fuller.
 *
 * Landon Fuller has been granted a perpetual, worldwide, non-exclusive,
 * no-charge, royalty-free, irrevocable copyright * license to reproduce,
 * prepare derivative works of, publicly display, publicly perform,
 * sublicense, and distribute the original work contributed by Landon Fuller
 * to the Bacula project in source or object form.
 *
 * If you wish to license these contributions under an alternate open source
 * license please contact Landon Fuller <landonf@opendarwin.org>.
 */

#ifndef __CRYPTO_H_
#define __CRYPTO_H_

/* Opaque X509 Public/Private Key Pair Structure */
typedef struct X509_Keypair X509_KEYPAIR;

/* Opaque Message Digest Structure */
/* Digest is defined (twice) in crypto.c */
typedef struct Digest DIGEST;

/* Opaque Message Signature Structure */
typedef struct Signature SIGNATURE;

/* Opaque PKI Symmetric Key Data Structure */
typedef struct Crypto_Session CRYPTO_SESSION;

/* Opaque Encryption/Decryption Context Structure */
typedef struct Cipher_Context CIPHER_CONTEXT;

/* PEM Decryption Passphrase Callback */
typedef int (CRYPTO_PEM_PASSWD_CB) (char *buf, int size, const void *userdata);

/* Digest Types */
typedef enum {
   /* These are stored on disk and MUST NOT change */
   CRYPTO_DIGEST_NONE = 0,
   CRYPTO_DIGEST_MD5 = 1,
   CRYPTO_DIGEST_SHA1 = 2,
   CRYPTO_DIGEST_SHA256 = 3,
   CRYPTO_DIGEST_SHA512 = 4
} crypto_digest_t;

/* Cipher Types */
typedef enum {
   /* These are not stored on disk */
   CRYPTO_CIPHER_AES_128_CBC,
   CRYPTO_CIPHER_AES_192_CBC,
   CRYPTO_CIPHER_AES_256_CBC,
   CRYPTO_CIPHER_BLOWFISH_CBC
} crypto_cipher_t;

/* Crypto API Errors */
typedef enum {
   CRYPTO_ERROR_NONE           = 0, /* No error */
   CRYPTO_ERROR_NOSIGNER       = 1, /* Signer not found */
   CRYPTO_ERROR_NORECIPIENT    = 2, /* Recipient not found */
   CRYPTO_ERROR_INVALID_DIGEST = 3, /* Unsupported digest algorithm */
   CRYPTO_ERROR_INVALID_CRYPTO = 4, /* Unsupported encryption algorithm */
   CRYPTO_ERROR_BAD_SIGNATURE  = 5, /* Signature is invalid */
   CRYPTO_ERROR_DECRYPTION     = 6, /* Decryption error */
   CRYPTO_ERROR_INTERNAL       = 7  /* Internal Error */
} crypto_error_t;

/* Message Digest Sizes */
#define CRYPTO_DIGEST_MD5_SIZE 16     /* 128 bits */
#define CRYPTO_DIGEST_SHA1_SIZE 20    /* 160 bits */
#define CRYPTO_DIGEST_SHA256_SIZE 32  /* 256 bits */
#define CRYPTO_DIGEST_SHA512_SIZE 64  /* 512 bits */

/* Maximum Message Digest Size */
#ifdef HAVE_OPENSSL

/* Let OpenSSL define a few things */
#define CRYPTO_DIGEST_MAX_SIZE         EVP_MAX_MD_SIZE
#define CRYPTO_CIPHER_MAX_BLOCK_SIZE   EVP_MAX_BLOCK_LENGTH

#else /* HAVE_OPENSSL */

/*
 * This must be kept in sync with the available message digest algorithms.
 * Just in case someone forgets, I've added assertions
 * to crypto_digest_finalize().
 *      MD5: 128 bits
 *      SHA-1: 160 bits
 */
#ifndef HAVE_SHA2
#define CRYPTO_DIGEST_MAX_SIZE CRYPTO_DIGEST_SHA1_SIZE
#else
#define CRYPTO_DIGEST_MAX_SIZE CRYPTO_DIGEST_SHA512_SIZE
#endif

/* Dummy Value */
#define CRYPTO_CIPHER_MAX_BLOCK_SIZE 0

#endif /* HAVE_OPENSSL */

#endif /* __CRYPTO_H_ */
