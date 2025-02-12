/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_TLS_RESOURCE_ITEMS_H_
#define BAREOS_LIB_TLS_RESOURCE_ITEMS_H_

/* clang-format off */

// Common TLS-Settings for both (Certificate and PSK).
#define TLS_COMMON_CONFIG(res) \
  { "TlsAuthenticate", CFG_TYPE_BOOL, ITEM(res, authenticate_), {config::DefaultValue{"false"}, config::Description{"Use TLS only to authenticate, not for encryption."}}}, \
  { "TlsEnable", CFG_TYPE_BOOL, ITEM(res, tls_enable_), {config::DefaultValue{"true"}, config::Description{"Enable TLS support."}}}, \
  { "TlsRequire", CFG_TYPE_BOOL, ITEM(res, tls_require_), {config::DefaultValue{"true"}, config::Description{"If set to \"no\", Bareos can fall back to use unencrypted connections. "}}}, \
  { "EnableKtls", CFG_TYPE_BOOL, ITEM(res, enable_ktls_), {config::DefaultValue{"false"}, config::Description{"If set to \"yes\", Bareos will allow the SSL implementation to use Kernel TLS. "}}}, \
  { "TlsCipherList", CFG_TYPE_STDSTR, ITEM(res, cipherlist_), {config::Description{"Colon separated list of valid TLSv1.2 and lower Ciphers; see \"openssl ciphers\" command. Leftmost element has the highest priority."}, config::PlatformSpecific{}}}, \
  { "TlsCipherSuites", CFG_TYPE_STDSTR, ITEM(res, ciphersuites_), {config::Description{"Colon separated list of valid TLSv1.3 Ciphers; see \"openssl ciphers -s -tls1_3\" command. Leftmost element has the highest priority. Currently only SHA256 ciphers are supported."}, config::PlatformSpecific{}}}, \
  { "TlsDhFile", CFG_TYPE_STDSTRDIR, ITEM(res, tls_cert_.dhfile_), {config::Description{"Path to PEM encoded Diffie-Hellman parameter file. If this directive is specified, DH key exchange will be used for the ephemeral keying, allowing for forward secrecy of communications."}}}, \
  { "TlsProtocol", CFG_TYPE_STDSTR, ITEM(res, protocol_), {config::IntroducedIn{20, 0, 0}, config::Description{"OpenSSL Configuration: Protocol"}, config::PlatformSpecific{}}}

// TLS Settings for Certificate only
#define TLS_CERT_CONFIG(res)                                                \
  { "TlsVerifyPeer", CFG_TYPE_BOOL, ITEM(res, tls_cert_.verify_peer_), {config::DefaultValue{"false"}, config::Description{"If disabled, all certificates signed by a known CA will be accepted. If enabled, the CN of a certificate must the Address or in the \"TLS Allowed CN\" list."}}}, \
  { "TlsCaCertificateFile", CFG_TYPE_STDSTRDIR, ITEM(res, tls_cert_.ca_certfile_), {config::Description{"Path of a PEM encoded TLS CA certificate(s) file."}}}, \
  { "TlsCaCertificateDir", CFG_TYPE_STDSTRDIR, ITEM(res, tls_cert_.ca_certdir_), {config::Description{"Path of a TLS CA certificate directory."}}}, \
  { "TlsCertificateRevocationList", CFG_TYPE_STDSTRDIR, ITEM(res, tls_cert_.crlfile_), {config::Description{"Path of a Certificate Revocation List file."}}}, \
  { "TlsCertificate", CFG_TYPE_STDSTRDIR, ITEM(res, tls_cert_.certfile_), {config::Description{"Path of a PEM encoded TLS certificate."}}}, \
  { "TlsKey", CFG_TYPE_STDSTRDIR, ITEM(res, tls_cert_.keyfile_), {config::Description{"Path of a PEM encoded private key. It must correspond to the specified \"TLS Certificate\"."}}}, \
  { "TlsAllowedCn", CFG_TYPE_STR_VECTOR, ITEM(res, tls_cert_.allowed_certificate_common_names_), {config::Description{"\"Common Name\"s (CNs) of the allowed peer certificates."}}}

/* clang-format on */
#endif  // BAREOS_LIB_TLS_RESOURCE_ITEMS_H_
