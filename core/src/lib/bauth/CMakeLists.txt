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

function(c_identifier output input)
  set(str "${input}")
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" str "${str}")
  string(REGEX REPLACE "_+" "_" str "${str}")
  string(TOUPPER "${str}" str)
  set(${output}
      "${str}"
      PARENT_SCOPE
  )
endfunction()

function(define_auth_plugin name)
  set(options)
  set(oneValueArgs VERIFIER PROVER)
  set(multiValueArgs)

  cmake_parse_arguments(
    ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN}
  )

  if(ARG_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR
        "Unparsed arguments to define_auth_plugin(${name}): ${ARG_UNPARSED_ARGUMENTS}"
    )
  endif()

  if(NOT ARG_VERIFIER)
    message(FATAL_ERROR "Missing verifier in define_auth_plugin(${name})")
  endif()

  if(NOT ARG_PROVER)
    message(FATAL_ERROR "Missing prover in define_auth_plugin(${name})")
  endif()

  c_identifier(ident ${name})
  message(DEBUG "define_auth_plugin(${name}): c-ident: ${ident}")

  add_library(BAuth::Verifier::${name} ALIAS ${ARG_VERIFIER})
  target_compile_definitions(
    ${ARG_VERIFIER}
    INTERFACE HAVE_BAUTH_VERIFIER_${ident}
    PRIVATE AUTH_NAME="${name}"
  )
  add_library(BAuth::Prover::${name} ALIAS ${ARG_PROVER})
  target_compile_definitions(
    ${ARG_PROVER}
    INTERFACE HAVE_BAUTH_PROVER_${ident}
    PRIVATE AUTH_NAME="${name}"
  )
endfunction()

add_library(bauth_cram-md5 OBJECT)
target_sources(bauth_cram-md5 PRIVATE cram_md5.cc)
target_link_libraries(bauth_cram-md5 PRIVATE OpenSSL::SSL)

add_library(bauth_trust OBJECT)
target_sources(bauth_trust PRIVATE tls.cc)

define_auth_plugin(cram-md5 VERIFIER bauth_cram-md5 PROVER bauth_cram-md5)

define_auth_plugin(trust VERIFIER bauth_trust PROVER bauth_trust)
