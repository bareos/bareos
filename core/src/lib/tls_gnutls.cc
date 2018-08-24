/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * tls_gnutls.c TLS support functions when using GNUTLS backend.
 *
 * Author: Marco van Wieringen <marco.van.wieringen@bareos.com>
 */

#include <assert.h>
#include "include/bareos.h"
#include "tls_gnutls.h"

TlsGnuTls::TlsGnuTls() { return; }

TlsGnuTls::~TlsGnuTls() { return; }

#if defined(HAVE_TLS) && defined(HAVE_GNUTLS)

#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#define TLS_DEFAULT_CIPHERS "NONE:+VERS-TLS1.0:+CIPHER-ALL:+COMP-ALL:+RSA:+DHE-RSA:+DHE-DSS:+MAC-ALL"

#define DH_BITS 1024

class TlsImplementationOpenSsl {
 private:
  friend class GnuTls;
};

/* TLS Context Structure */
struct TlsImplementationGnuTls {
  gnutls_dh_params dh_params;
  gnutls_certificate_client_credentials gnutls_cred;

  CRYPTO_PEM_PASSWD_CB *pem_callback;
  const char *cipher_list;
  const void *pem_userdata;
  unsigned char *dhdata;
  bool VerifyPeer;
  bool tls_require;
};

struct TlsConnection {
  TlsImplementationGnuTls *ctx;
  gnutls_session_t gnutls_state;
};

static inline bool LoadDhfileData(TLS_IMPLEMENTATION *ctx, const char *dhfile)
{
  FILE *fp;
  int error;
  size_t size;
  struct stat st;
  gnutls_datum_t dhparms;

  /*
   * Load the content of the file into memory.
   */
  if (stat(dhfile, &st) < 0) {
    return false;
  }

  if ((fp = fopen(dhfile, "r")) == (FILE *)NULL) {
    return false;
  }

  /*
   * Allocate a memory buffer to hold the DH data.
   */
  ctx->dhdata = (unsigned char *)malloc(st.st_size + 1);

  size = fread(ctx->dhdata, sizeof(ctx->dhdata), 1, fp);

  fclose(fp);

  dhparms.data = ctx->dhdata;
  dhparms.size = size;

  error = gnutls_dh_params_import_pkcs3(ctx->dh_params, &dhparms, GNUTLS_X509_FMT_PEM);
  if (error != GNUTLS_E_SUCCESS) {
    return false;
  }

  return true;
}

TLS_IMPLEMENTATION *new_tls_context(const char *cipherlist, CRYPTO_TLS_PSK_CB) {}

/*
 * Create a new TLS_IMPLEMENTATION instance.
 *  Returns: Pointer to TLS_IMPLEMENTATION instance on success
 *           NULL on failure;
 */
TLS_IMPLEMENTATION *new_tls_context(const char *CaCertfile,
                                    const char *CaCertdir,
                                    const char *crlfile,
                                    const char *certfile,
                                    const char *keyfile,
                                    CRYPTO_PEM_PASSWD_CB *pem_callback,
                                    const void *pem_userdata,
                                    const char *dhfile,
                                    const char *cipherlist,
                                    bool VerifyPeer)
{
  int error;
  TLS_IMPLEMENTATION *ctx;

  ctx = (TLS_IMPLEMENTATION *)malloc(sizeof(TLS_IMPLEMENTATION));
  memset(ctx, 0, sizeof(TLS_IMPLEMENTATION));

  ctx->pem_callback = pem_callback;
  ctx->pem_userdata = pem_userdata;
  ctx->cipher_list  = cipherlist;
  ctx->VerifyPeer   = VerifyPeer;

  error = gnutls_certificate_allocate_credentials(&ctx->gnutls_cred);
  if (error != GNUTLS_E_SUCCESS) {
    Jmsg1(NULL, M_ERROR, 0, _("Failed to create a new GNUTLS certificate credential: ERR=%s\n"),
          gnutls_strerror(error));
    free(ctx);
    return NULL;
  }

  /*
   * GNUTLS supports only a certfile not a certdir.
   */
  if (CaCertdir && !CaCertfile) {
    Jmsg0(NULL, M_ERROR, 0, _("GNUTLS doesn't support certdir use certfile instead\n"));
    goto bail_out;
  }

  /*
   * Try to load the trust file, first in PEM format and if that fails in DER format.
   */
  if (CaCertfile) {
    error = gnutls_certificate_set_x509_trust_file(ctx->gnutls_cred, CaCertfile, GNUTLS_X509_FMT_PEM);
    if (error < GNUTLS_E_SUCCESS) {
      error = gnutls_certificate_set_x509_trust_file(ctx->gnutls_cred, CaCertfile, GNUTLS_X509_FMT_DER);
      if (error < GNUTLS_E_SUCCESS) {
        Jmsg1(NULL, M_ERROR, 0, _("Error loading CA certificates from %s\n"), CaCertfile);
        goto bail_out;
      }
    }
  } else if (VerifyPeer) {
    /*
     * At least one CA is required for peer verification
     */
    Jmsg0(NULL, M_ERROR, 0, _("Certificate file must be specified as a verification store\n"));
    goto bail_out;
  }

  /*
   * Try to load the revocation list file, first in PEM format and if that fails in DER format.
   */
  if (crlfile) {
    error = gnutls_certificate_set_x509_crl_file(ctx->gnutls_cred, crlfile, GNUTLS_X509_FMT_PEM);
    if (error < GNUTLS_E_SUCCESS) {
      error = gnutls_certificate_set_x509_crl_file(ctx->gnutls_cred, crlfile, GNUTLS_X509_FMT_DER);
      if (error < GNUTLS_E_SUCCESS) {
        Jmsg1(NULL, M_ERROR, 0, _("Error loading certificate revocation list from %s\n"), crlfile);
        goto bail_out;
      }
    }
  }

  /*
   * Try to load the certificate and the keyfile, first in PEM format and if that fails in DER format.
   */
  if (certfile && keyfile) {
    error = gnutls_certificate_set_x509_key_file(ctx->gnutls_cred, certfile, keyfile, GNUTLS_X509_FMT_PEM);
    if (error != GNUTLS_E_SUCCESS) {
      error = gnutls_certificate_set_x509_key_file(ctx->gnutls_cred, certfile, keyfile, GNUTLS_X509_FMT_DER);
      if (error != GNUTLS_E_SUCCESS) {
        Jmsg2(NULL, M_ERROR, 0, _("Error loading key from %s or certificate from %s\n"), keyfile, certfile);
        goto bail_out;
      }
    }
  }

  error = gnutls_dh_params_init(&ctx->dh_params);
  if (error != GNUTLS_E_SUCCESS) {
    goto bail_out;
  }

  if (dhfile) {
    if (!LoadDhfileData(ctx, dhfile)) {
      Jmsg1(NULL, M_ERROR, 0, _("Failed to load DH file %s\n"), dhfile);
      goto bail_out;
    }
  } else {
    error = gnutls_dh_params_generate2(ctx->dh_params, DH_BITS);
    if (error != GNUTLS_E_SUCCESS) {
      Jmsg0(NULL, M_ERROR, 0, _("Failed to generate new DH parameters\n"));
      goto bail_out;
    }
  }

  /*
   * Link the dh params and the credentials.
   */
  gnutls_certificate_set_dh_params(ctx->gnutls_cred, ctx->dh_params);

  return ctx;

bail_out:
  FreeTlsContext(ctx);
  return NULL;
}

void TlsGnuTls::FreeTlsContext(TLS_IMPLEMENTATION *ctx)
{
  gnutls_certificate_free_credentials(ctx->gnutls_cred);

  if (ctx->dhdata) {
    free(ctx->dhdata);
  }

  free(ctx);
}

/*
 * Certs are not automatically verified during the handshake.
 */
static inline bool TlsCertVerify(TlsConnectionContextGnuTls *tls_conn)
{
  unsigned int status = 0;
  int error;
  time_t now = time(NULL);
  time_t peertime;

  error = gnutls_certificate_verify_peers2(tls_conn->gnutls_state, &status);
  if (error != GNUTLS_E_SUCCESS) {
    Jmsg1(NULL, M_ERROR, 0, _("gnutls_certificate_verify_peers2 failed: ERR=%s\n"), gnutls_strerror(error));
    return false;
  }

  if (status) {
    Jmsg1(NULL, M_ERROR, 0, _("peer certificate untrusted or revoked (0x%x)\n"), status);
    return false;
  }

  peertime = gnutls_certificate_expiration_time_peers(tls_conn->gnutls_state);
  if (peertime == -1) {
    Jmsg0(NULL, M_ERROR, 0, _("gnutls_certificate_expiration_time_peers failed\n"));
    return false;
  }
  if (peertime < now) {
    Jmsg0(NULL, M_ERROR, 0, _("peer certificate is expired\n"));
    return false;
  }

  peertime = gnutls_certificate_activation_time_peers(tls_conn->gnutls_state);
  if (peertime == -1) {
    Jmsg0(NULL, M_ERROR, 0, _("gnutls_certificate_activation_time_peers failed\n"));
    return false;
  }
  if (peertime > now) {
    Jmsg0(NULL, M_ERROR, 0, _("peer certificate not yet active\n"));
    return false;
  }

  return true;
}

/*
 * Verifies a list of common names against the certificate commonName attribute.
 *
 * Returns: true on success
 *          false on failure
 */
bool TlsPostconnectVerifyCn(JobControlRecord *jcr, const std::vector<std::string> &verify_list)
{
  char *cn;
  int error, cnt;
  unsigned int list_size;
  size_t cn_length;
  char cannonicalname[256];
  bool auth_success = false;
  gnutls_x509_crt_t cert;
  const gnutls_datum_t *peer_cert_list;

  /*
   * See if we verify the peer certificate.
   */
  if (!tls_conn->ctx->VerifyPeer) {
    return true;
  }

  peer_cert_list = gnutls_certificate_get_peers(tls_conn->gnutls_state, &list_size);
  if (!peer_cert_list) {
    return false;
  }

  error = gnutls_x509_crt_init(&cert);
  if (error != GNUTLS_E_SUCCESS) {
    return false;
  }

  gnutls_x509_crt_import(cert, peer_cert_list, GNUTLS_X509_FMT_DER);
  if (error != GNUTLS_E_SUCCESS) {
    gnutls_x509_crt_deinit(cert);
    return false;
  }

  /*
   * See what CN's are available.
   */
  error = 0;
  for (cnt = 0; error >= 0; cnt++) {
    cn_length = sizeof(cannonicalname);
    error =
        gnutls_x509_crt_get_dn_by_oid(cert, GNUTLS_OID_X520_COMMON_NAME, cnt, 0, cannonicalname, &cn_length);
    if (error < 0) {
      break;
    }

    /*
     * NULL Terminate data.
     */
    cannonicalname[255] = '\0';

    /*
     * Try all the CNs in the list
     */
    foreach_alist(cn, verify_list)
    {
      Dmsg2(120, "comparing CNs: cert-cn=%s, allowed-cn=%s\n", cannonicalname, cn);
      if (Bstrcasecmp(cn, cannonicalname)) {
        auth_success = true;
        break;
      }
    }

    if (auth_success) {
      break;
    }
  }

  gnutls_x509_crt_deinit(cert);

  return auth_success;
}

/*
 * Verifies a peer's hostname against the subjectAltName and commonName attributes.
 *
 * Returns: true on success
 *          false on failure
 */
bool TlsPostconnectVerifyHost(JobControlRecord *jcr, TlsConnectionContextGnuTls *tls_conn, const char *host)
{
  int error;
  unsigned int list_size;
  gnutls_x509_crt_t cert;
  const gnutls_datum_t *peer_cert_list;

  /*
   * See if we verify the peer certificate.
   */
  if (!tls_conn->ctx->VerifyPeer) {
    return true;
  }

  peer_cert_list = gnutls_certificate_get_peers(tls_conn->gnutls_state, &list_size);
  if (!peer_cert_list) {
    return false;
  }

  error = gnutls_x509_crt_init(&cert);
  if (error != GNUTLS_E_SUCCESS) {
    return false;
  }

  error = gnutls_x509_crt_import(cert, peer_cert_list, GNUTLS_X509_FMT_DER);
  if (error != GNUTLS_E_SUCCESS) {
    gnutls_x509_crt_deinit(cert);
    return false;
  }

  if (!gnutls_x509_crt_check_hostname(cert, host)) {
    gnutls_x509_crt_deinit(cert);
    return false;
  }

  gnutls_x509_crt_deinit(cert);
  return true;
}

/*
 * Create a new TlsConnectionContextGnuTls instance.
 *
 * Returns: Pointer to TlsConnectionContextGnuTls instance on success
 *          NULL on failure;
 */
TlsConnectionContextGnuTls *new_tls_connection(TLS_IMPLEMENTATION *ctx, int fd, bool server)
{
  TlsConnectionContextGnuTls *tls_conn;
  int error;

  /*
   * Allocate our new tls connection
   */
  tls_conn = (TlsConnectionContextGnuTls *)malloc(sizeof(TlsConnectionContextGnuTls));
  memset(tls_conn, 0, sizeof(TlsConnectionContextGnuTls));

  /*
   * Link the TLS context and the TLS session.
   */
  tls_conn->ctx = ctx;

  if (server) {
    error = gnutls_init(&tls_conn->gnutls_state, GNUTLS_SERVER);
  } else {
    error = gnutls_init(&tls_conn->gnutls_state, GNUTLS_CLIENT);
  }

  if (error != GNUTLS_E_SUCCESS) {
    Jmsg1(NULL, M_ERROR, 0, _("Failed to create a new GNUTLS session: ERR=%s\n"), gnutls_strerror(error));
    goto bail_out;
  }

  /*
   * Set the default ciphers to use for the TLS connection.
   */
  if (ctx->cipher_list) {
    gnutls_priority_set_direct(tls_conn->gnutls_state, ctx->cipher_list, NULL);
  } else {
    gnutls_priority_set_direct(tls_conn->gnutls_state, TLS_DEFAULT_CIPHERS, NULL);
  }

  /*
   * Link the credentials and the session.
   */
  gnutls_credentials_set(tls_conn->gnutls_state, GNUTLS_CRD_CERTIFICATE, ctx->gnutls_cred);

  /*
   * Link the TLS session and the filedescriptor of the socket used.
   * gnutls_transport_set_ptr may cause problems on some platforms,
   * therefore the replacement gnutls_transport_set_int is used,
   * when available (since GnuTLS >= 3.1.9)
   */
#ifdef HAVE_GNUTLS_TRANSPORT_SET_INT
  gnutls_transport_set_int(tls_conn->gnutls_state, fd);
#else
  gnutls_transport_set_ptr(tls_conn->gnutls_state, (gnutls_transport_ptr_t)fd);
#endif

  /*
   * Server specific settings.
   */
  if (server) {
    /*
     * See if we require the other party to have a certificate too.
     */
    if (ctx->tls_require) {
      gnutls_certificate_server_set_request(tls_conn->gnutls_state, GNUTLS_CERT_REQUIRE);
    } else {
      gnutls_certificate_server_set_request(tls_conn->gnutls_state, GNUTLS_CERT_REQUEST);
    }

    gnutls_dh_set_prime_bits(tls_conn->gnutls_state, DH_BITS);
  }

  return tls_conn;

bail_out:
  free(tls_conn);
  return NULL;
}

void TlsGnuTls::FreeTlsConnection(TlsConnectionContextGnuTls *tls_conn)
{
  gnutls_deinit(tls_conn->gnutls_state);
  free(tls_conn);
}

static inline bool GnutlsBsockSessionStart(BareosSocket *bsock, bool server)
{
  int flags, error;
  bool status = true;
  bool done   = false;
  unsigned int list_size;
  TlsConnectionContextGnuTls *tls_conn = bsock->tls_conn;
  const gnutls_datum_t *peer_cert_list;

  /* Ensure that socket is non-blocking */
  flags = bsock->SetNonblocking();

  /* start timer */
  bsock->timer_start = watchdog_time;
  bsock->ClearTimedOut();
  bsock->SetKillable(false);

  while (!done) {
    error = gnutls_handshake(tls_conn->gnutls_state);

    switch (error) {
      case GNUTLS_E_SUCCESS:
        status = true;
        done   = true;
        break;
      case GNUTLS_E_AGAIN:
      case GNUTLS_E_INTERRUPTED:
        if (gnutls_record_get_direction(tls_conn->gnutls_state) == 1) {
          WaitForWritableFd(bsock->fd_, 10000, false);
        } else {
          WaitForReadableFd(bsock->fd_, 10000, false);
        }
        status = true;
        continue;
      default:
        status = false;
        goto cleanup;
    }

    if (bsock->IsTimedOut()) {
      goto cleanup;
    }

    /*
     * See if we need to verify the peer.
     */
    peer_cert_list = gnutls_certificate_get_peers(tls_conn->gnutls_state, &list_size);
    if (!peer_cert_list && !tls_conn->ctx->tls_require) {
      goto cleanup;
    }

    if (tls_conn->ctx->VerifyPeer) {
      if (!TlsCertVerify(tls_conn)) {
        status = false;
        goto cleanup;
      }
    }
  }

cleanup:
  /* Restore saved flags */
  bsock->RestoreBlocking(flags);

  /* Clear timer */
  bsock->timer_start = 0;
  bsock->SetKillable(true);

  return status;
}

/*
 * Initiates a TLS connection with the server.
 *  Returns: true on success
 *           false on failure
 */
bool TlsBsockConnect(BareosSocket *bsock) { return GnutlsBsockSessionStart(bsock, false); }

/*
 * Listens for a TLS connection from a client.
 *  Returns: true on success
 *           false on failure
 */
bool TlsBsockAccept(BareosSocket *bsock) { return GnutlsBsockSessionStart(bsock, true); }

void TlsBsockShutdown(BareosSocket *bsock)
{
  TlsConnectionContextGnuTls *tls_conn = bsock->tls_conn;

  gnutls_bye(tls_conn->gnutls_state, GNUTLS_SHUT_WR);
}

/* Does all the manual labor for TlsBsockReadn() and TlsBsockWriten() */
static inline int GnutlsBsockReadwrite(BareosSocket *bsock, char *ptr, int nbytes, bool write)
{
  TlsConnectionContextGnuTls *tls_conn = bsock->tls_conn;
  int error;
  int flags;
  int nleft    = 0;
  int nwritten = 0;

  /* Ensure that socket is non-blocking */
  flags = bsock->SetNonblocking();

  /* start timer */
  bsock->timer_start = watchdog_time;
  bsock->ClearTimedOut();
  bsock->SetKillable(false);

  nleft = nbytes;

  while (nleft > 0) {
    if (write) {
      nwritten = gnutls_record_send(tls_conn->gnutls_state, ptr, nleft);
    } else {
      nwritten = gnutls_record_recv(tls_conn->gnutls_state, ptr, nleft);
    }

    /* Handle errors */
    if (nwritten > 0) {
      nleft -= nwritten;
      if (nleft) {
        ptr += nwritten;
      }
    } else {
      switch (nwritten) {
        case GNUTLS_E_REHANDSHAKE:
          /*
           * TLS renegotiation requested.
           */
          error = gnutls_handshake(tls_conn->gnutls_state);
          if (error != GNUTLS_E_SUCCESS) {
            goto cleanup;
          }
          break;
        case GNUTLS_E_AGAIN:
        case GNUTLS_E_INTERRUPTED:
          if (gnutls_record_get_direction(tls_conn->gnutls_state) == 1) {
            WaitForWritableFd(bsock->fd_, 10000, false);
          } else {
            WaitForReadableFd(bsock->fd_, 10000, false);
          }
          break;
        default:
          goto cleanup;
      }
    }

    /* Everything done? */
    if (nleft == 0) {
      goto cleanup;
    }

    /* Timeout/Termination, let's take what we can get */
    if (bsock->IsTimedOut() || bsock->IsTerminated()) {
      goto cleanup;
    }
  }

cleanup:
  /* Restore saved flags */
  bsock->RestoreBlocking(flags);

  /* Clear timer */
  bsock->timer_start = 0;
  bsock->SetKillable(true);

  return nbytes - nleft;
}

int TlsBsockWriten(BareosSocket *bsock, char *ptr, int32_t nbytes)
{
  return GnutlsBsockReadwrite(bsock, ptr, nbytes, true);
}

int TlsBsockReadn(BareosSocket *bsock, char *ptr, int32_t nbytes)
{
  return GnutlsBsockReadwrite(bsock, ptr, nbytes, false);
}

#else /* NOT HAVE_TLS && HAVE_GNUTLS */

bool TlsGnuTls::init() { return false; }
void TlsGnuTls::FreeTlsConnection() {}
void TlsGnuTls::FreeTlsContext(std::shared_ptr<Tls> &ctx) {}

void TlsGnuTls::SetTlsPskClientContext(const PskCredentials &credentials) {}
void TlsGnuTls::SetTlsPskServerContext() {}

bool TlsGnuTls::TlsPostconnectVerifyHost(JobControlRecord *jcr, const char *host) { return false; }
bool TlsGnuTls::TlsPostconnectVerifyCn(JobControlRecord *jcr, const std::vector<std::string> &verify_list)
{
  return false;
};

bool TlsGnuTls::TlsBsockAccept(BareosSocket *bsock) { return false; }
int TlsGnuTls::TlsBsockWriten(BareosSocket *bsock, char *ptr, int32_t nbytes) { return 0; }
int TlsGnuTls::TlsBsockReadn(BareosSocket *bsock, char *ptr, int32_t nbytes) { return 0; }
bool TlsGnuTls::TlsBsockConnect(BareosSocket *bsock) { return false; }
void TlsGnuTls::TlsBsockShutdown(BareosSocket *bsock) {}
void TlsGnuTls::TlsLogConninfo(JobControlRecord *jcr, const char *host, int port, const char *who) const {}

#endif /* HAVE_TLS && HAVE_GNUTLS */
