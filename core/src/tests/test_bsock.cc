/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "bsock_test.h"
#include "create_resource.h"
#include "tests/bareos_test_sockets.h"
#include "tests/init_openssl.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>

#include <thread>
#include <future>
#include <unistd.h>

#include "console/console.h"
#include "console/console_conf.h"
#include "console/console_globals.h"
#include "stored/stored_conf.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"

#include "lib/tls/openssl.h"
#include "lib/bsock_tcp.h"
#include "lib/bnet.h"
#include "lib/bstringlist.h"
#include "lib/version.h"

#include "include/jcr.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>

#define CLIENT_AS_A_THREAD 0

class StorageResource;
#include "dird/authenticate.h"

#define MIN_MSG_LEN 15
#define MAX_MSG_LEN 175 + 25
#define CONSOLENAME "testconsole"
#define CONSOLEPASSWORD "secret"

static std::string cipher_server, cipher_client;

static std::unique_ptr<directordaemon::ConsoleResource> dir_cons_config;
static std::unique_ptr<directordaemon::DirectorResource> dir_dir_config;
static std::unique_ptr<console::DirectorResource> cons_dir_config;
static std::unique_ptr<console::ConsoleResource> cons_cons_config;

namespace {
using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpPkeyCtxPtr
    = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;
using X509CrlPtr = std::unique_ptr<X509_CRL, decltype(&X509_CRL_free)>;
using X509RevokedPtr
    = std::unique_ptr<X509_REVOKED, decltype(&X509_REVOKED_free)>;
using Asn1IntegerPtr
    = std::unique_ptr<ASN1_INTEGER, decltype(&ASN1_INTEGER_free)>;
using Asn1TimePtr = std::unique_ptr<ASN1_TIME, decltype(&ASN1_TIME_free)>;
using X509ExtensionPtr
    = std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)>;

struct GeneratedTlsArtifacts {
  std::filesystem::path directory;
  std::filesystem::path ca_cert;
  std::filesystem::path server_cert;
  std::filesystem::path server_key;
  std::filesystem::path client_cert;
  std::filesystem::path client_key;
  std::filesystem::path crl;

  GeneratedTlsArtifacts() = default;
  GeneratedTlsArtifacts(const GeneratedTlsArtifacts&) = delete;
  GeneratedTlsArtifacts& operator=(const GeneratedTlsArtifacts&) = delete;
  GeneratedTlsArtifacts(GeneratedTlsArtifacts&&) = default;
  GeneratedTlsArtifacts& operator=(GeneratedTlsArtifacts&&) = default;

  ~GeneratedTlsArtifacts()
  {
    if (!directory.empty()) {
      std::error_code ec;
      std::filesystem::remove_all(directory, ec);
    }
  }
};

std::string GetOpenSslError()
{
  const unsigned long error = ERR_get_error();
  if (!error) { return "unknown OpenSSL error"; }

  char buffer[256];
  ERR_error_string_n(error, buffer, sizeof(buffer));
  return buffer;
}

bool WritePrivateKeyPem(const std::filesystem::path& path,
                        EVP_PKEY* key,
                        std::string* error)
{
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    *error = "Could not open " + path.string() + " for writing";
    return false;
  }

  const bool ok
      = PEM_write_PrivateKey(file, key, nullptr, nullptr, 0, nullptr, nullptr)
        == 1;
  fclose(file);

  if (!ok) {
    *error = "Could not write private key " + path.string() + ": "
             + GetOpenSslError();
  }

  return ok;
}

bool WriteCertificatePem(const std::filesystem::path& path,
                         X509* cert,
                         std::string* error)
{
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    *error = "Could not open " + path.string() + " for writing";
    return false;
  }

  const bool ok = PEM_write_X509(file, cert) == 1;
  fclose(file);

  if (!ok) {
    *error = "Could not write certificate " + path.string() + ": "
             + GetOpenSslError();
  }

  return ok;
}

bool WriteCrlPem(const std::filesystem::path& path,
                 X509_CRL* crl,
                 std::string* error)
{
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    *error = "Could not open " + path.string() + " for writing";
    return false;
  }

  const bool ok = PEM_write_X509_CRL(file, crl) == 1;
  fclose(file);

  if (!ok) {
    *error = "Could not write CRL " + path.string() + ": " + GetOpenSslError();
  }

  return ok;
}

EvpPkeyPtr GenerateRsaKey(std::string* error)
{
  EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr),
                    EVP_PKEY_CTX_free);
  if (!ctx) {
    *error = "Could not create key generation context: " + GetOpenSslError();
    return {nullptr, EVP_PKEY_free};
  }

  if (EVP_PKEY_keygen_init(ctx.get()) <= 0
      || EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048) <= 0) {
    *error = "Could not initialize RSA key generation: " + GetOpenSslError();
    return {nullptr, EVP_PKEY_free};
  }

  EVP_PKEY* raw_key = nullptr;
  if (EVP_PKEY_keygen(ctx.get(), &raw_key) <= 0) {
    *error = "Could not generate RSA key: " + GetOpenSslError();
    return {nullptr, EVP_PKEY_free};
  }

  return EvpPkeyPtr(raw_key, EVP_PKEY_free);
}

bool AddExtension(X509* cert,
                  X509* issuer_cert,
                  int nid,
                  const char* value,
                  std::string* error)
{
  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, issuer_cert ? issuer_cert : cert, cert, nullptr, nullptr,
                 0);

  X509ExtensionPtr extension(
      X509V3_EXT_conf_nid(nullptr, &ctx, nid, const_cast<char*>(value)),
      X509_EXTENSION_free);
  if (!extension) {
    *error = "Could not create X509 extension: " + GetOpenSslError();
    return false;
  }

  if (X509_add_ext(cert, extension.get(), -1) != 1) {
    *error = "Could not add X509 extension: " + GetOpenSslError();
    return false;
  }

  return true;
}

X509Ptr GenerateCertificate(EVP_PKEY* subject_key,
                            std::string_view common_name,
                            long serial,
                            int valid_days,
                            EVP_PKEY* issuer_key,
                            X509* issuer_cert,
                            bool is_ca,
                            const char* extended_key_usage,
                            std::string* error)
{
  X509Ptr cert(X509_new(), X509_free);
  if (!cert) {
    *error = "Could not create X509 certificate: " + GetOpenSslError();
    return {nullptr, X509_free};
  }

  if (X509_set_version(cert.get(), 2) != 1
      || ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), serial) != 1
      || X509_gmtime_adj(X509_get_notBefore(cert.get()), 0) == nullptr
      || X509_gmtime_adj(X509_get_notAfter(cert.get()),
                         60L * 60 * 24 * valid_days)
             == nullptr
      || X509_set_pubkey(cert.get(), subject_key) != 1) {
    *error = "Could not initialize X509 certificate: " + GetOpenSslError();
    return {nullptr, X509_free};
  }

  X509_NAME* subject = X509_get_subject_name(cert.get());
  if (!subject
      || X509_NAME_add_entry_by_txt(
             subject, "CN", MBSTRING_ASC,
             reinterpret_cast<const unsigned char*>(common_name.data()), -1, -1,
             0)
             != 1) {
    *error = "Could not set certificate subject: " + GetOpenSslError();
    return {nullptr, X509_free};
  }

  X509_NAME* issuer_name = issuer_cert ? X509_get_subject_name(issuer_cert)
                                       : X509_get_subject_name(cert.get());
  if (!issuer_name || X509_set_issuer_name(cert.get(), issuer_name) != 1) {
    *error = "Could not set certificate issuer: " + GetOpenSslError();
    return {nullptr, X509_free};
  }

  if (is_ca) {
    if (!AddExtension(cert.get(), issuer_cert, NID_basic_constraints,
                      "critical,CA:TRUE", error)
        || !AddExtension(cert.get(), issuer_cert, NID_key_usage,
                         "critical,keyCertSign,cRLSign", error)) {
      return {nullptr, X509_free};
    }
  } else {
    if (!AddExtension(cert.get(), issuer_cert, NID_basic_constraints,
                      "critical,CA:FALSE", error)
        || !AddExtension(cert.get(), issuer_cert, NID_key_usage,
                         "critical,digitalSignature,keyEncipherment", error)
        || !AddExtension(cert.get(), issuer_cert, NID_ext_key_usage,
                         extended_key_usage, error)) {
      return {nullptr, X509_free};
    }
  }

  if (X509_sign(cert.get(), issuer_key ? issuer_key : subject_key, EVP_sha256())
      <= 0) {
    *error = "Could not sign certificate: " + GetOpenSslError();
    return {nullptr, X509_free};
  }

  return cert;
}

X509CrlPtr GenerateCrl(X509* ca_cert,
                       EVP_PKEY* ca_key,
                       long revoked_serial,
                       std::string* error)
{
  X509CrlPtr crl(X509_CRL_new(), X509_CRL_free);
  if (!crl) {
    *error = "Could not create X509 CRL: " + GetOpenSslError();
    return {nullptr, X509_CRL_free};
  }

  Asn1TimePtr last_update(ASN1_TIME_new(), ASN1_TIME_free);
  Asn1TimePtr next_update(ASN1_TIME_new(), ASN1_TIME_free);
  Asn1TimePtr revocation_date(ASN1_TIME_new(), ASN1_TIME_free);
  if (!last_update || !next_update || !revocation_date) {
    *error = "Could not allocate ASN1_TIME objects";
    return {nullptr, X509_CRL_free};
  }

  if (X509_CRL_set_version(crl.get(), 1) != 1
      || X509_CRL_set_issuer_name(crl.get(), X509_get_subject_name(ca_cert))
             != 1
      || ASN1_TIME_adj(last_update.get(), time(nullptr), 0, 0) == nullptr
      || ASN1_TIME_adj(next_update.get(), time(nullptr), 7, 0) == nullptr
      || ASN1_TIME_adj(revocation_date.get(), time(nullptr), 0, 0) == nullptr
      || X509_CRL_set1_lastUpdate(crl.get(), last_update.get()) != 1
      || X509_CRL_set1_nextUpdate(crl.get(), next_update.get()) != 1) {
    *error = "Could not initialize CRL: " + GetOpenSslError();
    return {nullptr, X509_CRL_free};
  }

  X509RevokedPtr revoked(X509_REVOKED_new(), X509_REVOKED_free);
  Asn1IntegerPtr serial_number(ASN1_INTEGER_new(), ASN1_INTEGER_free);
  if (!revoked || !serial_number) {
    *error = "Could not allocate revoked certificate entry";
    return {nullptr, X509_CRL_free};
  }

  if (ASN1_INTEGER_set(serial_number.get(), revoked_serial) != 1
      || X509_REVOKED_set_serialNumber(revoked.get(), serial_number.get()) != 1
      || X509_REVOKED_set_revocationDate(revoked.get(), revocation_date.get())
             != 1
      || X509_CRL_add0_revoked(crl.get(), revoked.get()) != 1) {
    *error = "Could not add revoked certificate to CRL: "
             + GetOpenSslError();
    return {nullptr, X509_CRL_free};
  }
  revoked.release();

  if (X509_CRL_sort(crl.get()) != 1
      || X509_CRL_sign(crl.get(), ca_key, EVP_sha256()) <= 0) {
    *error = "Could not sign CRL: " + GetOpenSslError();
    return {nullptr, X509_CRL_free};
  }

  return crl;
}

bool GenerateTlsArtifactsForRevocationTest(GeneratedTlsArtifacts* artifacts,
                                           std::string* error)
{
  char temp_directory[] = "/tmp/bsock-crl-XXXXXX";
  char* created_directory = mkdtemp(temp_directory);
  if (!created_directory) {
    *error = "Could not create temporary directory";
    return false;
  }

  artifacts->directory = created_directory;
  artifacts->ca_cert = artifacts->directory / "ca-cert.pem";
  artifacts->server_cert = artifacts->directory / "server-cert.pem";
  artifacts->server_key = artifacts->directory / "server-key.pem";
  artifacts->client_cert = artifacts->directory / "client-cert.pem";
  artifacts->client_key = artifacts->directory / "client-key.pem";
  artifacts->crl = artifacts->directory / "revoked-client.crl.pem";

  auto ca_key = GenerateRsaKey(error);
  if (!ca_key) { return false; }

  auto ca_cert = GenerateCertificate(ca_key.get(), "Bareos Test CA", 1, 30,
                                     ca_key.get(), nullptr, true, nullptr,
                                     error);
  if (!ca_cert) { return false; }

  auto server_key = GenerateRsaKey(error);
  if (!server_key) { return false; }

  auto server_cert = GenerateCertificate(server_key.get(), "bareos-server", 2,
                                         30, ca_key.get(), ca_cert.get(), false,
                                         "serverAuth", error);
  if (!server_cert) { return false; }

  auto client_key = GenerateRsaKey(error);
  if (!client_key) { return false; }

  auto client_cert = GenerateCertificate(client_key.get(), "bareos-client", 3,
                                         30, ca_key.get(), ca_cert.get(), false,
                                         "clientAuth", error);
  if (!client_cert) { return false; }

  auto crl = GenerateCrl(ca_cert.get(), ca_key.get(), 3, error);
  if (!crl) { return false; }

  return WritePrivateKeyPem(artifacts->server_key, server_key.get(), error)
         && WriteCertificatePem(artifacts->server_cert, server_cert.get(), error)
         && WritePrivateKeyPem(artifacts->client_key, client_key.get(), error)
         && WriteCertificatePem(artifacts->client_cert, client_cert.get(), error)
         && WriteCertificatePem(artifacts->ca_cert, ca_cert.get(), error)
         && WriteCrlPem(artifacts->crl, crl.get(), error);
}
}  // namespace

static void InitSignalHandler()
{
#if !defined(HAVE_WIN32)
  struct sigaction sig = {};
  sig.sa_handler = SIG_IGN;
  sigaction(SIGUSR2, &sig, nullptr);
  sigaction(SIGPIPE, &sig, nullptr);
#endif
}

void InitForTest()
{
  OSDependentInit();
  InitOpenSsl();
  InitSignalHandler();

  dir_cons_config.reset(
      directordaemon::CreateAndInitializeNewConsoleResource());
  dir_dir_config.reset(
      directordaemon::CreateAndInitializeNewDirectorResource());
  cons_dir_config.reset(console::CreateAndInitializeNewDirectorResource());
  cons_cons_config.reset(console::CreateAndInitializeNewConsoleResource());

  directordaemon::me = dir_dir_config.get();
  console::me = cons_cons_config.get();

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  debug_level = 0;
  SetTrace(0);

  working_directory = "/tmp";
  MyNameIs(0, NULL, "bsock_test");
}

static void clone_a_server_socket(BareosSocket* bs)
{
  std::unique_ptr<BareosSocket> bs2(bs->clone());
  bs2->fsend("cloned-bareos-socket-0987654321");
  bs2->close();

  bs->fsend("bareos-socket-1234567890");
}

static void start_bareos_server(std::promise<bool>* promise,
                                std::string console_name,
                                std::string console_password,
                                std::string,
                                const listening_socket& ls)

{
  int newsockfd = accept_socket(ls);

  if (newsockfd < 0) { return; }

  std::unique_ptr<BareosSocket> bs(create_new_bareos_socket(newsockfd));

  char* name = (char*)console_name.c_str();
  s_password password;
  password.encoding = p_encoding_md5;
  password.value = (char*)console_password.c_str();

  bool success = false;
  if (bs->recv() <= 0) {
    Dmsg1(10, T_("Connection request from %s failed.\n"), bs->who());
  } else if (bs->message_length < MIN_MSG_LEN
             || bs->message_length > MAX_MSG_LEN) {
    Dmsg2(10, T_("Invalid connection from %s. Len=%d\n"), bs->who(),
          bs->message_length);
  } else {
    Dmsg1(10, "Cons->Dir: %s", bs->msg);
    if (!bs->AuthenticateInboundConnection(NULL, nullptr, name, password,
                                           dir_cons_config.get())) {
      Dmsg0(10, "Server: inbound auth failed\n");
    } else {
      bs->fsend(T_("1000 OK: %s Version: %s (%s)\n"), my_name,
                kBareosVersionStrings.Full, kBareosVersionStrings.Date);
      Dmsg0(10, "Server: inbound auth successful\n");
      std::string cipher;
      if (bs->tls_conn) {
        cipher = bs->tls_conn->TlsCipherGetName();
        Dmsg1(10, "Server used cipher: <%s>\n", cipher.c_str());
        cipher_server = cipher;
      }
      if (dir_cons_config->IsTlsConfigured()) {
        Dmsg0(10, bs->TlsEstablished() ? "Tls enable\n"
                                       : "Tls failed to establish\n");
        success = bs->TlsEstablished();
      } else {
        Dmsg0(10, "Tls disabled by command\n");
        if (bs->TlsEstablished()) {
          Dmsg0(10, "bs->tls_established_ should be false but is true\n");
        }
        success = !bs->TlsEstablished();
      }
    }
  }
  if (success) { clone_a_server_socket(bs.get()); }
  bs->close();
  promise->set_value(success);
}

static void clone_a_client_socket(std::shared_ptr<BareosSocket> UA_sock)
{
  std::string received_msg;
  std::string orig_msg2("cloned-bareos-socket-0987654321");
  std::unique_ptr<BareosSocket> UA_sock2(UA_sock->clone());
  UA_sock2->recv();
  received_msg = UA_sock2->msg;
  EXPECT_STREQ(orig_msg2.c_str(), received_msg.c_str());
  UA_sock2->close();

  std::string orig_msg("bareos-socket-1234567890");
  UA_sock->recv();
  received_msg = UA_sock->msg;
  EXPECT_STREQ(orig_msg.c_str(), received_msg.c_str());
}

#if CLIENT_AS_A_THREAD
static int connect_to_server(std::string console_name,
                             std::string console_password,
                             std::string server_address,
                             int server_port)
#else
static bool connect_to_server(std::string console_name,
                              std::string console_password,
                              std::string server_address,
                              int server_port)
#endif
{
  utime_t heart_beat = 0;

  JobControlRecord jcr;

  char* name = (char*)console_name.c_str();

  s_password password;
  password.encoding = p_encoding_md5;
  password.value = (char*)console_password.c_str();

  std::shared_ptr<BareosSocketTCP> UA_sock(new BareosSocketTCP);
  UA_sock->sleep_time_after_authentication_error = 0;
  jcr.dir_bsock = UA_sock.get();

  bool success = false;

  if (!UA_sock->connect(NULL, 1, 15, heart_beat, "Director daemon",
                        (char*)server_address.c_str(), NULL, server_port,
                        false)) {
    Dmsg0(10, "socket connect failed\n");
  } else {
    Dmsg0(10, "socket connect OK\n");
    uint32_t response_id = kMessageIdUnknown;
    BStringList response_args;
    if (!UA_sock->ConsoleAuthenticateWithDirector(
            &jcr, name, password, cons_dir_config.get(), console_name,
            response_args, response_id)) {
      Emsg0(M_ERROR, 0, "Authenticate Failed\n");
    } else {
      EXPECT_EQ(response_id, kMessageIdOk) << "Received the wrong message id.";
      Dmsg0(10, "Authenticate Connect to Server successful!\n");
      std::string cipher;
      if (UA_sock->tls_conn) {
        cipher = UA_sock->tls_conn->TlsCipherGetName();
        Dmsg1(10, "Client used cipher: <%s>\n", cipher.c_str());
        cipher_client = cipher;
      }
      if (cons_dir_config->IsTlsConfigured()) {
        Dmsg0(10, UA_sock->TlsEstablished() ? "Tls enable\n"
                                            : "Tls failed to establish\n");
        success = UA_sock->TlsEstablished();
      } else {
        Dmsg0(10, "Tls disabled by command\n");
        if (UA_sock->TlsEstablished()) {
          Dmsg0(10, "UA_sock->tls_established_ should be false but is true\n");
        }
        success = !UA_sock->TlsEstablished();
      }
    }
  }
  if (success) { clone_a_client_socket(UA_sock); }
  if (UA_sock) {
    UA_sock->close();
    jcr.dir_bsock = nullptr;
  }
  return success;
}

std::string client_cons_name;
std::string client_cons_password;

std::string server_cons_name;
std::string server_cons_password;

TEST(bsock, auth_works)
{
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  client_cons_name = "clientname";
  client_cons_password = "verysecretpassword";

  server_cons_name = client_cons_name;
  server_cons_password = client_cons_password;

  InitForTest();

  cons_dir_config->tls_enable_ = false;
  dir_cons_config->tls_enable_ = false;

  auto ls = create_listening_socket();
  ASSERT_NE(ls, std::nullopt);

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, server_cons_name,
                            server_cons_password, HOST, std::ref(*ls));

  Dmsg0(10, "connecting to server\n");
  EXPECT_TRUE(connect_to_server(client_cons_name, client_cons_password, HOST,
                                ls->port));

  server_thread.join();
  EXPECT_TRUE(future.get());
}


TEST(bsock, auth_works_with_different_names)
{
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  client_cons_name = "clientname";
  client_cons_password = "verysecretpassword";

  server_cons_name = "differentclientname";
  server_cons_password = client_cons_password;

  InitForTest();

  cons_dir_config->tls_enable_ = false;
  dir_cons_config->tls_enable_ = false;

  auto ls = create_listening_socket();
  ASSERT_NE(ls, std::nullopt);

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, server_cons_name,
                            server_cons_password, HOST, std::ref(*ls));

  Dmsg0(10, "connecting to server\n");
  EXPECT_TRUE(connect_to_server(client_cons_name, client_cons_password, HOST,
                                ls->port));

  server_thread.join();
  EXPECT_TRUE(future.get());
}

TEST(bsock, auth_fails_with_different_passwords)
{
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  client_cons_name = "clientname";
  client_cons_password = "verysecretpassword";

  server_cons_name = client_cons_name;
  server_cons_password = "othersecretpassword";

  InitForTest();

  cons_dir_config->tls_enable_ = false;
  dir_cons_config->tls_enable_ = false;

  auto ls = create_listening_socket();
  ASSERT_NE(ls, std::nullopt);

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, server_cons_name,
                            server_cons_password, HOST, std::ref(*ls));

  Dmsg0(10, "connecting to server\n");
  EXPECT_FALSE(connect_to_server(client_cons_name, client_cons_password, HOST,
                                 ls->port));


  server_thread.join();
  EXPECT_FALSE(future.get());
}

TEST(bsock, auth_works_with_tls_cert)
{
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  client_cons_name = "clientname";
  client_cons_password = "verysecretpassword";

  server_cons_name = client_cons_name;
  server_cons_password = client_cons_password;

  InitForTest();

  cons_dir_config->tls_enable_ = true;
  dir_cons_config->tls_enable_ = true;

  auto ls = create_listening_socket();
  ASSERT_NE(ls, std::nullopt);

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, server_cons_name,
                            server_cons_password, HOST, std::ref(*ls));

  Dmsg0(10, "connecting to server\n");

#if CLIENT_AS_A_THREAD
  std::thread client_thread(connect_to_server, client_cons_name,
                            client_cons_password, HOST, ls->port,
                            cons_dir_config.get());
  client_thread.join();
#else
  EXPECT_TRUE(connect_to_server(client_cons_name, client_cons_password, HOST,
                                ls->port));
#endif

  server_thread.join();

  EXPECT_TRUE(cipher_server == cipher_client);
  EXPECT_TRUE(future.get());
}

TEST(bsock, auth_fails_with_revoked_tls_cert)
{
  std::promise<bool> promise;
  std::future<bool> future = promise.get_future();

  client_cons_name = "clientname";
  client_cons_password = "verysecretpassword";

  server_cons_name = client_cons_name;
  server_cons_password = client_cons_password;

  InitForTest();

  GeneratedTlsArtifacts artifacts;
  std::string error;
  ASSERT_TRUE(GenerateTlsArtifactsForRevocationTest(&artifacts, &error))
      << error;

  cipher_server.clear();
  cipher_client.clear();

  cons_dir_config->tls_enable_ = true;
  cons_dir_config->tls_cert_.verify_peer_ = false;
  cons_dir_config->tls_cert_.ca_certfile_ = artifacts.ca_cert.string();
  cons_dir_config->tls_cert_.certfile_ = artifacts.client_cert.string();
  cons_dir_config->tls_cert_.keyfile_ = artifacts.client_key.string();
  cons_dir_config->tls_cert_.crlfile_.clear();

  dir_cons_config->tls_enable_ = true;
  dir_cons_config->tls_cert_.verify_peer_ = true;
  dir_cons_config->tls_cert_.ca_certfile_ = artifacts.ca_cert.string();
  dir_cons_config->tls_cert_.certfile_ = artifacts.server_cert.string();
  dir_cons_config->tls_cert_.keyfile_ = artifacts.server_key.string();
  dir_cons_config->tls_cert_.crlfile_ = artifacts.crl.string();
  dir_cons_config->tls_cert_.allowed_certificate_common_names_.clear();

  auto ls = create_listening_socket();
  ASSERT_NE(ls, std::nullopt);

  Dmsg0(10, "starting listen thread...\n");
  std::thread server_thread(start_bareos_server, &promise, server_cons_name,
                            server_cons_password, HOST, std::ref(*ls));

  Dmsg0(10, "connecting to server\n");
  EXPECT_FALSE(connect_to_server(client_cons_name, client_cons_password, HOST,
                                 ls->port));

  server_thread.join();
  EXPECT_FALSE(future.get());
}

class BareosSocketTCPMock : public BareosSocketTCP {
 public:
  BareosSocketTCPMock(std::string& t) : test_variable_(t) {}

  virtual ~BareosSocketTCPMock() { test_variable_ = "Destructor Called"; }
  std::string& test_variable_;
};

TEST(bsock, create_bareos_socket_unique_ptr)
{
  std::string test_variable;
  {
    std::unique_ptr<BareosSocketTCPMock, std::function<void(BareosSocket*)>> p;
    {
      std::unique_ptr<BareosSocketTCPMock, std::function<void(BareosSocket*)>>
          p1(new BareosSocketTCPMock(test_variable),
             [](BareosSocket* ptr) { delete ptr; });
      EXPECT_NE(p1.get(), nullptr);
      p = std::move(p1);
      EXPECT_EQ(p1.get(), nullptr);
      EXPECT_TRUE(test_variable.empty());
    }
    EXPECT_NE(p.get(), nullptr);
  }
  EXPECT_STREQ(test_variable.c_str(), "Destructor Called");
}

TEST(BNet, FormatAndSendResponseMessage)
{
  std::unique_ptr<TestSockets> test_sockets(
      create_connected_server_and_client_bareos_socket());
  EXPECT_NE(test_sockets.get(), nullptr)
      << "Could not create Bareos test sockets.";
  if (!test_sockets) { return; }

  std::string m("Test123");

  test_sockets->client->FormatAndSendResponseMessage(kMessageIdOk, m);

  uint32_t id = kMessageIdUnknown;
  BStringList args;
  bool ok = test_sockets->server->ReceiveAndEvaluateResponseMessage(id, args);

  EXPECT_TRUE(ok) << "ReceiveAndEvaluateResponseMessage errored.";
  EXPECT_EQ(id, kMessageIdOk) << "Wrong MessageID received.";

  std::string test("1000 Test123");
  EXPECT_STREQ(args.JoinReadable().c_str(), test.c_str());
}
