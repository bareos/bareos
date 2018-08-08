/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_TLS_OPENSSL_PRIVATE_H_
#define BAREOS_LIB_TLS_OPENSSL_PRIVATE_H_

#include <bareos.h>
#include <string>

#include <openssl/x509v3.h>

class TlsOpenSslPrivate
{
public:
   TlsOpenSslPrivate();
   ~TlsOpenSslPrivate();

   int OpensslBsockReadwrite(BareosSocket *bsock, char *ptr, int nbytes, bool write);
   bool OpensslBsockSessionStart(BareosSocket *bsock, bool server);

   void ClientContextInsertCredentials(const PskCredentials &cred);
   void ServerContextInsertCredentials(const PskCredentials &cred);

/* callbacks */
   static int tls_pem_callback_dispatch(char *buf, int size, int rwflag, void *userdata);
   static int OpensslVerifyPeer(int ok, X509_STORE_CTX *store);
   static unsigned int psk_server_cb(SSL *ssl,
                                  const char *identity,
                                  unsigned char *psk,
                                  unsigned int max_psk_len);
   static unsigned int psk_client_cb(SSL *ssl,
                                  const char * /*hint*/,
                                  char *identity,
                                  unsigned int max_identity_len,
                                  unsigned char *psk,
                                  unsigned int max_psk_len);
/* *************** */

   SSL                  *openssl_;
   SSL_CTX              *openssl_ctx_;

/* psk attributes list for all connections */
   static std::map<const SSL_CTX *, PskCredentials> psk_server_credentials;
   static std::map<const SSL_CTX *, PskCredentials> psk_client_credentials;
/* *************** */

/* cert attributes */
   int                   tcp_file_descriptor_;
   std::string           ca_certfile_;
   std::string           ca_certdir_;
   std::string           crlfile_;
   std::string           certfile_;
   std::string           keyfile_;
   CRYPTO_PEM_PASSWD_CB *pem_callback_;
   void                 *pem_userdata_;
   std::string           dhfile_;
   std::string           cipherlist_;
   bool                  verify_peer_;
/* *************** */
};


#endif /* BAREOS_LIB_TLS_OPENSSL_PRIVATE_H_ */
