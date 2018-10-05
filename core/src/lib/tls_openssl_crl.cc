/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2018 Bareos GmbH & Co. KG

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
 * tls_openssl.c TLS support functions when using OPENSSL backend.
 *
 * Author: Landon Fuller <landonf@threerings.net>
 */

#include "include/bareos.h"
#include "lib/crypto_openssl.h"
#include "lib/tls_openssl_crl.h"
#include "lib/bpoll.h"
#include <assert.h>

#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <openssl/err.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>

#define MAX_CRLS 16

#if (OPENSSL_VERSION_NUMBER >= 0x00907000L) && (OPENSSL_VERSION_NUMBER < 0x10100000L)

static X509_LOOKUP_METHOD *X509_LOOKUP_crl_reloader(void);
static int LoadNewCrlFile(X509_LOOKUP *lu, const char *fname);

bool SetCertificateRevocationList(const std::string &crlfile, SSL_CTX *openssl_ctx)
{
   X509_STORE *store;
   X509_LOOKUP *lookup;

   store = SSL_CTX_get_cert_store(openssl_ctx);
   if (!store) {
      OpensslPostErrors(M_FATAL, _("Error loading revocation list file"));
      return false;
   }

   lookup = X509_STORE_add_lookup(store, X509_LOOKUP_crl_reloader());
   if (!lookup) {
      OpensslPostErrors(M_FATAL, _("Error loading revocation list file"));
      return false;
   }

   if (!LoadNewCrlFile(lookup, (char *)crlfile.c_str())) {
      OpensslPostErrors(M_FATAL, _("Error loading revocation list file"));
      return false;
   }

   X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);

   return true;
}


struct TlsCrlReloadContext
{
   time_t mtime;
   char *crl_file_name;
   X509_CRL *crls[MAX_CRLS];
};

/*
 * Automatic Certificate Revocation List reload logic.
 */
static int CrlReloaderNew(X509_LOOKUP *lookup)
{
   TlsCrlReloadContext *data;

   data = (TlsCrlReloadContext *)malloc(sizeof(TlsCrlReloadContext));
   memset(data, 0, sizeof(TlsCrlReloadContext));

   lookup->method_data = (char *)data;
   return 1;
}

static void CrlReloaderFree(X509_LOOKUP *lookup)
{
   int cnt;
   TlsCrlReloadContext *data;

   if (lookup->method_data) {
      data = (TlsCrlReloadContext *)lookup->method_data;

      if (data->crl_file_name) {
         free(data->crl_file_name);
      }

      for (cnt = 0; cnt < MAX_CRLS; cnt++) {
         if (data->crls[cnt]) {
            X509_CRL_free(data->crls[cnt]);
         }
      }

      free(data);
      lookup->method_data = NULL;
   }
}

/*
 * Load the new content from a Certificate Revocation List (CRL).
 */
static int CrlReloaderReloadFile(X509_LOOKUP *lookup)
{
   int cnt, ok = 0;
   struct stat st;
   BIO *in = NULL;
   TlsCrlReloadContext *data;

   data = (TlsCrlReloadContext *)lookup->method_data;
   if (!data->crl_file_name) {
      goto bail_out;
   }

   if (stat(data->crl_file_name, &st) != 0) {
      goto bail_out;
   }

   in = BIO_new_file(data->crl_file_name, "r");
   if (!in) {
      goto bail_out;
   }

   /*
    * Load a maximum of MAX_CRLS Certificate Revocation Lists.
    */
   data->mtime = st.st_mtime;
   for (cnt = 0; cnt < MAX_CRLS; cnt++) {
      X509_CRL *crl;

      if ((crl = PEM_read_bio_X509_CRL(in, NULL, NULL, NULL)) == NULL) {
         if (cnt == 0) {
            /*
             * We try to read multiple times only the first is fatal.
             */
            goto bail_out;
         } else {
            break;
         }
      }

      if (data->crls[cnt]) {
         X509_CRL_free(data->crls[cnt]);
      }
      data->crls[cnt] = crl;
   }

   /*
    * Clear the other slots.
    */
   while (++cnt < MAX_CRLS) {
      if (data->crls[cnt]) {
         X509_CRL_free(data->crls[cnt]);
         data->crls[cnt] = NULL;
      }
   }

   ok = 1;

bail_out:
   if (in) {
      BIO_free(in);
   }
   return ok;
}

/*
 * See if the data in the Certificate Revocation List (CRL) is newer then we loaded before.
 */
static int CrlReloaderReloadIfNewer(X509_LOOKUP *lookup)
{
   int ok = 0;
   TlsCrlReloadContext *data;
   struct stat st;

   data = (TlsCrlReloadContext *)lookup->method_data;
   if (!data->crl_file_name) {
      return ok;
   }

   if (stat(data->crl_file_name, &st) != 0) {
      return ok;
   }

   if (st.st_mtime > data->mtime) {
      ok = CrlReloaderReloadFile(lookup);
      if (!ok) {
         goto bail_out;
      }
   }
   ok = 1;

bail_out:
   return ok;
}

/*
 * Load the data from a Certificate Revocation List (CRL) into memory.
 */
static int CrlReloaderFileLoad(X509_LOOKUP *lookup, const char *argp)
{
   int ok = 0;
   TlsCrlReloadContext *data;

   data = (TlsCrlReloadContext *)lookup->method_data;
   if (data->crl_file_name) {
      free(data->crl_file_name);
   }
   data->crl_file_name = bstrdup(argp);

   ok = CrlReloaderReloadFile(lookup);
   if (!ok) {
      goto bail_out;
   }
   ok = 1;

bail_out:
   return ok;
}

static int CrlReloaderCtrl(X509_LOOKUP *lookup, int cmd, const char *argp, long argl, char **ret)
{
   int ok = 0;

   switch (cmd) {
   case X509_L_FILE_LOAD:
      ok = CrlReloaderFileLoad(lookup, argp);
      break;
   default:
      break;
   }

   return ok;
}

/*
 * Check if a CRL entry is expired.
 */
static int CrlEntryExpired(X509_CRL *crl)
{
   int lastUpdate, nextUpdate;

   if (!crl) {
      return 0;
   }

   lastUpdate = X509_cmp_current_time(X509_CRL_get_lastUpdate(crl));
   nextUpdate = X509_cmp_current_time(X509_CRL_get_nextUpdate(crl));

   if (lastUpdate < 0 && nextUpdate > 0) {
      return 0;
   }

   return 1;
}

/*
 * Retrieve a CRL entry by Subject.
 */
static int CrlReloaderGetBySubject(X509_LOOKUP *lookup, int type, X509_NAME *name, X509_OBJECT *ret)
{
   int cnt, ok = 0;
   TlsCrlReloadContext *data = NULL;

   if (type != X509_LU_CRL) {
      return ok;
   }

   data = (TlsCrlReloadContext *)lookup->method_data;
   if (!data->crls[0]) {
      return ok;
   }

   ret->type = 0;
   ret->data.crl = NULL;
   for (cnt = 0; cnt < MAX_CRLS; cnt++) {
      if (CrlEntryExpired(data->crls[cnt]) && !CrlReloaderReloadIfNewer(lookup)) {
         goto bail_out;
      }

      if (X509_NAME_cmp(data->crls[cnt]->crl->issuer, name)) {
         continue;
      }

      ret->type = type;
      ret->data.crl = data->crls[cnt];
      ok = 1;
      break;
   }

   return ok;

bail_out:
   return ok;
}

static int LoadNewCrlFile(X509_LOOKUP *lu, const char *fname)
{
   int ok = 0;

   if (!fname) {
      return ok;
   }
   ok = X509_LOOKUP_ctrl(lu, X509_L_FILE_LOAD, fname, 0, NULL);

   return ok;
}

static X509_LOOKUP_METHOD x509_crl_reloader = {
   "CRL file reloader",
   CrlReloaderNew,            /* new */
   CrlReloaderFree,           /* free */
   NULL,                        /* init */
   NULL,                        /* shutdown */
   CrlReloaderCtrl,           /* ctrl */
   CrlReloaderGetBySubject, /* get_by_subject */
   NULL,                        /* get_by_issuer_serial */
   NULL,                        /* get_by_fingerprint */
   NULL                         /* get_by_alias */
};

static X509_LOOKUP_METHOD *X509_LOOKUP_crl_reloader(void)
{
   return (&x509_crl_reloader);
}

#endif /* (OPENSSL_VERSION_NUMBER >= 0x00907000L) && (OPENSSL_VERSION_NUMBER < 0x10100000L) */
