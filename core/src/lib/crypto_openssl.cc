/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
 * crypto_openssl.c Encryption support functions when OPENSSL backend.
 *
 * Author: Landon Fuller <landonf@opendarwin.org>
 */

#include "include/bareos.h"
#include "lib/berrno.h"
#include "lib/crypto.h"
#include "lib/crypto_openssl.h"

#if defined(HAVE_OPENSSL)

#  include "include/allow_deprecated.h"
#  include <openssl/err.h>
#  include <openssl/rand.h>

#  if defined(HAVE_CRYPTO)

#    include "jcr.h"
#    include <assert.h>
#    include "lib/alist.h"
#    include <openssl/ssl.h>
#    include <openssl/x509v3.h>
#    include <openssl/asn1.h>
#    include <openssl/asn1t.h>
#    include <openssl/engine.h>
#    include <openssl/evp.h>
#    include <iomanip>
#    include <sstream>


/*
 * For OpenSSL version 1.x, EVP_PKEY_encrypt no longer exists. It was not an
 * official API.
 */
#    if (OPENSSL_VERSION_NUMBER >= 0x10000000L)
#      define EVP_PKEY_encrypt EVP_PKEY_encrypt_old
#      define EVP_PKEY_decrypt EVP_PKEY_decrypt_old
#    endif

/*
 * Sanity checks.
 *
 * Various places in the bareos source code define arrays of size
 * CRYPTO_DIGEST_MAX_SIZE. Make sure this is large enough for all EVP digest
 * routines supported by openssl.
 */
#    if (EVP_MAX_MD_SIZE > CRYPTO_DIGEST_MAX_SIZE)
#      error \
          "EVP_MAX_MD_SIZE > CRYPTO_DIGEST_MAX_SIZE, please update src/lib/crypto.h"
#    endif

#    if (EVP_MAX_BLOCK_LENGTH != CRYPTO_CIPHER_MAX_BLOCK_SIZE)
#      error \
          "EVP_MAX_BLOCK_LENGTH != CRYPTO_CIPHER_MAX_BLOCK_SIZE, please update src/lib/crypto.h"
#    endif

/* ASN.1 Declarations */
#    define BAREOS_ASN1_VERSION 0

typedef struct {
  ASN1_INTEGER* version;
  ASN1_OCTET_STRING* subjectKeyIdentifier;
  ASN1_OBJECT* digestAlgorithm;
  ASN1_OBJECT* signatureAlgorithm;
  ASN1_OCTET_STRING* signature;
} SignerInfo;

typedef struct {
  ASN1_INTEGER* version;
  ASN1_OCTET_STRING* subjectKeyIdentifier;
  ASN1_OBJECT* keyEncryptionAlgorithm;
  ASN1_OCTET_STRING* encryptedKey;
} RecipientInfo;

ASN1_SEQUENCE(SignerInfo)
    = {ASN1_SIMPLE(SignerInfo, version, ASN1_INTEGER),
       ASN1_SIMPLE(SignerInfo, subjectKeyIdentifier, ASN1_OCTET_STRING),
       ASN1_SIMPLE(SignerInfo, digestAlgorithm, ASN1_OBJECT),
       ASN1_SIMPLE(SignerInfo, signatureAlgorithm, ASN1_OBJECT),
       ASN1_SIMPLE(SignerInfo,
                   signature,
                   ASN1_OCTET_STRING)} ASN1_SEQUENCE_END(SignerInfo);

ASN1_SEQUENCE(RecipientInfo) = {
    ASN1_SIMPLE(RecipientInfo, version, ASN1_INTEGER),
    ASN1_SIMPLE(RecipientInfo, subjectKeyIdentifier, ASN1_OCTET_STRING),
    ASN1_SIMPLE(RecipientInfo, keyEncryptionAlgorithm, ASN1_OBJECT),
    ASN1_SIMPLE(RecipientInfo, encryptedKey, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(RecipientInfo);

typedef struct {
  ASN1_INTEGER* version;
  STACK_OF(SignerInfo) * signerInfo;
} SignatureData;

typedef struct {
  ASN1_INTEGER* version;
  ASN1_OBJECT* contentEncryptionAlgorithm;
  ASN1_OCTET_STRING* iv;
  STACK_OF(RecipientInfo) * recipientInfo;
} CryptoData;

ASN1_SEQUENCE(SignatureData) = {
    ASN1_SIMPLE(SignatureData, version, ASN1_INTEGER),
    ASN1_SET_OF(SignatureData, signerInfo, SignerInfo),
} ASN1_SEQUENCE_END(SignatureData);

ASN1_SEQUENCE(CryptoData)
    = {ASN1_SIMPLE(CryptoData, version, ASN1_INTEGER),
       ASN1_SIMPLE(CryptoData, contentEncryptionAlgorithm, ASN1_OBJECT),
       ASN1_SIMPLE(CryptoData, iv, ASN1_OCTET_STRING),
       ASN1_SET_OF(CryptoData,
                   recipientInfo,
                   RecipientInfo)} ASN1_SEQUENCE_END(CryptoData);

IMPLEMENT_ASN1_FUNCTIONS(SignerInfo)
IMPLEMENT_ASN1_FUNCTIONS(RecipientInfo)
IMPLEMENT_ASN1_FUNCTIONS(SignatureData)
IMPLEMENT_ASN1_FUNCTIONS(CryptoData)

#    if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
/* Openssl Version < 1.1 */

#      define OBJ_get0_data(o) ((o)->data)
#      define OBJ_length(o) ((o)->length)

#      define X509_EXTENSION_get_data(ext) ((ext)->value)
#      define EVP_PKEY_id(key) ((key)->type)

#      define EVP_PKEY_up_ref(x509_key) \
        CRYPTO_add(&(x509_key->references), 1, CRYPTO_LOCK_EVP_PKEY)

IMPLEMENT_STACK_OF(SignerInfo)
IMPLEMENT_STACK_OF(RecipientInfo)

/*
 * SignerInfo and RecipientInfo stack macros, generated by OpenSSL's
 * util/mkstack.pl.
 */
#      define sk_SignerInfo_new(st) SKM_sk_new(SignerInfo, (st))
#      define sk_SignerInfo_new_null() SKM_sk_new_null(SignerInfo)
#      define sk_SignerInfo_free(st) SKM_sk_free(SignerInfo, (st))
#      define sk_SignerInfo_num(st) SKM_sk_num(SignerInfo, (st))
#      define sk_SignerInfo_value(st, i) SKM_sk_value(SignerInfo, (st), (i))
#      define sk_SignerInfo_set(st, i, val) \
        SKM_sk_set(SignerInfo, (st), (i), (val))
#      define sk_SignerInfo_zero(st) SKM_sk_zero(SignerInfo, (st))
#      define sk_SignerInfo_push(st, val) SKM_sk_push(SignerInfo, (st), (val))
#      define sk_SignerInfo_unshift(st, val) \
        SKM_sk_unshift(SignerInfo, (st), (val))
#      define sk_SignerInfo_find(st, val) SKM_sk_find(SignerInfo, (st), (val))
#      define sk_SignerInfo_delete(st, i) SKM_sk_delete(SignerInfo, (st), (i))
#      define sk_SignerInfo_delete_ptr(st, ptr) \
        SKM_sk_delete_ptr(SignerInfo, (st), (ptr))
#      define sk_SignerInfo_insert(st, val, i) \
        SKM_sk_insert(SignerInfo, (st), (val), (i))
#      define sk_SignerInfo_set_cmp_func(st, cmp) \
        SKM_sk_set_cmp_func(SignerInfo, (st), (cmp))
#      define sk_SignerInfo_dup(st) SKM_sk_dup(SignerInfo, st)
#      define sk_SignerInfo_pop_free(st, free_func) \
        SKM_sk_pop_free(SignerInfo, (st), (free_func))
#      define sk_SignerInfo_shift(st) SKM_sk_shift(SignerInfo, (st))
#      define sk_SignerInfo_pop(st) SKM_sk_pop(SignerInfo, (st))
#      define sk_SignerInfo_sort(st) SKM_sk_sort(SignerInfo, (st))
#      define sk_SignerInfo_is_sorted(st) SKM_sk_is_sorted(SignerInfo, (st))

#      define sk_RecipientInfo_new(st) SKM_sk_new(RecipientInfo, (st))
#      define sk_RecipientInfo_new_null() SKM_sk_new_null(RecipientInfo)
#      define sk_RecipientInfo_free(st) SKM_sk_free(RecipientInfo, (st))
#      define sk_RecipientInfo_num(st) SKM_sk_num(RecipientInfo, (st))
#      define sk_RecipientInfo_value(st, i) \
        SKM_sk_value(RecipientInfo, (st), (i))
#      define sk_RecipientInfo_set(st, i, val) \
        SKM_sk_set(RecipientInfo, (st), (i), (val))
#      define sk_RecipientInfo_zero(st) SKM_sk_zero(RecipientInfo, (st))
#      define sk_RecipientInfo_push(st, val) \
        SKM_sk_push(RecipientInfo, (st), (val))
#      define sk_RecipientInfo_unshift(st, val) \
        SKM_sk_unshift(RecipientInfo, (st), (val))
#      define sk_RecipientInfo_find(st, val) \
        SKM_sk_find(RecipientInfo, (st), (val))
#      define sk_RecipientInfo_delete(st, i) \
        SKM_sk_delete(RecipientInfo, (st), (i))
#      define sk_RecipientInfo_delete_ptr(st, ptr) \
        SKM_sk_delete_ptr(RecipientInfo, (st), (ptr))
#      define sk_RecipientInfo_insert(st, val, i) \
        SKM_sk_insert(RecipientInfo, (st), (val), (i))
#      define sk_RecipientInfo_set_cmp_func(st, cmp) \
        SKM_sk_set_cmp_func(RecipientInfo, (st), (cmp))
#      define sk_RecipientInfo_dup(st) SKM_sk_dup(RecipientInfo, st)
#      define sk_RecipientInfo_pop_free(st, free_func) \
        SKM_sk_pop_free(RecipientInfo, (st), (free_func))
#      define sk_RecipientInfo_shift(st) SKM_sk_shift(RecipientInfo, (st))
#      define sk_RecipientInfo_pop(st) SKM_sk_pop(RecipientInfo, (st))
#      define sk_RecipientInfo_sort(st) SKM_sk_sort(RecipientInfo, (st))
#      define sk_RecipientInfo_is_sorted(st) \
        SKM_sk_is_sorted(RecipientInfo, (st))

#    else
/* Openssl Version >= 1.1 */

/* ignore the suggest-override warnings caused by following DEFINE_STACK_OF() */
#      pragma GCC diagnostic push
#      pragma GCC diagnostic ignored "-Wunused-function"
DEFINE_STACK_OF(SignerInfo)
DEFINE_STACK_OF(RecipientInfo)
#      pragma GCC diagnostic pop


#      define M_ASN1_OCTET_STRING_free(a) ASN1_STRING_free((ASN1_STRING*)a)
#      define M_ASN1_OCTET_STRING_cmp(a, b) \
        ASN1_STRING_cmp((const ASN1_STRING*)a, (const ASN1_STRING*)b)
#      define M_ASN1_OCTET_STRING_dup(a) \
        (ASN1_OCTET_STRING*)ASN1_STRING_dup((const ASN1_STRING*)a)
#      define M_ASN1_OCTET_STRING_set(a, b, c) \
        ASN1_STRING_set((ASN1_STRING*)a, b, c)

#      define M_ASN1_STRING_length(x) ((x)->length)
#      define M_ASN1_STRING_data(x) ((x)->data)

#    endif

#    define d2i_ASN1_SET_OF_SignerInfo(st, pp, length, d2i_func, free_func, \
                                       ex_tag, ex_class)                    \
      SKM_ASN1_SET_OF_d2i(SignerInfo, (st), (pp), (length), (d2i_func),     \
                          (free_func), (ex_tag), (ex_class))
#    define i2d_ASN1_SET_OF_SignerInfo(st, pp, i2d_func, ex_tag, ex_class, \
                                       is_set)                             \
      SKM_ASN1_SET_OF_i2d(SignerInfo, (st), (pp), (i2d_func), (ex_tag),    \
                          (ex_class), (is_set))
#    define ASN1_seq_pack_SignerInfo(st, i2d_func, buf, len) \
      SKM_ASN1_seq_pack(SignerInfo, (st), (i2d_func), (buf), (len))
#    define ASN1_seq_unpack_SignerInfo(buf, len, d2i_func, free_func) \
      SKM_ASN1_seq_unpack(SignerInfo, (buf), (len), (d2i_func), (free_func))

#    define d2i_ASN1_SET_OF_RecipientInfo(st, pp, length, d2i_func, free_func, \
                                          ex_tag, ex_class)                    \
      SKM_ASN1_SET_OF_d2i(RecipientInfo, (st), (pp), (length), (d2i_func),     \
                          (free_func), (ex_tag), (ex_class))
#    define i2d_ASN1_SET_OF_RecipientInfo(st, pp, i2d_func, ex_tag, ex_class, \
                                          is_set)                             \
      SKM_ASN1_SET_OF_i2d(RecipientInfo, (st), (pp), (i2d_func), (ex_tag),    \
                          (ex_class), (is_set))
#    define ASN1_seq_pack_RecipientInfo(st, i2d_func, buf, len) \
      SKM_ASN1_seq_pack(RecipientInfo, (st), (i2d_func), (buf), (len))
#    define ASN1_seq_unpack_RecipientInfo(buf, len, d2i_func, free_func) \
      SKM_ASN1_seq_unpack(RecipientInfo, (buf), (len), (d2i_func), (free_func))
/* End of util/mkstack.pl block */

/* X509 Public/Private Key Pair Structure */
struct X509_Keypair {
  ASN1_OCTET_STRING* keyid;
  EVP_PKEY* pubkey;
  EVP_PKEY* privkey;
};

/* Message Digest Structure */
struct EvpDigest : public Digest {
#    if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
  /* Openssl Version < 1.1 */
 private:
  EVP_MD_CTX ctx;

 public:
  EvpDigest(JobControlRecord* jcr, crypto_digest_t type, const EVP_MD* md)
      : Digest(jcr, type)
  {
    EVP_MD_CTX_init(&ctx);
    if (EVP_DigestInit_ex(&ctx, md, NULL) == 0) { throw DigestInitException{}; }
  }

  virtual ~EvpDigest() { EVP_MD_CTX_cleanup(&ctx); }


  EVP_MD_CTX& get_ctx() { return ctx; }
#    else
  /* Openssl Version >= 1.1 */

 private:
  EVP_MD_CTX* ctx;

 public:
  EVP_MD_CTX& get_ctx() { return *ctx; }

  EvpDigest(JobControlRecord* jcr, crypto_digest_t type, const EVP_MD* md)
      : Digest(jcr, type)
  {
    ctx = EVP_MD_CTX_new();
    EVP_MD_CTX_init(ctx);
    if (EVP_DigestInit_ex(ctx, md, NULL) == 0) { throw DigestInitException{}; }
  }

  virtual ~EvpDigest()
  {
    EVP_MD_CTX_free(ctx);
    ctx = NULL;
  }
#    endif

  virtual bool Update(const uint8_t* data, uint32_t length) override;
  virtual bool Finalize(uint8_t* data, uint32_t* length) override;
};


/* Message Signature Structure */
struct Signature {
  SignatureData* sigData;
  JobControlRecord* jcr;
};

/* Encryption Session Data */
struct Crypto_Session {
  CryptoData* cryptoData;     /* ASN.1 Structure */
  unsigned char* session_key; /* Private symmetric session key */
  size_t session_key_len;     /* Symmetric session key length */
};

/* Symmetric Cipher Context */
struct Cipher_Context {
  EVP_CIPHER_CTX* ctx;

  Cipher_Context() { ctx = EVP_CIPHER_CTX_new(); }

  ~Cipher_Context() { EVP_CIPHER_CTX_free(ctx); }
};

/* PEM Password Dispatch Context */
typedef struct PEM_CB_Context {
  CRYPTO_PEM_PASSWD_CB* pem_callback;
  const void* pem_userdata;
} PEM_CB_CONTEXT;

/*
 * Extract subjectKeyIdentifier from x509 certificate.
 * Returns: On success, an ASN1_OCTET_STRING that must be freed via
 * M_ASN1_OCTET_STRING_free(). NULL on failure.
 */
static ASN1_OCTET_STRING* openssl_cert_keyid(X509* cert)
{
  X509_EXTENSION* ext = NULL;
  const X509V3_EXT_METHOD* method;
  ASN1_OCTET_STRING* keyid;
  int i;
#    if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
  const unsigned char* ext_value_data;
#    else
  unsigned char* ext_value_data;
#    endif


  /* Find the index to the subjectKeyIdentifier extension */
  i = X509_get_ext_by_NID(cert, NID_subject_key_identifier, -1);
  if (i < 0) {
    /* Not found */
    return NULL;
  }

  /* Grab the extension */
  ext = X509_get_ext(cert, i);

  /* Get x509 extension method structure */
  if (!(method = X509V3_EXT_get(ext))) { return NULL; }

  ext_value_data = X509_EXTENSION_get_data(ext)->data;

#    if (OPENSSL_VERSION_NUMBER > 0x00907000L)
  if (method->it) {
    /* New style ASN1 */

    /* Decode ASN1 item in data */
    keyid = (ASN1_OCTET_STRING*)ASN1_item_d2i(
        NULL, &ext_value_data, X509_EXTENSION_get_data(ext)->length,
        ASN1_ITEM_ptr(method->it));
  } else {
    /* Old style ASN1 */

    /* Decode ASN1 item in data */
    keyid = (ASN1_OCTET_STRING*)method->d2i(
        NULL, &ext_value_data, X509_EXTENSION_get_data(ext)->length);
  }

#    else
  keyid = (ASN1_OCTET_STRING*)method->d2i(NULL, &ext_value_data,
                                          X509_EXTENSION_get_data(ext)->length);
#    endif

  return keyid;
}

/*
 * Create a new keypair object.
 *  Returns: A pointer to a X509 KEYPAIR object on success.
 *           NULL on failure.
 */
X509_KEYPAIR* crypto_keypair_new(void)
{
  X509_KEYPAIR* keypair;

  /* Allocate our keypair structure */
  keypair = (X509_KEYPAIR*)malloc(sizeof(X509_KEYPAIR));

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

X509_KEYPAIR* crypto_keypair_dup(X509_KEYPAIR* keypair)
{
  X509_KEYPAIR* newpair;

  newpair = crypto_keypair_new();

  if (!newpair) {
    /* Allocation failed */
    return NULL;
  }

  /* Increment the public key ref count */
  if (keypair->pubkey) {
    EVP_PKEY_up_ref(keypair->pubkey);
    newpair->pubkey = keypair->pubkey;
  }

  /* Increment the private key ref count */
  if (keypair->privkey) {
    EVP_PKEY_up_ref(keypair->privkey);
    newpair->privkey = keypair->privkey;
  }

  /* Duplicate the keyid */
  if (keypair->keyid) {
    newpair->keyid = M_ASN1_OCTET_STRING_dup(keypair->keyid);
    if (!newpair->keyid) {
      /* Allocation failed */
      CryptoKeypairFree(newpair);
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
int CryptoKeypairLoadCert(X509_KEYPAIR* keypair, const char* file)
{
  BIO* bio;
  X509* cert;

  /* Open the file */
  if (!(bio = BIO_new_file(file, "r"))) {
    OpensslPostErrors(M_ERROR, T_("Unable to open certificate file"));
    return false;
  }

  cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
  BIO_free(bio);
  if (!cert) {
    OpensslPostErrors(M_ERROR, T_("Unable to read certificate from file"));
    return false;
  }

  /* Extract the public key */
  if (!(keypair->pubkey = X509_get_pubkey(cert))) {
    OpensslPostErrors(M_ERROR,
                      T_("Unable to extract public key from certificate"));
    goto err;
  }

  /* Extract the subjectKeyIdentifier extension field */
  if ((keypair->keyid = openssl_cert_keyid(cert)) == NULL) {
    Jmsg0(NULL, M_ERROR, 0,
          T_("Provided certificate does not include the required "
            "subjectKeyIdentifier extension."));
    goto err;
  }

  /* Validate the public key type (only RSA is supported) */
  if (EVP_PKEY_type(EVP_PKEY_id(keypair->pubkey)) != EVP_PKEY_RSA) {
    Jmsg1(NULL, M_ERROR, 0, T_("Unsupported key type provided: %d\n"),
          EVP_PKEY_type(EVP_PKEY_id(keypair->pubkey)));
    goto err;
  }

  X509_free(cert);
  return true;

err:
  X509_free(cert);
  if (keypair->pubkey) { EVP_PKEY_free(keypair->pubkey); }
  return false;
}

/* Dispatch user PEM encryption callbacks */
static int CryptoPemCallbackDispatch(char* buf, int size, int, void* userdata)
{
  PEM_CB_CONTEXT* ctx = (PEM_CB_CONTEXT*)userdata;
  return (ctx->pem_callback(buf, size, ctx->pem_userdata));
}

/*
 * Check a PEM-encoded file
 * for the existence of a private key.
 * Returns: true if a private key is found
 *          false otherwise
 */
bool CryptoKeypairHasKey(const char* file)
{
  BIO* bio;
  char* name = NULL;
  char* header = NULL;
  unsigned char* data = NULL;
  bool retval = false;
  long len;

  if (!(bio = BIO_new_file(file, "r"))) {
    OpensslPostErrors(M_ERROR, T_("Unable to open private key file"));
    return false;
  }

  while (PEM_read_bio(bio, &name, &header, &data, &len)) {
    /* We don't care what the data is, just that it's there */
    OPENSSL_free(header);
    OPENSSL_free(data);

    /* PEM Header Found, check for a private key
     * Due to OpenSSL limitations, we must specifically
     * list supported PEM private key encodings. */
    if (bstrcmp(name, PEM_STRING_RSA) || bstrcmp(name, PEM_STRING_DSA)
        || bstrcmp(name, PEM_STRING_PKCS8)
        || bstrcmp(name, PEM_STRING_PKCS8INF)) {
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
  OpensslPostErrors(M_ERROR, T_("Unable to read private key from file"));
  return retval;
}

/*
 * Load a PEM-encoded private key.
 *  Returns: true on success
 *           false on failure
 */
int CryptoKeypairLoadKey(X509_KEYPAIR* keypair,
                         const char* file,
                         CRYPTO_PEM_PASSWD_CB* pem_callback,
                         const void* pem_userdata)
{
  BIO* bio;
  PEM_CB_CONTEXT ctx;

  /* Open the file */
  if (!(bio = BIO_new_file(file, "r"))) {
    OpensslPostErrors(M_ERROR, T_("Unable to open private key file"));
    return false;
  }

  /* Set up PEM encryption callback */
  if (pem_callback) {
    ctx.pem_callback = pem_callback;
    ctx.pem_userdata = pem_userdata;
  } else {
    ctx.pem_callback = CryptoDefaultPemCallback;
    ctx.pem_userdata = NULL;
  }

  keypair->privkey
      = PEM_read_bio_PrivateKey(bio, NULL, CryptoPemCallbackDispatch, &ctx);
  BIO_free(bio);
  if (!keypair->privkey) {
    OpensslPostErrors(M_ERROR, T_("Unable to read private key from file"));
    return false;
  }

  return true;
}

// Free memory associated with a keypair object.
void CryptoKeypairFree(X509_KEYPAIR* keypair)
{
  if (keypair->pubkey) { EVP_PKEY_free(keypair->pubkey); }
  if (keypair->privkey) { EVP_PKEY_free(keypair->privkey); }
  if (keypair->keyid) { M_ASN1_OCTET_STRING_free(keypair->keyid); }
  free(keypair);
}


/*
 * Create a new message digest context of the specified type
 *  Returns: A pointer to a DIGEST object on success.
 *           NULL on failure.
 */
DIGEST* OpensslDigestNew(JobControlRecord* jcr, crypto_digest_t type)
{
  Dmsg1(150, "crypto_digest_new jcr=%p\n", jcr);

  try {
    /* Determine the correct OpenSSL message digest type */
    switch (type) {
      case CRYPTO_DIGEST_MD5:
        // Solaris deprecates use of md5 in OpenSSL
        ALLOW_DEPRECATED(return new EvpDigest(jcr, type, EVP_md5());)
        break;
      case CRYPTO_DIGEST_SHA1:
        return new EvpDigest(jcr, type, EVP_sha1());
#    ifdef HAVE_SHA2
      case CRYPTO_DIGEST_SHA256:
        return new EvpDigest(jcr, type, EVP_sha256());
      case CRYPTO_DIGEST_SHA512:
        return new EvpDigest(jcr, type, EVP_sha512());
#    endif
      default:
        Jmsg1(jcr, M_ERROR, 0, T_("Unsupported digest type: %d\n"), type);
        throw DigestInitException{};
    }
  } catch (const DigestInitException& e) {
    Dmsg0(150, "Digest init failed.\n");
    OpensslPostErrors(jcr, M_ERROR, T_("OpenSSL digest initialization failed"));
    return NULL;
  }
}

/*
 * Hash length bytes of data into the provided digest context.
 * Returns: true on success
 *          false on failure
 */
bool EvpDigest::Update(const uint8_t* data, uint32_t length)
{
  if (EVP_DigestUpdate(&get_ctx(), data, length) == 0) {
    Dmsg0(150, "digest update failed\n");
    OpensslPostErrors(jcr, M_ERROR, T_("OpenSSL digest update failed"));
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
bool EvpDigest::Finalize(uint8_t* dest, uint32_t* length)
{
  if (!EVP_DigestFinal(&get_ctx(), dest, (unsigned int*)length)) {
    Dmsg0(150, "digest finalize failed\n");
    OpensslPostErrors(jcr, M_ERROR, T_("OpenSSL digest finalize failed"));
    return false;
  } else {
    return true;
  }
}

// Free memory associated with a digest object.
void CryptoDigestFree(DIGEST* digest) { delete digest; }

/*
 * Create a new message signature context.
 *  Returns: A pointer to a SIGNATURE object on success.
 *           NULL on failure.
 */
SIGNATURE* crypto_sign_new(JobControlRecord* jcr)
{
  SIGNATURE* sig;

  sig = (SIGNATURE*)malloc(sizeof(SIGNATURE));
  if (!sig) { return NULL; }

  sig->sigData = SignatureData_new();
  sig->jcr = jcr;
  Dmsg1(150, "crypto_sign_new jcr=%p\n", jcr);

  if (!sig->sigData) {
    /* Allocation failed in OpenSSL */
    free(sig);
    return NULL;
  }

  /* Set the ASN.1 structure version number */
  ASN1_INTEGER_set(sig->sigData->version, BAREOS_ASN1_VERSION);

  return sig;
}

/*
 * For a given public key, find the associated SignatureInfo record
 *   and create a digest context for signature validation
 *
 * Returns: CRYPTO_ERROR_NONE on success, with the newly allocated DIGEST in
 * digest. A crypto_error_t value on failure.
 */
crypto_error_t CryptoSignGetDigest(SIGNATURE* sig,
                                   X509_KEYPAIR* keypair,
                                   crypto_digest_t& type,
                                   DIGEST** digest)
{
  STACK_OF(SignerInfo) * signers;
  SignerInfo* si;
  int i;

  signers = sig->sigData->signerInfo;

  for (i = 0; i < sk_SignerInfo_num(signers); i++) {
    si = sk_SignerInfo_value(signers, i);
    if (M_ASN1_OCTET_STRING_cmp(keypair->keyid, si->subjectKeyIdentifier)
        == 0) {
      /* Get the digest algorithm and allocate a digest context */
      Dmsg1(150, "CryptoSignGetDigest jcr=%p\n", sig->jcr);
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
#    ifdef HAVE_SHA2
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
#    endif
        default:
          type = CRYPTO_DIGEST_NONE;
          *digest = NULL;
          return CRYPTO_ERROR_INVALID_DIGEST;
      }

      /* Shouldn't happen */
      if (*digest == NULL) {
        OpensslPostErrors(sig->jcr, M_ERROR, T_("OpenSSL digest_new failed"));
        return CRYPTO_ERROR_INVALID_DIGEST;
      } else {
        return CRYPTO_ERROR_NONE;
      }
    } else {
      OpensslPostErrors(sig->jcr, M_ERROR, T_("OpenSSL sign get digest failed"));
    }
  }

  return CRYPTO_ERROR_NOSIGNER;
}

/*
 * For a given signature, public key, and digest, verify the SIGNATURE.
 * Returns: CRYPTO_ERROR_NONE on success.
 *          A crypto_error_t value on failure.
 */
crypto_error_t CryptoSignVerify(SIGNATURE* sig,
                                X509_KEYPAIR* keypair,
                                DIGEST* digest)
{
  STACK_OF(SignerInfo) * signers;
  SignerInfo* si;
  int ok, i;
  unsigned int sigLen;
#    if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
  const unsigned char* sigData;
#    else
  unsigned char* sigData;
#    endif

  signers = sig->sigData->signerInfo;

  /* Find the signer */
  for (i = 0; i < sk_SignerInfo_num(signers); i++) {
    si = sk_SignerInfo_value(signers, i);
    if (M_ASN1_OCTET_STRING_cmp(keypair->keyid, si->subjectKeyIdentifier)
        == 0) {
      /* Extract the signature data */
      sigLen = M_ASN1_STRING_length(si->signature);
      sigData = M_ASN1_STRING_data(si->signature);

      ok = EVP_VerifyFinal(&dynamic_cast<EvpDigest*>(digest)->get_ctx(),
                           sigData, sigLen, keypair->pubkey);
      if (ok >= 1) {
        return CRYPTO_ERROR_NONE;
      } else if (ok == 0) {
        OpensslPostErrors(sig->jcr, M_ERROR,
                          T_("OpenSSL digest Verify final failed"));
        return CRYPTO_ERROR_BAD_SIGNATURE;
      } else if (ok < 0) {
        /* Shouldn't happen */
        OpensslPostErrors(sig->jcr, M_ERROR,
                          T_("OpenSSL digest Verify final failed"));
        return CRYPTO_ERROR_INTERNAL;
      }
    }
  }
  Jmsg(sig->jcr, M_ERROR, 0, T_("No signers found for crypto verify.\n"));
  /* Signer wasn't found. */
  return CRYPTO_ERROR_NOSIGNER;
}


/*
 * Add a new signer
 *  Returns: true on success
 *           false on failure
 */
int CryptoSignAddSigner(SIGNATURE* sig, DIGEST* digest, X509_KEYPAIR* keypair)
{
  SignerInfo* si = NULL;
  unsigned char* buf = NULL;
  unsigned int len;

  si = SignerInfo_new();

  if (!si) {
    /* Allocation failed in OpenSSL */
    return false;
  }

  /* Set the ASN.1 structure version number */
  ASN1_INTEGER_set(si->version, BAREOS_ASN1_VERSION);

  /* Set the digest algorithm identifier */
  switch (digest->type) {
    case CRYPTO_DIGEST_MD5:
      si->digestAlgorithm = OBJ_nid2obj(NID_md5);
      break;
    case CRYPTO_DIGEST_SHA1:
      si->digestAlgorithm = OBJ_nid2obj(NID_sha1);
      break;
#    ifdef HAVE_SHA2
    case CRYPTO_DIGEST_SHA256:
      si->digestAlgorithm = OBJ_nid2obj(NID_sha256);
      break;
    case CRYPTO_DIGEST_SHA512:
      si->digestAlgorithm = OBJ_nid2obj(NID_sha512);
      break;
#    endif
    default:
      /* This should never happen */
      goto err;
  }

  /* Drop the string allocated by OpenSSL, and add our subjectKeyIdentifier */
  M_ASN1_OCTET_STRING_free(si->subjectKeyIdentifier);
  si->subjectKeyIdentifier = M_ASN1_OCTET_STRING_dup(keypair->keyid);


  /* Set our signature algorithm. We currently require RSA */
  assert(EVP_PKEY_type(EVP_PKEY_id(keypair->pubkey)) == EVP_PKEY_RSA);
  /* This is slightly evil. Reach into the BAREOS_LIB_MD_H_ structure and grab
   * the key type */
  {
    auto digest_ctx = &dynamic_cast<EvpDigest*>(digest)->get_ctx();
    si->signatureAlgorithm = OBJ_nid2obj(EVP_MD_CTX_type(digest_ctx));

    /* Finalize/Sign our Digest */
    len = EVP_PKEY_size(keypair->privkey);
    buf = (unsigned char*)malloc(len);
    if (!EVP_SignFinal(digest_ctx, buf, &len, keypair->privkey)) {
      OpensslPostErrors(M_ERROR, T_("Signature creation failed"));
      goto err;
    }
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
  if (si) { SignerInfo_free(si); }
  if (buf) { free(buf); }

  return false;
}

/*
 * Encodes the SignatureData structure. The length argument is used to specify
 * the size of dest. A length of 0 will cause no data to be written to dest, and
 * the required length to be written to length. The caller can then allocate
 * sufficient space for the output.
 *
 * Returns: true on success, stores the encoded data in dest, and the size in
 * length. false on failure.
 */
int CryptoSignEncode(SIGNATURE* sig, uint8_t* dest, uint32_t* length)
{
  if (*length == 0) {
    *length = i2d_SignatureData(sig->sigData, NULL);
    return true;
  }

  *length = i2d_SignatureData(sig->sigData, (unsigned char**)&dest);
  return true;
}

/*
 * Decodes the SignatureData structure. The length argument is used to specify
 the
 * size of sigData.
 *
 * Returns: SIGNATURE instance on success.
 *          NULL on failure.

 */

SIGNATURE* crypto_sign_decode(JobControlRecord* jcr,
                              const uint8_t* sigData,
                              uint32_t length)
{
  SIGNATURE* sig;
#    if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
  const unsigned char* p = (const unsigned char*)sigData;
#    else
  unsigned char* p = (unsigned char*)sigData;
#    endif

  sig = (SIGNATURE*)malloc(sizeof(SIGNATURE));
  if (!sig) { return NULL; }
  sig->jcr = jcr;

  /* d2i_SignatureData modifies the supplied pointer */
  sig->sigData = d2i_SignatureData(NULL, &p, length);

  if (!sig->sigData) {
    /* Allocation / Decoding failed in OpenSSL */
    OpensslPostErrors(jcr, M_ERROR, T_("Signature decoding failed"));
    free(sig);
    return NULL;
  }

  return sig;
}

// Free memory associated with a signature object.
void CryptoSignFree(SIGNATURE* sig)
{
  SignatureData_free(sig->sigData);
  free(sig);
}

/*
 * Create a new encryption session.
 *  Returns: A pointer to a CRYPTO_SESSION object on success.
 *           NULL on failure.
 *
 *  Note! BAREOS malloc() fails if out of memory.
 */
CRYPTO_SESSION* crypto_session_new(crypto_cipher_t cipher,
                                   alist<X509_KEYPAIR*>* pubkeys)
{
  CRYPTO_SESSION* cs;
  X509_KEYPAIR* keypair = nullptr;
  const EVP_CIPHER* ec;
  unsigned char* iv;
  int iv_len;

  /* Allocate our session description structures */
  cs = (CRYPTO_SESSION*)malloc(sizeof(CRYPTO_SESSION));

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
  ASN1_INTEGER_set(cs->cryptoData->version, BAREOS_ASN1_VERSION);

  // Acquire a cipher instance and set the ASN.1 cipher NID
  switch (cipher) {
    case CRYPTO_CIPHER_BLOWFISH_CBC:
      /* Blowfish CBC */
      cs->cryptoData->contentEncryptionAlgorithm = OBJ_nid2obj(NID_bf_cbc);
      ec = EVP_bf_cbc();
      break;
    case CRYPTO_CIPHER_3DES_CBC:
      /* 3DES CBC */
      cs->cryptoData->contentEncryptionAlgorithm
          = OBJ_nid2obj(NID_des_ede3_cbc);
      ec = EVP_des_ede3_cbc();
      break;
#    ifndef OPENSSL_NO_AES
    case CRYPTO_CIPHER_AES_128_CBC:
      /* AES 128 bit CBC */
      cs->cryptoData->contentEncryptionAlgorithm = OBJ_nid2obj(NID_aes_128_cbc);
      ec = EVP_aes_128_cbc();
      break;
#      ifndef HAVE_OPENSSL_EXPORT_LIBRARY
#        ifdef NID_aes_192_cbc
    case CRYPTO_CIPHER_AES_192_CBC:
      /* AES 192 bit CBC */
      cs->cryptoData->contentEncryptionAlgorithm = OBJ_nid2obj(NID_aes_192_cbc);
      ec = EVP_aes_192_cbc();
      break;
#        endif
#        ifdef NID_aes_256_cbc
    case CRYPTO_CIPHER_AES_256_CBC:
      /* AES 256 bit CBC */
      cs->cryptoData->contentEncryptionAlgorithm = OBJ_nid2obj(NID_aes_256_cbc);
      ec = EVP_aes_256_cbc();
      break;
#        endif
#      endif /* OPENSSL_NO_AES */
#      ifndef OPENSSL_NO_CAMELLIA
#        ifdef NID_camellia_128_cbc
    case CRYPTO_CIPHER_CAMELLIA_128_CBC:
      /* Camellia 128 bit CBC */
      cs->cryptoData->contentEncryptionAlgorithm
          = OBJ_nid2obj(NID_camellia_128_cbc);
      ec = EVP_camellia_128_cbc();
      break;
#        endif
#        ifdef NID_camellia_192_cbc
    case CRYPTO_CIPHER_CAMELLIA_192_CBC:
      /* Camellia 192 bit CBC */
      cs->cryptoData->contentEncryptionAlgorithm
          = OBJ_nid2obj(NID_camellia_192_cbc);
      ec = EVP_camellia_192_cbc();
      break;
#        endif
#        ifdef NID_camellia_256_cbc
    case CRYPTO_CIPHER_CAMELLIA_256_CBC:
      /* Camellia 256 bit CBC */
      cs->cryptoData->contentEncryptionAlgorithm
          = OBJ_nid2obj(NID_camellia_256_cbc);
      ec = EVP_camellia_256_cbc();
      break;
#        endif
#      endif /* OPENSSL_NO_CAMELLIA */
#    endif   /* HAVE_OPENSSL_EXPORT_LIBRARY */
#    if !defined(OPENSSL_NO_SHA) && !defined(OPENSSL_NO_SHA1)
#      ifdef NID_aes_128_cbc_hmac_sha1
    case CRYPTO_CIPHER_AES_128_CBC_HMAC_SHA1:
      /* AES 128 bit CBC HMAC SHA1 */
      cs->cryptoData->contentEncryptionAlgorithm
          = OBJ_nid2obj(NID_aes_128_cbc_hmac_sha1);
      ec = EVP_aes_128_cbc_hmac_sha1();
      break;
#      endif
#      ifdef NID_aes_256_cbc_hmac_sha1
    case CRYPTO_CIPHER_AES_256_CBC_HMAC_SHA1:
      /* AES 256 bit CBC HMAC SHA1 */
      cs->cryptoData->contentEncryptionAlgorithm
          = OBJ_nid2obj(NID_aes_256_cbc_hmac_sha1);
      ec = EVP_aes_256_cbc_hmac_sha1();
      break;
#      endif
#    endif /* !OPENSSL_NO_SHA && !OPENSSL_NO_SHA1 */
    default:
      Jmsg0(NULL, M_ERROR, 0, T_("Unsupported cipher type specified\n"));
      CryptoSessionFree(cs);
      return NULL;
  }

  /* Generate a symmetric session key */
  cs->session_key_len = EVP_CIPHER_key_length(ec);
  cs->session_key = (unsigned char*)malloc(cs->session_key_len);
  if (RAND_bytes(cs->session_key, cs->session_key_len) <= 0) {
    /* OpenSSL failure */
    CryptoSessionFree(cs);
    return NULL;
  }

  /* Generate an IV if possible */
  if ((iv_len = EVP_CIPHER_iv_length(ec))) {
    iv = (unsigned char*)malloc(iv_len);

    /* Generate random IV */
    if (RAND_bytes(iv, iv_len) <= 0) {
      /* OpenSSL failure */
      CryptoSessionFree(cs);
      free(iv);
      return NULL;
    }

    /* Store it in our ASN.1 structure */
    if (!M_ASN1_OCTET_STRING_set(cs->cryptoData->iv, iv, iv_len)) {
      /* Allocation failed in OpenSSL */
      CryptoSessionFree(cs);
      free(iv);
      return NULL;
    }
    free(iv);
  }

  /* Create RecipientInfo structures for supplied
   * public keys. */
  foreach_alist (keypair, pubkeys) {
    RecipientInfo* ri;
    unsigned char* ekey;
    int ekey_len;

    ri = RecipientInfo_new();
    if (!ri) {
      /* Allocation failed in OpenSSL */
      CryptoSessionFree(cs);
      return NULL;
    }

    /* Set the ASN.1 structure version number */
    ASN1_INTEGER_set(ri->version, BAREOS_ASN1_VERSION);

    /* Drop the string allocated by OpenSSL, and add our subjectKeyIdentifier */
    M_ASN1_OCTET_STRING_free(ri->subjectKeyIdentifier);
    ri->subjectKeyIdentifier = M_ASN1_OCTET_STRING_dup(keypair->keyid);

    /* Set our key encryption algorithm. We currently require RSA */
    assert(keypair->pubkey
           && EVP_PKEY_type(EVP_PKEY_id(keypair->pubkey)) == EVP_PKEY_RSA);
    ri->keyEncryptionAlgorithm = OBJ_nid2obj(NID_rsaEncryption);

    /* Encrypt the session key */
    ekey = (unsigned char*)malloc(EVP_PKEY_size(keypair->pubkey));

    ALLOW_DEPRECATED(
        if ((ekey_len = EVP_PKEY_encrypt(ekey, cs->session_key,
                                         cs->session_key_len, keypair->pubkey))
            <= 0) {
          /* OpenSSL failure */
          RecipientInfo_free(ri);
          CryptoSessionFree(cs);
          free(ekey);
          return NULL;
        })

    /* Store it in our ASN.1 structure */
    if (!M_ASN1_OCTET_STRING_set(ri->encryptedKey, ekey, ekey_len)) {
      /* Allocation failed in OpenSSL */
      RecipientInfo_free(ri);
      CryptoSessionFree(cs);
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
 * required length to be written to length. The caller can then allocate
 * sufficient space for the output.
 *
 * Returns: true on success, stores the encoded data in dest, and the size in
 * length. false on failure.
 */
bool CryptoSessionEncode(CRYPTO_SESSION* cs, uint8_t* dest, uint32_t* length)
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
 * Returns: CRYPTO_ERROR_NONE and a pointer to a newly allocated CRYPTO_SESSION
 * structure in *session on success. A crypto_error_t value on failure.
 */
crypto_error_t CryptoSessionDecode(const uint8_t* data,
                                   uint32_t length,
                                   alist<X509_KEYPAIR*>* keypairs,
                                   CRYPTO_SESSION** session)
{
  CRYPTO_SESSION* cs;
  X509_KEYPAIR* keypair = nullptr;
  STACK_OF(RecipientInfo) * recipients;
  crypto_error_t retval = CRYPTO_ERROR_NONE;
#    if (OPENSSL_VERSION_NUMBER >= 0x0090800FL)
  const unsigned char* p = (const unsigned char*)data;
#    else
  unsigned char* p = (unsigned char*)data;
#    endif

  /* bareos-fd.conf doesn't contains any key */
  if (!keypairs) { return CRYPTO_ERROR_NORECIPIENT; }

  cs = (CRYPTO_SESSION*)malloc(sizeof(CRYPTO_SESSION));

  /* Initialize required fields */
  cs->session_key = NULL;

  /* d2i_CryptoData modifies the supplied pointer */
  cs->cryptoData = d2i_CryptoData(NULL, &p, length);

  if (!cs->cryptoData) {
    /* Allocation / Decoding failed in OpenSSL */
    OpensslPostErrors(M_ERROR, T_("CryptoData decoding failed"));
    retval = CRYPTO_ERROR_INTERNAL;
    goto err;
  }

  recipients = cs->cryptoData->recipientInfo;

  /* Find a matching RecipientInfo structure for a supplied
   * public key */
  foreach_alist (keypair, keypairs) {
    RecipientInfo* ri;
    int i;

    /* Private key available? */
    if (keypair->privkey == NULL) { continue; }

    for (i = 0; i < sk_RecipientInfo_num(recipients); i++) {
      ri = sk_RecipientInfo_value(recipients, i);

      /* Match against the subjectKeyIdentifier */
      if (M_ASN1_OCTET_STRING_cmp(keypair->keyid, ri->subjectKeyIdentifier)
          == 0) {
        /* Match found, extract symmetric encryption session data */

        /* RSA is required. */
        assert(EVP_PKEY_type(EVP_PKEY_id(keypair->privkey)) == EVP_PKEY_RSA);

        /* If we recieve a RecipientInfo structure that does not use
         * RSA, return an error */
        if (OBJ_obj2nid(ri->keyEncryptionAlgorithm) != NID_rsaEncryption) {
          retval = CRYPTO_ERROR_INVALID_CRYPTO;
          goto err;
        }

        /* Decrypt the session key */
        /* Allocate sufficient space for the largest possible decrypted data */
        cs->session_key
            = (unsigned char*)malloc(EVP_PKEY_size(keypair->privkey));
        ALLOW_DEPRECATED(
            cs->session_key_len = EVP_PKEY_decrypt(
                cs->session_key, M_ASN1_STRING_data(ri->encryptedKey),
                M_ASN1_STRING_length(ri->encryptedKey), keypair->privkey);)
        if (cs->session_key_len <= 0) {
          OpensslPostErrors(M_ERROR, T_("Failure decrypting the session key"));
          retval = CRYPTO_ERROR_DECRYPTION;
          goto err;
        }

        /* Session key successfully extracted, return the CRYPTO_SESSION
         * structure */
        *session = cs;
        return CRYPTO_ERROR_NONE;
      }
    }
  }

  /* No matching recipient found */
  return CRYPTO_ERROR_NORECIPIENT;

err:
  CryptoSessionFree(cs);
  return retval;
}

// Free memory associated with a crypto session object.
void CryptoSessionFree(CRYPTO_SESSION* cs)
{
  if (cs->cryptoData) { CryptoData_free(cs->cryptoData); }
  if (cs->session_key) { free(cs->session_key); }
  free(cs);
}

/*
 * Create a new crypto cipher context with the specified session object
 *  Returns: A pointer to a CIPHER_CONTEXT object on success. The cipher block
 * size is returned in blocksize. NULL on failure.
 */
CIPHER_CONTEXT* crypto_cipher_new(CRYPTO_SESSION* cs,
                                  bool encrypt,
                                  uint32_t* blocksize)
{
  CIPHER_CONTEXT* cipher_ctx = new CIPHER_CONTEXT;
  const EVP_CIPHER* ec;

  // Acquire a cipher instance for the given ASN.1 cipher NID
  if ((ec = EVP_get_cipherbyobj(cs->cryptoData->contentEncryptionAlgorithm))
      == NULL) {
    Jmsg1(NULL, M_ERROR, 0, T_("Unsupported contentEncryptionAlgorithm: %d\n"),
          OBJ_obj2nid(cs->cryptoData->contentEncryptionAlgorithm));
    delete cipher_ctx;
    return NULL;
  }

  if (encrypt) {
    /* Initialize for encryption */
    if (!EVP_CipherInit_ex(cipher_ctx->ctx, ec, NULL, NULL, NULL, 1)) {
      OpensslPostErrors(M_ERROR,
                        T_("OpenSSL cipher context initialization failed"));
      goto err;
    }
  } else {
    /* Initialize for decryption */
    if (!EVP_CipherInit_ex(cipher_ctx->ctx, ec, NULL, NULL, NULL, 0)) {
      OpensslPostErrors(M_ERROR,
                        T_("OpenSSL cipher context initialization failed"));
      goto err;
    }
  }

  /* Set the key size */
  if (!EVP_CIPHER_CTX_set_key_length(cipher_ctx->ctx, cs->session_key_len)) {
    OpensslPostErrors(
        M_ERROR, T_("Encryption session provided an invalid symmetric key"));
    goto err;
  }

  /* Validate the IV length */
  if (EVP_CIPHER_iv_length(ec) != M_ASN1_STRING_length(cs->cryptoData->iv)) {
    OpensslPostErrors(M_ERROR, T_("Encryption session provided an invalid IV"));
    goto err;
  }

  /* Add the key and IV to the cipher context */
  if (!EVP_CipherInit_ex(cipher_ctx->ctx, NULL, NULL, cs->session_key,
                         M_ASN1_STRING_data(cs->cryptoData->iv), -1)) {
    OpensslPostErrors(M_ERROR,
                      T_("OpenSSL cipher context key/IV initialization failed"));
    goto err;
  }

  *blocksize = EVP_CIPHER_CTX_block_size(cipher_ctx->ctx);
  return cipher_ctx;

err:
  delete cipher_ctx;
  return NULL;
}

/*
 * Encrypt/Decrypt length bytes of data using the provided cipher context
 * Returns: true on success, number of bytes output in written
 *          false on failure
 */
bool CryptoCipherUpdate(CIPHER_CONTEXT* cipher_ctx,
                        const uint8_t* data,
                        uint32_t length,
                        const uint8_t* dest,
                        uint32_t* written)
{
  if (!EVP_CipherUpdate(cipher_ctx->ctx, (unsigned char*)dest, (int*)written,
                        (const unsigned char*)data, length)) {
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
bool CryptoCipherFinalize(CIPHER_CONTEXT* cipher_ctx,
                          uint8_t* dest,
                          uint32_t* written)
{
  if (!EVP_CipherFinal_ex(cipher_ctx->ctx, (unsigned char*)dest,
                          (int*)written)) {
    /* This really shouldn't fail */
    return false;
  } else {
    return true;
  }
}

// Free memory associated with a cipher context.
void CryptoCipherFree(CIPHER_CONTEXT* cipher_ctx) { delete cipher_ctx; }

const char* crypto_digest_name(DIGEST* digest)
{
  return crypto_digest_name(digest->type);
}

std::optional<std::string> compute_hash(const std::string& unhashed,
                                        const std::string& digestname)
{
  std::stringstream result;
  const EVP_MD* type = EVP_get_digestbyname(digestname.c_str());
  if (type) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;
    EVP_MD_CTX* context = EVP_MD_CTX_create();
    if ((context != nullptr) && EVP_DigestInit_ex(context, type, nullptr)
        && EVP_DigestUpdate(context, unhashed.c_str(), unhashed.length())
        && EVP_DigestFinal_ex(context, hash, &lengthOfHash)) {
      result << "{" << EVP_MD_name(type) << "}";
      result << std::hex << std::setw(2) << std::setfill('0');
      for (unsigned int i = 0; i < lengthOfHash; ++i) {
        result << static_cast<int>(hash[i]);
      }
      EVP_MD_CTX_destroy(context);
      return result.str();
    }
  }
  return {};
}

#  endif /* HAVE_CRYPTO */

// Generic OpenSSL routines used for both TLS and CRYPTO support.
/* Are we initialized? */
static int crypto_initialized = false;

/*
 * Perform global initialization of OpenSSL
 * This function is not thread safe.
 *  Returns: 0 on success
 *           errno on failure
 */
int InitCrypto(void)
{
  int status;

  if ((status = OpensslInitThreads()) != 0) {
    BErrNo be;
    Jmsg1(NULL, M_ABORT, 0, T_("Unable to init OpenSSL threading: ERR=%s\n"),
          be.bstrerror(status));
  }

  /* Load libssl and libcrypto human-readable error strings */
  SSL_load_error_strings();

  /* Initialize OpenSSL SSL  library */
#  if OPENSSL_VERSION_NUMBER < 0x10100000L
  SSL_library_init();
#  else
  OPENSSL_init_ssl(0, NULL);
#  endif

  /* Register OpenSSL ciphers and digests */
  OpenSSL_add_all_algorithms();

#  ifdef HAVE_ENGINE_LOAD_PK11
  ENGINE_load_pk11();
#  else
  // Load all the builtin engines.
  ALLOW_DEPRECATED(ENGINE_load_builtin_engines();
                   ENGINE_register_all_complete();)
#  endif

  crypto_initialized = true;

  return status;
}

/*
 * Perform global cleanup of OpenSSL
 * All cryptographic operations must be completed before calling this function.
 * This function is not thread safe.
 *  Returns: 0 on success
 *           errno on failure
 */
int CleanupCrypto(void)
{
  /* Ensure that we've actually been initialized; Doing this here decreases the
   * complexity of client's termination/cleanup code. */
  if (!crypto_initialized) { return 0; }

#  ifndef HAVE_SUN_OS
  // Cleanup the builtin engines.
  ENGINE_cleanup();
#  endif

  OpensslCleanupThreads();

  /* Free libssl and libcrypto error strings */
  ERR_free_strings();

  /* Free all ciphers and digests */
  EVP_cleanup();

  /* Free memory used by PRNG */
  RAND_cleanup();

  crypto_initialized = false;

  return 0;
}

/* Array of mutexes for use with OpenSSL static locking */
static pthread_mutex_t* mutexes;

/* OpenSSL dynamic locking structure */
struct CRYPTO_dynlock_value {
  pthread_mutex_t mutex;
};

/*
 * ***FIXME*** this is a sort of dummy to avoid having to
 *   change all the existing code to pass either a jcr or
 *   a NULL.  Passing a NULL causes the messages to be
 *   printed by the daemon -- not very good :-(
 */
void OpensslPostErrors(int type, const char* errstring)
{
  OpensslPostErrors(NULL, type, errstring);
}

// Post all per-thread openssl errors
void OpensslPostErrors(JobControlRecord* jcr, int type, const char* errstring)
{
  char buf[512];
  unsigned long sslerr;

  /* Pop errors off of the per-thread queue */
  while ((sslerr = ERR_get_error()) != 0) {
    /* Acquire the human readable string */
    ERR_error_string_n(sslerr, buf, sizeof(buf));
    Dmsg3(50, "jcr=%p %s: ERR=%s\n", jcr, errstring, buf);
    Qmsg(jcr, type, 0, "%s: ERR=%s\n", errstring, buf);
  }
}

/*
 * we don't add the callbacks for openssl > 1.1.0
 * so if any of the callbacks is actually used (i.e. any of the openssl macros
 * does use the callback name instead of evaluating to nothing), then we will
 * get a compile error
 * we will get a compile error
 */
#  if OPENSSL_VERSION_NUMBER < 0x10100000L
/*
 * Return an OpenSSL thread ID
 *  Returns: thread ID
 *
 */
[[maybe_unused]] static unsigned long GetOpensslThreadId(void)
{
#    ifdef HAVE_WIN32
  return (unsigned long)getpid();
#    else
  /* Comparison without use of pthread_equal() is mandated by the OpenSSL API
   *
   * Note: this creates problems with the new Win32 pthreads
   *   emulation code, which defines pthread_t as a structure. */
  return ((unsigned long)pthread_self());
#    endif /* not HAVE_WIN32 */
}

// Allocate a dynamic OpenSSL mutex
[[maybe_unused]] static struct CRYPTO_dynlock_value*
openssl_create_dynamic_mutex(const char*, int)
{
  struct CRYPTO_dynlock_value* dynlock;
  int status;

  dynlock = (struct CRYPTO_dynlock_value*)malloc(
      sizeof(struct CRYPTO_dynlock_value));

  if ((status = pthread_mutex_init(&dynlock->mutex, NULL)) != 0) {
    BErrNo be;
    Jmsg1(NULL, M_ABORT, 0, T_("Unable to init mutex: ERR=%s\n"),
          be.bstrerror(status));
  }

  return dynlock;
}

[[maybe_unused]] static void OpensslUpdateDynamicMutex(
    int mode,
    struct CRYPTO_dynlock_value* dynlock,
    const char*,
    int)
{
  if (mode & CRYPTO_LOCK) {
    lock_mutex(dynlock->mutex);
  } else {
    unlock_mutex(dynlock->mutex);
  }
}

[[maybe_unused]] static void OpensslDestroyDynamicMutex(
    struct CRYPTO_dynlock_value* dynlock,
    const char*,
    int)
{
  int status;

  if ((status = pthread_mutex_destroy(&dynlock->mutex)) != 0) {
    BErrNo be;
    Jmsg1(NULL, M_ABORT, 0, T_("Unable to destroy mutex: ERR=%s\n"),
          be.bstrerror(status));
  }

  free(dynlock);
}

// (Un)Lock a static OpenSSL mutex
[[maybe_unused]] static void openssl_update_static_mutex(int mode,
                                                         int i,
                                                         const char*,
                                                         int)
{
  if (mode & CRYPTO_LOCK) {
    lock_mutex(mutexes[i]);
  } else {
    unlock_mutex(mutexes[i]);
  }
}
#  endif

/*
 * Initialize OpenSSL thread support
 *  Returns: 0 on success
 *           errno on failure
 */
int OpensslInitThreads(void)
{
  int i, numlocks;
  int status;


  /* Set thread ID callback */
  CRYPTO_set_id_callback(GetOpensslThreadId);

  /* Initialize static locking */
  numlocks = CRYPTO_num_locks();
  mutexes = (pthread_mutex_t*)malloc(numlocks * sizeof(pthread_mutex_t));
  for (i = 0; i < numlocks; i++) {
    if ((status = pthread_mutex_init(&mutexes[i], NULL)) != 0) {
      BErrNo be;
      Jmsg1(NULL, M_FATAL, 0, T_("Unable to init mutex: ERR=%s\n"),
            be.bstrerror(status));
      return status;
    }
  }

  /* Set static locking callback */
  CRYPTO_set_locking_callback(openssl_update_static_mutex);

  /* Initialize dyanmic locking */
  CRYPTO_set_dynlock_create_callback(openssl_create_dynamic_mutex);
  CRYPTO_set_dynlock_lock_callback(OpensslUpdateDynamicMutex);
  CRYPTO_set_dynlock_destroy_callback(OpensslDestroyDynamicMutex);

  return 0;
}

// Clean up OpenSSL threading support
void OpensslCleanupThreads(void)
{
  int i, numlocks;
  int status;

  /* Unset thread ID callback */
  CRYPTO_set_id_callback(NULL);

  /* Deallocate static lock mutexes */
  numlocks = CRYPTO_num_locks();
  for (i = 0; i < numlocks; i++) {
    if ((status = pthread_mutex_destroy(&mutexes[i])) != 0) {
      BErrNo be;

      switch (status) {
        case EPERM:
          /* No need to report errors when we get an EPERM */
          break;
        default:
          /* We don't halt execution, reporting the error should be sufficient
           */
          Jmsg2(NULL, M_ERROR, 0, T_("Unable to destroy mutex: %d ERR=%s\n"),
                status, be.bstrerror(status));
          break;
      }
    }
  }

  /* Unset static locking callback */
  CRYPTO_set_locking_callback(NULL);

  /* Free static lock array */
  free(mutexes);

  /* Unset dynamic locking callbacks */
  CRYPTO_set_dynlock_create_callback(NULL);
  CRYPTO_set_dynlock_lock_callback(NULL);
  CRYPTO_set_dynlock_destroy_callback(NULL);
}

#endif /* HAVE_OPENSSL */
