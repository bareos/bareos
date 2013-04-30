/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2011 Free Software Foundation Europe e.V.

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
 * crypto.c Encryption support functions
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 *
 * This file was contributed to the Bacula project by Landon Fuller.
 *
 * Landon Fuller has been granted a perpetual, worldwide, non-exclusive,
 * no-charge, royalty-free, irrevocable copyright license to reproduce,
 * prepare derivative works of, publicly display, publicly perform,
 * sublicense, and distribute the original work contributed by Landon Fuller
 * to the Bacula project in source or object form.
 *
 * If you wish to license these contributions under an alternate open source
 * license please contact Landon Fuller <landonf@opendarwin.org>.
 */


#include "bacula.h"
#include "jcr.h"
#include <assert.h>

/**
 * For OpenSSL version 1.x, EVP_PKEY_encrypt no longer
 *  exists.  It was not an official API.
 */
#ifdef HAVE_OPENSSLv1
#define EVP_PKEY_encrypt EVP_PKEY_encrypt_old
#define EVP_PKEY_decrypt EVP_PKEY_decrypt_old
#endif

/*
 * Bacula ASN.1 Syntax
 *
 * OID Allocation:
 * Prefix: iso.org.dod.internet.private.enterprise.threerings.external.bacula (1.3.6.1.4.1.22054.500.2)
 * Organization: Bacula Project
 * Contact Name: Kern Sibbald
 * Contact E-mail: kern@sibbald.com
 *
 * Top Level Allocations - 500.2
 * 1 - Published Allocations
 *   1.1 - Bacula Encryption
 *
 * Bacula Encryption - 500.2.1.1
 * 1 - ASN.1 Modules
 *    1.1 - BaculaCrypto
 * 2 - ASN.1 Object Identifiers
 *    2.1 - SignatureData
 *    2.2 - SignerInfo
 *    2.3 - CryptoData
 *    2.4 - RecipientInfo
 *
 * BaculaCrypto { iso(1) identified-organization(3) usdod(6)
 *                internet(1) private(4) enterprises(1) three-rings(22054)
 *                external(500) bacula(2) published(1) bacula-encryption(1)
 *                asn1-modules(1) bacula-crypto(1) }
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

#ifdef HAVE_CRYPTO /* Is encryption enabled? */
#ifdef HAVE_OPENSSL /* How about OpenSSL? */

/* Are we initialized? */
static int crypto_initialized = false;

/* ASN.1 Declarations */
#define BACULA_ASN1_VERSION 0

typedef struct {
   ASN1_INTEGER *version;
   ASN1_OCTET_STRING *subjectKeyIdentifier;
   ASN1_OBJECT *digestAlgorithm;
   ASN1_OBJECT *signatureAlgorithm;
   ASN1_OCTET_STRING *signature;
} SignerInfo;

typedef struct {
   ASN1_INTEGER *version;
   ASN1_OCTET_STRING *subjectKeyIdentifier;
   ASN1_OBJECT *keyEncryptionAlgorithm;
   ASN1_OCTET_STRING *encryptedKey;
} RecipientInfo;

ASN1_SEQUENCE(SignerInfo) = {
   ASN1_SIMPLE(SignerInfo, version, ASN1_INTEGER),
   ASN1_SIMPLE(SignerInfo, subjectKeyIdentifier, ASN1_OCTET_STRING),
   ASN1_SIMPLE(SignerInfo, digestAlgorithm, ASN1_OBJECT),
   ASN1_SIMPLE(SignerInfo, signatureAlgorithm, ASN1_OBJECT),
   ASN1_SIMPLE(SignerInfo, signature, ASN1_OCTET_STRING)
} ASN1_SEQUENCE_END(SignerInfo);

ASN1_SEQUENCE(RecipientInfo) = {
   ASN1_SIMPLE(RecipientInfo, version, ASN1_INTEGER),
   ASN1_SIMPLE(RecipientInfo, subjectKeyIdentifier, ASN1_OCTET_STRING),
   ASN1_SIMPLE(RecipientInfo, keyEncryptionAlgorithm, ASN1_OBJECT),
   ASN1_SIMPLE(RecipientInfo, encryptedKey, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(RecipientInfo);

typedef struct {
   ASN1_INTEGER *version;
   STACK_OF(SignerInfo) *signerInfo;
} SignatureData;

typedef struct {
   ASN1_INTEGER *version;
   ASN1_OBJECT *contentEncryptionAlgorithm;
   ASN1_OCTET_STRING *iv;
   STACK_OF(RecipientInfo) *recipientInfo;
} CryptoData;

ASN1_SEQUENCE(SignatureData) = {
   ASN1_SIMPLE(SignatureData, version, ASN1_INTEGER),
   ASN1_SET_OF(SignatureData, signerInfo, SignerInfo),
} ASN1_SEQUENCE_END(SignatureData);

ASN1_SEQUENCE(CryptoData) = {
   ASN1_SIMPLE(CryptoData, version, ASN1_INTEGER),
   ASN1_SIMPLE(CryptoData, contentEncryptionAlgorithm, ASN1_OBJECT),
   ASN1_SIMPLE(CryptoData, iv, ASN1_OCTET_STRING),
   ASN1_SET_OF(CryptoData, recipientInfo, RecipientInfo)
} ASN1_SEQUENCE_END(CryptoData);

IMPLEMENT_ASN1_FUNCTIONS(SignerInfo)
IMPLEMENT_ASN1_FUNCTIONS(RecipientInfo)
IMPLEMENT_ASN1_FUNCTIONS(SignatureData)
IMPLEMENT_ASN1_FUNCTIONS(CryptoData)
IMPLEMENT_STACK_OF(SignerInfo)
IMPLEMENT_STACK_OF(RecipientInfo)

/*
 * SignerInfo and RecipientInfo stack macros, generated by OpenSSL's util/mkstack.pl.
 */
#define sk_SignerInfo_new(st) SKM_sk_new(SignerInfo, (st))
#define sk_SignerInfo_new_null() SKM_sk_new_null(SignerInfo)
#define sk_SignerInfo_free(st) SKM_sk_free(SignerInfo, (st))
#define sk_SignerInfo_num(st) SKM_sk_num(SignerInfo, (st))
#define sk_SignerInfo_value(st, i) SKM_sk_value(SignerInfo, (st), (i))
#define sk_SignerInfo_set(st, i, val) SKM_sk_set(SignerInfo, (st), (i), (val))
#define sk_SignerInfo_zero(st) SKM_sk_zero(SignerInfo, (st))
#define sk_SignerInfo_push(st, val) SKM_sk_push(SignerInfo, (st), (val))
#define sk_SignerInfo_unshift(st, val) SKM_sk_unshift(SignerInfo, (st), (val))
#define sk_SignerInfo_find(st, val) SKM_sk_find(SignerInfo, (st), (val))
#define sk_SignerInfo_delete(st, i) SKM_sk_delete(SignerInfo, (st), (i))
#define sk_SignerInfo_delete_ptr(st, ptr) SKM_sk_delete_ptr(SignerInfo, (st), (ptr))
#define sk_SignerInfo_insert(st, val, i) SKM_sk_insert(SignerInfo, (st), (val), (i))
#define sk_SignerInfo_set_cmp_func(st, cmp) SKM_sk_set_cmp_func(SignerInfo, (st), (cmp))
#define sk_SignerInfo_dup(st) SKM_sk_dup(SignerInfo, st)
#define sk_SignerInfo_pop_free(st, free_func) SKM_sk_pop_free(SignerInfo, (st), (free_func))
#define sk_SignerInfo_shift(st) SKM_sk_shift(SignerInfo, (st))
#define sk_SignerInfo_pop(st) SKM_sk_pop(SignerInfo, (st))
#define sk_SignerInfo_sort(st) SKM_sk_sort(SignerInfo, (st))
#define sk_SignerInfo_is_sorted(st) SKM_sk_is_sorted(SignerInfo, (st))

#define d2i_ASN1_SET_OF_SignerInfo(st, pp, length, d2i_func, free_func, ex_tag, ex_class) \
        SKM_ASN1_SET_OF_d2i(SignerInfo, (st), (pp), (length), (d2i_func), (free_func), (ex_tag), (ex_class)) 
#define i2d_ASN1_SET_OF_SignerInfo(st, pp, i2d_func, ex_tag, ex_class, is_set) \
        SKM_ASN1_SET_OF_i2d(SignerInfo, (st), (pp), (i2d_func), (ex_tag), (ex_class), (is_set))
#define ASN1_seq_pack_SignerInfo(st, i2d_func, buf, len) \
        SKM_ASN1_seq_pack(SignerInfo, (st), (i2d_func), (buf), (len))
#define ASN1_seq_unpack_SignerInfo(buf, len, d2i_func, free_func) \
        SKM_ASN1_seq_unpack(SignerInfo, (buf), (len), (d2i_func), (free_func))

#define sk_RecipientInfo_new(st) SKM_sk_new(RecipientInfo, (st))
#define sk_RecipientInfo_new_null() SKM_sk_new_null(RecipientInfo)
#define sk_RecipientInfo_free(st) SKM_sk_free(RecipientInfo, (st))
#define sk_RecipientInfo_num(st) SKM_sk_num(RecipientInfo, (st))
#define sk_RecipientInfo_value(st, i) SKM_sk_value(RecipientInfo, (st), (i))
#define sk_RecipientInfo_set(st, i, val) SKM_sk_set(RecipientInfo, (st), (i), (val))
#define sk_RecipientInfo_zero(st) SKM_sk_zero(RecipientInfo, (st))
#define sk_RecipientInfo_push(st, val) SKM_sk_push(RecipientInfo, (st), (val))
#define sk_RecipientInfo_unshift(st, val) SKM_sk_unshift(RecipientInfo, (st), (val))
#define sk_RecipientInfo_find(st, val) SKM_sk_find(RecipientInfo, (st), (val))
#define sk_RecipientInfo_delete(st, i) SKM_sk_delete(RecipientInfo, (st), (i))
#define sk_RecipientInfo_delete_ptr(st, ptr) SKM_sk_delete_ptr(RecipientInfo, (st), (ptr))
#define sk_RecipientInfo_insert(st, val, i) SKM_sk_insert(RecipientInfo, (st), (val), (i))
#define sk_RecipientInfo_set_cmp_func(st, cmp) SKM_sk_set_cmp_func(RecipientInfo, (st), (cmp))
#define sk_RecipientInfo_dup(st) SKM_sk_dup(RecipientInfo, st)
#define sk_RecipientInfo_pop_free(st, free_func) SKM_sk_pop_free(RecipientInfo, (st), (free_func))
#define sk_RecipientInfo_shift(st) SKM_sk_shift(RecipientInfo, (st))
#define sk_RecipientInfo_pop(st) SKM_sk_pop(RecipientInfo, (st))
#define sk_RecipientInfo_sort(st) SKM_sk_sort(RecipientInfo, (st))
#define sk_RecipientInfo_is_sorted(st) SKM_sk_is_sorted(RecipientInfo, (st))

#define d2i_ASN1_SET_OF_RecipientInfo(st, pp, length, d2i_func, free_func, ex_tag, ex_class) \
        SKM_ASN1_SET_OF_d2i(RecipientInfo, (st), (pp), (length), (d2i_func), (free_func), (ex_tag), (ex_class)) 
#define i2d_ASN1_SET_OF_RecipientInfo(st, pp, i2d_func, ex_tag, ex_class, is_set) \
        SKM_ASN1_SET_OF_i2d(RecipientInfo, (st), (pp), (i2d_func), (ex_tag), (ex_class), (is_set))
#define ASN1_seq_pack_RecipientInfo(st, i2d_func, buf, len) \
        SKM_ASN1_seq_pack(RecipientInfo, (st), (i2d_func), (buf), (len))
#define ASN1_seq_unpack_RecipientInfo(buf, len, d2i_func, free_func) \
        SKM_ASN1_seq_unpack(RecipientInfo, (buf), (len), (d2i_func), (free_func))
/* End of util/mkstack.pl block */

/* X509 Public/Private Key Pair Structure */
struct X509_Keypair {
   ASN1_OCTET_STRING *keyid;
   EVP_PKEY *pubkey;
   EVP_PKEY *privkey;
};

/* Message Digest Structure */
struct Digest {
   crypto_digest_t type;
   JCR *jcr;
   EVP_MD_CTX ctx;
};

/* Message Signature Structure */
struct Signature {
   SignatureData *sigData;
   JCR *jcr;
};

/* Encryption Session Data */
struct Crypto_Session {
   CryptoData *cryptoData;                        /* ASN.1 Structure */
   unsigned char *session_key;                    /* Private symmetric session key */
   size_t session_key_len;                        /* Symmetric session key length */
};

/* Symmetric Cipher Context */
struct Cipher_Context {
   EVP_CIPHER_CTX ctx;
};

/* PEM Password Dispatch Context */
typedef struct PEM_CB_Context {
   CRYPTO_PEM_PASSWD_CB *pem_callback;
   const void *pem_userdata;
} PEM_CB_CONTEXT;

/*
 * Extract subjectKeyIdentifier from x509 certificate.
 * Returns: On success, an ASN1_OCTET_STRING that must be freed via M_ASN1_OCTET_STRING_free().
 *          NULL on failure.
 */
static ASN1_OCTET_STRING *openssl_cert_keyid(X509 *cert) {
   X509_EXTENSION *ext;
   const X509V3_EXT_METHOD *method;
   ASN1_OCTET_STRING *keyid;
   int i;
#if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
   const unsigned char *ext_value_data;
#else
   unsigned char *ext_value_data;
#endif


   /* Find the index to the subjectKeyIdentifier extension */
   i = X509_get_ext_by_NID(cert, NID_subject_key_identifier, -1);
   if (i < 0) {
      /* Not found */
      return NULL;
   }

   /* Grab the extension */
   ext = X509_get_ext(cert, i);

   /* Get x509 extension method structure */
   if (!(method = X509V3_EXT_get(ext))) {
      return NULL;
   }

   ext_value_data = ext->value->data;

#if (OPENSSL_VERSION_NUMBER > 0x00907000L)
   if (method->it) {
      /* New style ASN1 */

      /* Decode ASN1 item in data */
      keyid = (ASN1_OCTET_STRING *) ASN1_item_d2i(NULL, &ext_value_data, ext->value->length,
                                                  ASN1_ITEM_ptr(method->it));
   } else {
      /* Old style ASN1 */

      /* Decode ASN1 item in data */
      keyid = (ASN1_OCTET_STRING *) method->d2i(NULL, &ext_value_data, ext->value->length);
   }

#else
   keyid = (ASN1_OCTET_STRING *) method->d2i(NULL, &ext_value_data, ext->value->length);
#endif

   return keyid;
}

/*
 * Create a new keypair object.
 *  Returns: A pointer to a X509 KEYPAIR object on success.
 *           NULL on failure.
 */
X509_KEYPAIR *crypto_keypair_new(void) 
{
   X509_KEYPAIR *keypair;

   /* Allocate our keypair structure */
   keypair = (X509_KEYPAIR *)malloc(sizeof(X509_KEYPAIR));

   /* Initialize our keypair structure */
   keypair->keyid = NULL;
   keypair->pubkey = NULL;
   keypair->privkey = NULL;

   return keypair;
}

/*
 * Create a copy of a keypair object. The underlying
 * EVP objects are not duplicated, as no EVP_PKEY_dup()
 * API is available. Instead, the reference count is
 * incremented.
 */
X509_KEYPAIR *crypto_keypair_dup(X509_KEYPAIR *keypair)
{
   X509_KEYPAIR *newpair;

   newpair = crypto_keypair_new();

   if (!newpair) {
      /* Allocation failed */
      return NULL;
   }

   /* Increment the public key ref count */
   if (keypair->pubkey) {
      CRYPTO_add(&(keypair->pubkey->references), 1, CRYPTO_LOCK_EVP_PKEY);
      newpair->pubkey = keypair->pubkey;
   }

   /* Increment the private key ref count */
   if (keypair->privkey) {
      CRYPTO_add(&(keypair->privkey->references), 1, CRYPTO_LOCK_EVP_PKEY);
      newpair->privkey = keypair->privkey;
   }

   /* Duplicate the keyid */
   if (keypair->keyid) {
      newpair->keyid = M_ASN1_OCTET_STRING_dup(keypair->keyid);
      if (!newpair->keyid) {
         /* Allocation failed */
         crypto_keypair_free(newpair);
         return NULL;
      }
   }

   return newpair;
}


/*
 * Load a public key from a PEM-encoded x509 certificate.
 *  Returns: true on success
 *           false on failure
 */
int crypto_keypair_load_cert(X509_KEYPAIR *keypair, const char *file)
{
   BIO *bio;
   X509 *cert;

   /* Open the file */
   if (!(bio = BIO_new_file(file, "r"))) {
      openssl_post_errors(M_ERROR, _("Unable to open certificate file"));
      return false;
   }

   cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
   BIO_free(bio);
   if (!cert) {
      openssl_post_errors(M_ERROR, _("Unable to read certificate from file"));
      return false;
   }

   /* Extract the public key */
   if (!(keypair->pubkey = X509_get_pubkey(cert))) {
      openssl_post_errors(M_ERROR, _("Unable to extract public key from certificate"));
      goto err;
   }

   /* Extract the subjectKeyIdentifier extension field */
   if ((keypair->keyid = openssl_cert_keyid(cert)) == NULL) {
      Jmsg0(NULL, M_ERROR, 0,
         _("Provided certificate does not include the required subjectKeyIdentifier extension."));
      goto err;
   }

   /* Validate the public key type (only RSA is supported) */
   if (EVP_PKEY_type(keypair->pubkey->type) != EVP_PKEY_RSA) {
       Jmsg1(NULL, M_ERROR, 0, 
             _("Unsupported key type provided: %d\n"), EVP_PKEY_type(keypair->pubkey->type));
       goto err;
   }

   X509_free(cert);
   return true;

err:
   X509_free(cert);
   if (keypair->pubkey) {
      EVP_PKEY_free(keypair->pubkey);
   }
   return false;
}

/* Dispatch user PEM encryption callbacks */
static int crypto_pem_callback_dispatch (char *buf, int size, int rwflag, void *userdata)
{
   PEM_CB_CONTEXT *ctx = (PEM_CB_CONTEXT *) userdata;
   return (ctx->pem_callback(buf, size, ctx->pem_userdata));
}

/*
 * Check a PEM-encoded file
 * for the existence of a private key.
 * Returns: true if a private key is found
 *          false otherwise
 */
bool crypto_keypair_has_key(const char *file) {
   BIO *bio;
   char *name = NULL;
   char *header = NULL;
   unsigned char *data = NULL;
   bool retval = false;
   long len;

   if (!(bio = BIO_new_file(file, "r"))) {
      openssl_post_errors(M_ERROR, _("Unable to open private key file"));
      return false;
   }

   while (PEM_read_bio(bio, &name, &header, &data, &len)) {
      /* We don't care what the data is, just that it's there */
      OPENSSL_free(header);
      OPENSSL_free(data);

      /*
       * PEM Header Found, check for a private key
       * Due to OpenSSL limitations, we must specifically
       * list supported PEM private key encodings.
       */
      if (strcmp(name, PEM_STRING_RSA) == 0
            || strcmp(name, PEM_STRING_DSA) == 0
            || strcmp(name, PEM_STRING_PKCS8) == 0
            || strcmp(name, PEM_STRING_PKCS8INF) == 0) {
         retval = true;
         OPENSSL_free(name);
         break;
      } else {
         OPENSSL_free(name);
      }
   }

   /* Free our bio */
   BIO_free(bio);

   /* Post PEM-decoding error messages, if any */
   openssl_post_errors(M_ERROR, _("Unable to read private key from file"));
   return retval;
}

/*
 * Load a PEM-encoded private key.
 *  Returns: true on success
 *           false on failure
 */
int crypto_keypair_load_key(X509_KEYPAIR *keypair, const char *file,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata)
{
   BIO *bio;
   PEM_CB_CONTEXT ctx;

   /* Open the file */
   if (!(bio = BIO_new_file(file, "r"))) {
      openssl_post_errors(M_ERROR, _("Unable to open private key file"));
      return false;
   }

   /* Set up PEM encryption callback */
   if (pem_callback) {
      ctx.pem_callback = pem_callback;
      ctx.pem_userdata = pem_userdata;
   } else {
      ctx.pem_callback = crypto_default_pem_callback;
      ctx.pem_userdata = NULL;
   }

   keypair->privkey = PEM_read_bio_PrivateKey(bio, NULL, crypto_pem_callback_dispatch, &ctx);
   BIO_free(bio);
   if (!keypair->privkey) {
      openssl_post_errors(M_ERROR, _("Unable to read private key from file"));
      return false;
   }

   return true;
}

/*
 * Free memory associated with a keypair object.
 */
void crypto_keypair_free(X509_KEYPAIR *keypair)
{
   if (keypair->pubkey) {
      EVP_PKEY_free(keypair->pubkey);
   }
   if (keypair->privkey) {
      EVP_PKEY_free(keypair->privkey);
   }
   if (keypair->keyid) {
      M_ASN1_OCTET_STRING_free(keypair->keyid);
   }
   free(keypair);
}

/*
 * Create a new message digest context of the specified type
 *  Returns: A pointer to a DIGEST object on success.
 *           NULL on failure.
 */
DIGEST *crypto_digest_new(JCR *jcr, crypto_digest_t type)
{
   DIGEST *digest;
   const EVP_MD *md = NULL; /* Quell invalid uninitialized warnings */

   digest = (DIGEST *)malloc(sizeof(DIGEST));
   digest->type = type;
   digest->jcr = jcr;
   Dmsg1(150, "crypto_digest_new jcr=%p\n", jcr);

   /* Initialize the OpenSSL message digest context */
   EVP_MD_CTX_init(&digest->ctx);

   /* Determine the correct OpenSSL message digest type */
   switch (type) {
   case CRYPTO_DIGEST_MD5:
      md = EVP_md5();
      break;
   case CRYPTO_DIGEST_SHA1:
      md = EVP_sha1();
      break;
#ifdef HAVE_SHA2
   case CRYPTO_DIGEST_SHA256:
      md = EVP_sha256();
      break;
   case CRYPTO_DIGEST_SHA512:
      md = EVP_sha512();
      break;
#endif
   default:
      Jmsg1(jcr, M_ERROR, 0, _("Unsupported digest type: %d\n"), type);
      goto err;
   }

   /* Initialize the backing OpenSSL context */
   if (EVP_DigestInit_ex(&digest->ctx, md, NULL) == 0) {
      goto err;
   }

   return digest;

err:
   /* This should not happen, but never say never ... */
   Dmsg0(150, "Digest init failed.\n");
   openssl_post_errors(jcr, M_ERROR, _("OpenSSL digest initialization failed"));
   crypto_digest_free(digest);
   return NULL;
}

/*
 * Hash length bytes of data into the provided digest context.
 * Returns: true on success
 *          false on failure
 */
bool crypto_digest_update(DIGEST *digest, const uint8_t *data, uint32_t length)
{
   if (EVP_DigestUpdate(&digest->ctx, data, length) == 0) {
      Dmsg0(150, "digest update failed\n");
      openssl_post_errors(digest->jcr, M_ERROR, _("OpenSSL digest update failed"));
      return false;
   } else { 
      return true;
   }
}

/*
 * Finalize the data in digest, storing the result in dest and the result size
 * in length. The result size can be determined with crypto_digest_size().
 *
 * Returns: true on success
 *          false on failure
 */
bool crypto_digest_finalize(DIGEST *digest, uint8_t *dest, uint32_t *length)
{
   if (!EVP_DigestFinal(&digest->ctx, dest, (unsigned int *)length)) {
      Dmsg0(150, "digest finalize failed\n");
      openssl_post_errors(digest->jcr, M_ERROR, _("OpenSSL digest finalize failed"));
      return false;
   } else {
      return true;
   }
}

/*
 * Free memory associated with a digest object.
 */
void crypto_digest_free(DIGEST *digest)
{
  EVP_MD_CTX_cleanup(&digest->ctx);
  free(digest);
}

/*
 * Create a new message signature context.
 *  Returns: A pointer to a SIGNATURE object on success.
 *           NULL on failure.
 */
SIGNATURE *crypto_sign_new(JCR *jcr)
{
   SIGNATURE *sig;

   sig = (SIGNATURE *)malloc(sizeof(SIGNATURE));
   if (!sig) {
      return NULL;
   }

   sig->sigData = SignatureData_new();
   sig->jcr = jcr;
   Dmsg1(150, "crypto_sign_new jcr=%p\n", jcr);

   if (!sig->sigData) {
      /* Allocation failed in OpenSSL */
      free(sig);
      return NULL;
   }

   /* Set the ASN.1 structure version number */
   ASN1_INTEGER_set(sig->sigData->version, BACULA_ASN1_VERSION);

   return sig;
}

/*
 * For a given public key, find the associated SignatureInfo record
 *   and create a digest context for signature validation
 *
 * Returns: CRYPTO_ERROR_NONE on success, with the newly allocated DIGEST in digest.
 *          A crypto_error_t value on failure.
 */
crypto_error_t crypto_sign_get_digest(SIGNATURE *sig, X509_KEYPAIR *keypair, 
                                      crypto_digest_t &type, DIGEST **digest)
{
   STACK_OF(SignerInfo) *signers;
   SignerInfo *si;
   int i;

   signers = sig->sigData->signerInfo;

   for (i = 0; i < sk_SignerInfo_num(signers); i++) {
      si = sk_SignerInfo_value(signers, i);
      if (M_ASN1_OCTET_STRING_cmp(keypair->keyid, si->subjectKeyIdentifier) == 0) {
         /* Get the digest algorithm and allocate a digest context */
         Dmsg1(150, "crypto_sign_get_digest jcr=%p\n", sig->jcr);
         switch (OBJ_obj2nid(si->digestAlgorithm)) {
         case NID_md5:
            Dmsg0(100, "sign digest algorithm is MD5\n");
            type = CRYPTO_DIGEST_MD5;
            *digest = crypto_digest_new(sig->jcr, CRYPTO_DIGEST_MD5);
            break;
         case NID_sha1:
            Dmsg0(100, "sign digest algorithm is SHA1\n");
            type = CRYPTO_DIGEST_SHA1;
            *digest = crypto_digest_new(sig->jcr, CRYPTO_DIGEST_SHA1);
            break;
#ifdef HAVE_SHA2
         case NID_sha256:
            Dmsg0(100, "sign digest algorithm is SHA256\n");
            type = CRYPTO_DIGEST_SHA256;
            *digest = crypto_digest_new(sig->jcr, CRYPTO_DIGEST_SHA256);
            break;
         case NID_sha512:
            Dmsg0(100, "sign digest algorithm is SHA512\n");
            type = CRYPTO_DIGEST_SHA512;
            *digest = crypto_digest_new(sig->jcr, CRYPTO_DIGEST_SHA512);
            break;
#endif
         default:
            type = CRYPTO_DIGEST_NONE;
            *digest = NULL;
            return CRYPTO_ERROR_INVALID_DIGEST;
         }

         /* Shouldn't happen */
         if (*digest == NULL) {
            openssl_post_errors(sig->jcr, M_ERROR, _("OpenSSL digest_new failed"));
            return CRYPTO_ERROR_INVALID_DIGEST;
         } else {
            return CRYPTO_ERROR_NONE;
         }
      } else {
         openssl_post_errors(sig->jcr, M_ERROR, _("OpenSSL sign get digest failed"));
      }

   }

   return CRYPTO_ERROR_NOSIGNER;
}

/*
 * For a given signature, public key, and digest, verify the SIGNATURE.
 * Returns: CRYPTO_ERROR_NONE on success.
 *          A crypto_error_t value on failure.
 */
crypto_error_t crypto_sign_verify(SIGNATURE *sig, X509_KEYPAIR *keypair, DIGEST *digest)
{
   STACK_OF(SignerInfo) *signers;
   SignerInfo *si;
   int ok, i;
   unsigned int sigLen;
#if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
   const unsigned char *sigData;
#else
   unsigned char *sigData;
#endif

   signers = sig->sigData->signerInfo;

   /* Find the signer */
   for (i = 0; i < sk_SignerInfo_num(signers); i++) {
      si = sk_SignerInfo_value(signers, i);
      if (M_ASN1_OCTET_STRING_cmp(keypair->keyid, si->subjectKeyIdentifier) == 0) {
         /* Extract the signature data */
         sigLen = M_ASN1_STRING_length(si->signature);
         sigData = M_ASN1_STRING_data(si->signature);

         ok = EVP_VerifyFinal(&digest->ctx, sigData, sigLen, keypair->pubkey);
         if (ok >= 1) {
            return CRYPTO_ERROR_NONE;
         } else if (ok == 0) {
            openssl_post_errors(sig->jcr, M_ERROR, _("OpenSSL digest Verify final failed"));
            return CRYPTO_ERROR_BAD_SIGNATURE;
         } else if (ok < 0) {
            /* Shouldn't happen */
            openssl_post_errors(sig->jcr, M_ERROR, _("OpenSSL digest Verify final failed"));
            return CRYPTO_ERROR_INTERNAL;
         }
      }
   }
   Jmsg(sig->jcr, M_ERROR, 0, _("No signers found for crypto verify.\n"));
   /* Signer wasn't found. */
   return CRYPTO_ERROR_NOSIGNER;
}


/*
 * Add a new signer
 *  Returns: true on success
 *           false on failure
 */
int crypto_sign_add_signer(SIGNATURE *sig, DIGEST *digest, X509_KEYPAIR *keypair)
{
   SignerInfo *si = NULL;
   unsigned char *buf = NULL;
   unsigned int len;

   si = SignerInfo_new();

   if (!si) {
      /* Allocation failed in OpenSSL */
      return false;
   }

   /* Set the ASN.1 structure version number */
   ASN1_INTEGER_set(si->version, BACULA_ASN1_VERSION);

   /* Set the digest algorithm identifier */
   switch (digest->type) {
   case CRYPTO_DIGEST_MD5:
      si->digestAlgorithm = OBJ_nid2obj(NID_md5);
      break;
   case CRYPTO_DIGEST_SHA1:
      si->digestAlgorithm = OBJ_nid2obj(NID_sha1);
      break;
#ifdef HAVE_SHA2
   case CRYPTO_DIGEST_SHA256:
      si->digestAlgorithm = OBJ_nid2obj(NID_sha256);
      break;
   case CRYPTO_DIGEST_SHA512:
      si->digestAlgorithm = OBJ_nid2obj(NID_sha512);
      break;
#endif
   default:
      /* This should never happen */
      goto err;
   }

   /* Drop the string allocated by OpenSSL, and add our subjectKeyIdentifier */
   M_ASN1_OCTET_STRING_free(si->subjectKeyIdentifier);
   si->subjectKeyIdentifier = M_ASN1_OCTET_STRING_dup(keypair->keyid);

   /* Set our signature algorithm. We currently require RSA */
   assert(EVP_PKEY_type(keypair->pubkey->type) == EVP_PKEY_RSA);
   /* This is slightly evil. Reach into the MD structure and grab the key type */
   si->signatureAlgorithm = OBJ_nid2obj(digest->ctx.digest->pkey_type);

   /* Finalize/Sign our Digest */
   len = EVP_PKEY_size(keypair->privkey);
   buf = (unsigned char *) malloc(len);
   if (!EVP_SignFinal(&digest->ctx, buf, &len, keypair->privkey)) {
      openssl_post_errors(M_ERROR, _("Signature creation failed"));
      goto err;
   }

   /* Add the signature to the SignerInfo structure */
   if (!M_ASN1_OCTET_STRING_set(si->signature, buf, len)) {
      /* Allocation failed in OpenSSL */
      goto err;
   }

   /* No longer needed */
   free(buf);

   /* Push the new SignerInfo structure onto the stack */
   sk_SignerInfo_push(sig->sigData->signerInfo, si);

   return true;

err:
   if (si) {
      SignerInfo_free(si);
   }
   if (buf) {
      free(buf);
   }

   return false;
}

/*
 * Encodes the SignatureData structure. The length argument is used to specify the
 * size of dest. A length of 0 will cause no data to be written to dest, and the
 * required length to be written to length. The caller can then allocate sufficient
 * space for the output.
 *
 * Returns: true on success, stores the encoded data in dest, and the size in length.
 *          false on failure.
 */
int crypto_sign_encode(SIGNATURE *sig, uint8_t *dest, uint32_t *length)
{
   if (*length == 0) {
      *length = i2d_SignatureData(sig->sigData, NULL);
      return true;
   }

   *length = i2d_SignatureData(sig->sigData, (unsigned char **)&dest);
   return true;
}

/*
 * Decodes the SignatureData structure. The length argument is used to specify the
 * size of sigData.
 *
 * Returns: SIGNATURE instance on success.
 *          NULL on failure.

 */

SIGNATURE *crypto_sign_decode(JCR *jcr, const uint8_t *sigData, uint32_t length)
{
   SIGNATURE *sig;
#if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
   const unsigned char *p = (const unsigned char *) sigData;
#else
   unsigned char *p = (unsigned char *)sigData;
#endif

   sig = (SIGNATURE *)malloc(sizeof(SIGNATURE));
   if (!sig) {
      return NULL;
   }
   sig->jcr = jcr;

   /* d2i_SignatureData modifies the supplied pointer */
   sig->sigData  = d2i_SignatureData(NULL, &p, length);

   if (!sig->sigData) {
      /* Allocation / Decoding failed in OpenSSL */
      openssl_post_errors(jcr, M_ERROR, _("Signature decoding failed"));
      free(sig);
      return NULL;
   }

   return sig;
}

/*
 * Free memory associated with a signature object.
 */
void crypto_sign_free(SIGNATURE *sig)
{
   SignatureData_free(sig->sigData);
   free (sig);
}

/*
 * Create a new encryption session.
 *  Returns: A pointer to a CRYPTO_SESSION object on success.
 *           NULL on failure.
 *
 *  Note! Bacula malloc() fails if out of memory.
 */
CRYPTO_SESSION *crypto_session_new (crypto_cipher_t cipher, alist *pubkeys)
{
   CRYPTO_SESSION *cs;
   X509_KEYPAIR *keypair;
   const EVP_CIPHER *ec;
   unsigned char *iv;
   int iv_len;

   /* Allocate our session description structures */
   cs = (CRYPTO_SESSION *)malloc(sizeof(CRYPTO_SESSION));

   /* Initialize required fields */
   cs->session_key = NULL;

   /* Allocate a CryptoData structure */
   cs->cryptoData = CryptoData_new();

   if (!cs->cryptoData) {
      /* Allocation failed in OpenSSL */
      free(cs);
      return NULL;
   }

   /* Set the ASN.1 structure version number */
   ASN1_INTEGER_set(cs->cryptoData->version, BACULA_ASN1_VERSION);

   /*
    * Acquire a cipher instance and set the ASN.1 cipher NID
    */
   switch (cipher) {
   case CRYPTO_CIPHER_AES_128_CBC:
      /* AES 128 bit CBC */
      cs->cryptoData->contentEncryptionAlgorithm = OBJ_nid2obj(NID_aes_128_cbc);
      ec = EVP_aes_128_cbc();
      break;
#ifndef HAVE_OPENSSL_EXPORT_LIBRARY
   case CRYPTO_CIPHER_AES_192_CBC:
      /* AES 192 bit CBC */
      cs->cryptoData->contentEncryptionAlgorithm = OBJ_nid2obj(NID_aes_192_cbc);
      ec = EVP_aes_192_cbc();
      break;
   case CRYPTO_CIPHER_AES_256_CBC:
      /* AES 256 bit CBC */
      cs->cryptoData->contentEncryptionAlgorithm = OBJ_nid2obj(NID_aes_256_cbc);
      ec = EVP_aes_256_cbc();
      break;
#endif
   case CRYPTO_CIPHER_BLOWFISH_CBC:
      /* Blowfish CBC */
      cs->cryptoData->contentEncryptionAlgorithm = OBJ_nid2obj(NID_bf_cbc);
      ec = EVP_bf_cbc();
      break;
   default:
      Jmsg0(NULL, M_ERROR, 0, _("Unsupported cipher type specified\n"));
      crypto_session_free(cs);
      return NULL;
   }

   /* Generate a symmetric session key */
   cs->session_key_len = EVP_CIPHER_key_length(ec);
   cs->session_key = (unsigned char *) malloc(cs->session_key_len);
   if (RAND_bytes(cs->session_key, cs->session_key_len) <= 0) {
      /* OpenSSL failure */
      crypto_session_free(cs);
      return NULL;
   }

   /* Generate an IV if possible */
   if ((iv_len = EVP_CIPHER_iv_length(ec))) {
      iv = (unsigned char *)malloc(iv_len);

      /* Generate random IV */
      if (RAND_bytes(iv, iv_len) <= 0) {
         /* OpenSSL failure */
         crypto_session_free(cs);
         free(iv);
         return NULL;
      }

      /* Store it in our ASN.1 structure */
      if (!M_ASN1_OCTET_STRING_set(cs->cryptoData->iv, iv, iv_len)) {
         /* Allocation failed in OpenSSL */
         crypto_session_free(cs);
         free(iv);
         return NULL;
      }
      free(iv);
   }

   /*
    * Create RecipientInfo structures for supplied
    * public keys.
    */
   foreach_alist(keypair, pubkeys) {
      RecipientInfo *ri;
      unsigned char *ekey;
      int ekey_len;

      ri = RecipientInfo_new();
      if (!ri) {
         /* Allocation failed in OpenSSL */
         crypto_session_free(cs);
         return NULL;
      }

      /* Set the ASN.1 structure version number */
      ASN1_INTEGER_set(ri->version, BACULA_ASN1_VERSION);

      /* Drop the string allocated by OpenSSL, and add our subjectKeyIdentifier */
      M_ASN1_OCTET_STRING_free(ri->subjectKeyIdentifier);
      ri->subjectKeyIdentifier = M_ASN1_OCTET_STRING_dup(keypair->keyid);

      /* Set our key encryption algorithm. We currently require RSA */
      assert(keypair->pubkey && EVP_PKEY_type(keypair->pubkey->type) == EVP_PKEY_RSA);
      ri->keyEncryptionAlgorithm = OBJ_nid2obj(NID_rsaEncryption);

      /* Encrypt the session key */
      ekey = (unsigned char *)malloc(EVP_PKEY_size(keypair->pubkey));

      if ((ekey_len = EVP_PKEY_encrypt(ekey, cs->session_key, cs->session_key_len, keypair->pubkey)) <= 0) {
         /* OpenSSL failure */
         RecipientInfo_free(ri);
         crypto_session_free(cs);
         free(ekey);
         return NULL;
      }

      /* Store it in our ASN.1 structure */
      if (!M_ASN1_OCTET_STRING_set(ri->encryptedKey, ekey, ekey_len)) {
         /* Allocation failed in OpenSSL */
         RecipientInfo_free(ri);
         crypto_session_free(cs);
         free(ekey);
         return NULL;
      }

      /* Free the encrypted key buffer */
      free(ekey);

      /* Push the new RecipientInfo structure onto the stack */
      sk_RecipientInfo_push(cs->cryptoData->recipientInfo, ri);
   }

   return cs;
}

/*
 * Encodes the CryptoData structure. The length argument is used to specify the
 * size of dest. A length of 0 will cause no data to be written to dest, and the
 * required length to be written to length. The caller can then allocate sufficient
 * space for the output.
 *
 * Returns: true on success, stores the encoded data in dest, and the size in length.
 *          false on failure.
 */
bool crypto_session_encode(CRYPTO_SESSION *cs, uint8_t *dest, uint32_t *length)
{
   if (*length == 0) {
      *length = i2d_CryptoData(cs->cryptoData, NULL);
      return true;
   }

   *length = i2d_CryptoData(cs->cryptoData, &dest);
   return true;
}

/*
 * Decodes the CryptoData structure. The length argument is
 * used to specify the size of data.
 *
 * Returns: CRYPTO_SESSION instance on success.
 *          NULL on failure.
 * Returns: CRYPTO_ERROR_NONE and a pointer to a newly allocated CRYPTO_SESSION structure in *session on success.
 *          A crypto_error_t value on failure.
 */
crypto_error_t crypto_session_decode(const uint8_t *data, uint32_t length, alist *keypairs, CRYPTO_SESSION **session)
{
   CRYPTO_SESSION *cs;
   X509_KEYPAIR *keypair;
   STACK_OF(RecipientInfo) *recipients;
   crypto_error_t retval = CRYPTO_ERROR_NONE;
#if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
   const unsigned char *p = (const unsigned char *)data;
#else
   unsigned char *p = (unsigned char *)data;
#endif

   /* bacula-fd.conf doesn't contains any key */
   if (!keypairs) {
      return CRYPTO_ERROR_NORECIPIENT;
   }

   cs = (CRYPTO_SESSION *)malloc(sizeof(CRYPTO_SESSION));

   /* Initialize required fields */
   cs->session_key = NULL;

   /* d2i_CryptoData modifies the supplied pointer */
   cs->cryptoData = d2i_CryptoData(NULL, &p, length);

   if (!cs->cryptoData) {
      /* Allocation / Decoding failed in OpenSSL */
      openssl_post_errors(M_ERROR, _("CryptoData decoding failed"));
      retval = CRYPTO_ERROR_INTERNAL;
      goto err;
   }

   recipients = cs->cryptoData->recipientInfo;

   /*
    * Find a matching RecipientInfo structure for a supplied
    * public key
    */
   foreach_alist(keypair, keypairs) {
      RecipientInfo *ri;
      int i;

      /* Private key available? */
      if (keypair->privkey == NULL) {
         continue;
      }

      for (i = 0; i < sk_RecipientInfo_num(recipients); i++) {
         ri = sk_RecipientInfo_value(recipients, i);

         /* Match against the subjectKeyIdentifier */
         if (M_ASN1_OCTET_STRING_cmp(keypair->keyid, ri->subjectKeyIdentifier) == 0) {
            /* Match found, extract symmetric encryption session data */
            
            /* RSA is required. */
            assert(EVP_PKEY_type(keypair->privkey->type) == EVP_PKEY_RSA);

            /* If we recieve a RecipientInfo structure that does not use
             * RSA, return an error */
            if (OBJ_obj2nid(ri->keyEncryptionAlgorithm) != NID_rsaEncryption) {
               retval = CRYPTO_ERROR_INVALID_CRYPTO;
               goto err;
            }

            /* Decrypt the session key */
            /* Allocate sufficient space for the largest possible decrypted data */
            cs->session_key = (unsigned char *)malloc(EVP_PKEY_size(keypair->privkey));
            cs->session_key_len = EVP_PKEY_decrypt(cs->session_key, M_ASN1_STRING_data(ri->encryptedKey),
                                  M_ASN1_STRING_length(ri->encryptedKey), keypair->privkey);

            if (cs->session_key_len <= 0) {
               openssl_post_errors(M_ERROR, _("Failure decrypting the session key"));
               retval = CRYPTO_ERROR_DECRYPTION;
               goto err;
            }

            /* Session key successfully extracted, return the CRYPTO_SESSION structure */
            *session = cs;
            return CRYPTO_ERROR_NONE;
         }
      }
   }

   /* No matching recipient found */
   return CRYPTO_ERROR_NORECIPIENT;

err:
   crypto_session_free(cs);
   return retval;
}

/*
 * Free memory associated with a crypto session object.
 */
void crypto_session_free(CRYPTO_SESSION *cs)
{
   if (cs->cryptoData) {
      CryptoData_free(cs->cryptoData);
   }
   if (cs->session_key){
      free(cs->session_key);
   }
   free(cs);
}

/*
 * Create a new crypto cipher context with the specified session object
 *  Returns: A pointer to a CIPHER_CONTEXT object on success. The cipher block size is returned in blocksize.
 *           NULL on failure.
 */
CIPHER_CONTEXT *crypto_cipher_new(CRYPTO_SESSION *cs, bool encrypt, uint32_t *blocksize)
{
   CIPHER_CONTEXT *cipher_ctx;
   const EVP_CIPHER *ec;

   cipher_ctx = (CIPHER_CONTEXT *)malloc(sizeof(CIPHER_CONTEXT));

   /*
    * Acquire a cipher instance for the given ASN.1 cipher NID
    */
   if ((ec = EVP_get_cipherbyobj(cs->cryptoData->contentEncryptionAlgorithm)) == NULL) {
      Jmsg1(NULL, M_ERROR, 0, 
         _("Unsupported contentEncryptionAlgorithm: %d\n"), OBJ_obj2nid(cs->cryptoData->contentEncryptionAlgorithm));
      free(cipher_ctx);
      return NULL;
   }

   /* Initialize the OpenSSL cipher context */
   EVP_CIPHER_CTX_init(&cipher_ctx->ctx);
   if (encrypt) {
      /* Initialize for encryption */
      if (!EVP_CipherInit_ex(&cipher_ctx->ctx, ec, NULL, NULL, NULL, 1)) {
         openssl_post_errors(M_ERROR, _("OpenSSL cipher context initialization failed"));
         goto err;
      }
   } else {
      /* Initialize for decryption */
      if (!EVP_CipherInit_ex(&cipher_ctx->ctx, ec, NULL, NULL, NULL, 0)) {
         openssl_post_errors(M_ERROR, _("OpenSSL cipher context initialization failed"));
         goto err;
      }
   }

   /* Set the key size */
   if (!EVP_CIPHER_CTX_set_key_length(&cipher_ctx->ctx, cs->session_key_len)) {
      openssl_post_errors(M_ERROR, _("Encryption session provided an invalid symmetric key"));
      goto err;
   }

   /* Validate the IV length */
   if (EVP_CIPHER_iv_length(ec) != M_ASN1_STRING_length(cs->cryptoData->iv)) {
      openssl_post_errors(M_ERROR, _("Encryption session provided an invalid IV"));
      goto err;
   }
   
   /* Add the key and IV to the cipher context */
   if (!EVP_CipherInit_ex(&cipher_ctx->ctx, NULL, NULL, cs->session_key, M_ASN1_STRING_data(cs->cryptoData->iv), -1)) {
      openssl_post_errors(M_ERROR, _("OpenSSL cipher context key/IV initialization failed"));
      goto err;
   }

   *blocksize = EVP_CIPHER_CTX_block_size(&cipher_ctx->ctx);
   return cipher_ctx;

err:
   crypto_cipher_free(cipher_ctx);
   return NULL;
}


/*
 * Encrypt/Decrypt length bytes of data using the provided cipher context
 * Returns: true on success, number of bytes output in written
 *          false on failure
 */
bool crypto_cipher_update(CIPHER_CONTEXT *cipher_ctx, const uint8_t *data, uint32_t length, const uint8_t *dest, uint32_t *written)
{
   if (!EVP_CipherUpdate(&cipher_ctx->ctx, (unsigned char *)dest, (int *)written, (const unsigned char *)data, length)) {
      /* This really shouldn't fail */
      return false;
   } else {
      return true;
   }
}

/*
 * Finalize the cipher context, writing any remaining data and necessary padding
 * to dest, and the size in written.
 * The result size will either be one block of data or zero.
 *
 * Returns: true on success
 *          false on failure
 */
bool crypto_cipher_finalize (CIPHER_CONTEXT *cipher_ctx, uint8_t *dest, uint32_t *written)
{
   if (!EVP_CipherFinal_ex(&cipher_ctx->ctx, (unsigned char *)dest, (int *) written)) {
      /* This really shouldn't fail */
      return false;
   } else {
      return true;
   }
}


/*
 * Free memory associated with a cipher context.
 */
void crypto_cipher_free (CIPHER_CONTEXT *cipher_ctx)
{
   EVP_CIPHER_CTX_cleanup(&cipher_ctx->ctx);
   free (cipher_ctx);
}


/*
 * Perform global initialization of OpenSSL
 * This function is not thread safe.
 *  Returns: 0 on success
 *           errno on failure
 */
int init_crypto (void)
{
   int stat;

   if ((stat = openssl_init_threads()) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, 
        _("Unable to init OpenSSL threading: ERR=%s\n"), be.bstrerror(stat));
   }

   /* Load libssl and libcrypto human-readable error strings */
   SSL_load_error_strings();

   /* Initialize OpenSSL SSL  library */
   SSL_library_init();

   /* Register OpenSSL ciphers and digests */
   OpenSSL_add_all_algorithms();

   if (!openssl_seed_prng()) {
      Jmsg0(NULL, M_ERROR_TERM, 0, _("Failed to seed OpenSSL PRNG\n"));
   }

   crypto_initialized = true;

   return stat;
}

/*
 * Perform global cleanup of OpenSSL
 * All cryptographic operations must be completed before calling this function.
 * This function is not thread safe.
 *  Returns: 0 on success
 *           errno on failure
 */
int cleanup_crypto (void)
{
   /*
    * Ensure that we've actually been initialized; Doing this here decreases the
    * complexity of client's termination/cleanup code.
    */
   if (!crypto_initialized) {
      return 0;
   }

   if (!openssl_save_prng()) {
      Jmsg0(NULL, M_ERROR, 0, _("Failed to save OpenSSL PRNG\n"));
   }

   openssl_cleanup_threads();

   /* Free libssl and libcrypto error strings */
   ERR_free_strings();

   /* Free all ciphers and digests */
   EVP_cleanup();

   /* Free memory used by PRNG */
   RAND_cleanup();

   crypto_initialized = false;

   return 0;
}


#else /* HAVE_OPENSSL */
# error No encryption library available
#endif /* HAVE_OPENSSL */

#else /* HAVE_CRYPTO */

/*
 * Cryptography Support Disabled
 */

/* Message Digest Structure */
struct Digest {
   crypto_digest_t type;
   JCR *jcr;
   union {
      SHA1Context sha1;
      MD5Context md5;
   };
};

/* Dummy Signature Structure */
struct Signature {
   JCR *jcr;
};

DIGEST *crypto_digest_new(JCR *jcr, crypto_digest_t type)
{
   DIGEST *digest;

   digest = (DIGEST *)malloc(sizeof(DIGEST));
   digest->type = type;
   digest->jcr = jcr;

   switch (type) {
   case CRYPTO_DIGEST_MD5:
      MD5Init(&digest->md5);
      break;
   case CRYPTO_DIGEST_SHA1:
      SHA1Init(&digest->sha1);
      break;
   default:
      Jmsg1(jcr, M_ERROR, 0, _("Unsupported digest type=%d specified\n"), type);
      free(digest);
      return NULL;
   }

   return (digest);
}

bool crypto_digest_update(DIGEST *digest, const uint8_t *data, uint32_t length)
{
   switch (digest->type) {
   case CRYPTO_DIGEST_MD5:
      /* Doesn't return anything ... */
      MD5Update(&digest->md5, (unsigned char *) data, length);
      return true;
   case CRYPTO_DIGEST_SHA1:
      int ret;
      if ((ret = SHA1Update(&digest->sha1, (const u_int8_t *) data, length)) == shaSuccess) {
         return true;
      } else {
         Jmsg1(NULL, M_ERROR, 0, _("SHA1Update() returned an error: %d\n"), ret);
         return false;
      }
      break;
   default:
      return false;
   }
}

bool crypto_digest_finalize(DIGEST *digest, uint8_t *dest, uint32_t *length) 
{
   switch (digest->type) {
   case CRYPTO_DIGEST_MD5:
      /* Guard against programmer error by either the API client or
       * an out-of-sync CRYPTO_DIGEST_MAX_SIZE */
      assert(*length >= CRYPTO_DIGEST_MD5_SIZE);
      *length = CRYPTO_DIGEST_MD5_SIZE;
      /* Doesn't return anything ... */
      MD5Final((unsigned char *)dest, &digest->md5);
      return true;
   case CRYPTO_DIGEST_SHA1:
      /* Guard against programmer error by either the API client or
       * an out-of-sync CRYPTO_DIGEST_MAX_SIZE */
      assert(*length >= CRYPTO_DIGEST_SHA1_SIZE);
      *length = CRYPTO_DIGEST_SHA1_SIZE;
      if (SHA1Final(&digest->sha1, (u_int8_t *) dest) == shaSuccess) {
         return true;
      } else {
         return false;
      }
      break;
   default:
      return false;
   }

   return false;
}

void crypto_digest_free(DIGEST *digest)
{
   free(digest);
}

/* Dummy routines */
int init_crypto (void) { return 0; }
int cleanup_crypto (void) { return 0; }

SIGNATURE *crypto_sign_new(JCR *jcr) { return NULL; }

crypto_error_t crypto_sign_get_digest (SIGNATURE *sig, X509_KEYPAIR *keypair, 
                                       crypto_digest_t &type, DIGEST **digest) 
   { return CRYPTO_ERROR_INTERNAL; }

crypto_error_t crypto_sign_verify (SIGNATURE *sig, X509_KEYPAIR *keypair, DIGEST *digest) { return CRYPTO_ERROR_INTERNAL; }

int crypto_sign_add_signer (SIGNATURE *sig, DIGEST *digest, X509_KEYPAIR *keypair) { return false; }
int crypto_sign_encode (SIGNATURE *sig, uint8_t *dest, uint32_t *length) { return false; }

SIGNATURE *crypto_sign_decode (JCR *jcr, const uint8_t *sigData, uint32_t length) { return NULL; }
void crypto_sign_free (SIGNATURE *sig) { }


X509_KEYPAIR *crypto_keypair_new(void) { return NULL; }
X509_KEYPAIR *crypto_keypair_dup (X509_KEYPAIR *keypair) { return NULL; }
int crypto_keypair_load_cert (X509_KEYPAIR *keypair, const char *file) { return false; }
bool crypto_keypair_has_key (const char *file) { return false; }
int crypto_keypair_load_key (X509_KEYPAIR *keypair, const char *file, CRYPTO_PEM_PASSWD_CB *pem_callback, const void *pem_userdata) { return false; }
void crypto_keypair_free (X509_KEYPAIR *keypair) { }

CRYPTO_SESSION *crypto_session_new (crypto_cipher_t cipher, alist *pubkeys) { return NULL; }
void crypto_session_free (CRYPTO_SESSION *cs) { }
bool crypto_session_encode (CRYPTO_SESSION *cs, uint8_t *dest, uint32_t *length) { return false; }
crypto_error_t crypto_session_decode(const uint8_t *data, uint32_t length, alist *keypairs, CRYPTO_SESSION **session) { return CRYPTO_ERROR_INTERNAL; }

CIPHER_CONTEXT *crypto_cipher_new (CRYPTO_SESSION *cs, bool encrypt, uint32_t *blocksize) { return NULL; }
bool crypto_cipher_update (CIPHER_CONTEXT *cipher_ctx, const uint8_t *data, uint32_t length, const uint8_t *dest, uint32_t *written) { return false; }
bool crypto_cipher_finalize (CIPHER_CONTEXT *cipher_ctx, uint8_t *dest, uint32_t *written) { return false; }
void crypto_cipher_free (CIPHER_CONTEXT *cipher_ctx) { }

#endif /* HAVE_CRYPTO */

/* Shared Code */

/*
 * Default PEM encryption passphrase callback.
 * Returns an empty password.
 */
int crypto_default_pem_callback(char *buf, int size, const void *userdata)
{
   bstrncpy(buf, "", size);
   return (strlen(buf));
}

/*
 * Returns the ASCII name of the digest type.
 * Returns: ASCII name of digest type.
 */
const char *crypto_digest_name(DIGEST *digest) 
{
   switch (digest->type) {
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
crypto_digest_t crypto_digest_stream_type(int stream)
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
const char *crypto_strerror(crypto_error_t error) {
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
