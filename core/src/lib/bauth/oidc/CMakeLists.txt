#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

find_package(CURL REQUIRED)
find_package(Jansson 2.11 REQUIRED) # we need at least 2.11

add_library(bauth_oidc_backend STATIC)
target_sources(bauth_oidc_backend PRIVATE oidc.cc)
target_link_libraries(
  bauth_oidc_backend PRIVATE jwt-cpp::jwt-cpp Jansson::Jansson Bareos::Lib
)

add_library(Bauth::Oidc::Backend ALIAS bauth_oidc_backend)

# these libraries are STATIC because OBJECT dependencies do not propagate, i.e.
# if a target links verifier, then it will not get common as dependency
add_library(bauth_oidc_verifier STATIC)
target_sources(bauth_oidc_verifier PRIVATE verifier.cc curl_http.cc)
target_link_libraries(
  bauth_oidc_verifier PRIVATE CURL::libcurl Bareos::Lib Jansson::Jansson
                              Bauth::Oidc::Backend
)

add_library(bauth_oidc_prover STATIC)
target_sources(bauth_oidc_prover PUBLIC prover.cc)
target_link_libraries(
  bauth_oidc_prover PUBLIC CURL::libcurl Bareos::Lib Jansson::Jansson
)

define_auth_plugin(Oidc VERIFIER bauth_oidc_verifier PROVER bauth_oidc_prover)
