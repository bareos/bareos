/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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
/**
 * @file
 * Bareos-specific base64 encoding used in CRAM-MD5 authentication.
 *
 * This is a port of BinToBase64(..., compatible=false) from
 * core/src/lib/base64.cc.  The "not compatible" variant uses signed
 * byte interpretation (int8_t) instead of unsigned (uint8_t) when
 * building the bit register.  This produces different output from
 * standard RFC 4648 base64 for any byte value >= 128.
 */
#ifndef BAREOS_WEBUI_PROXY_BAREOS_BASE64_H_
#define BAREOS_WEBUI_PROXY_BAREOS_BASE64_H_

#include <cstdint>
#include <string>

/**
 * Encode @p len raw bytes from @p data using the Bareos-specific base64
 * variant (compatible=false).  The result is a printable ASCII string
 * without padding characters.
 */
std::string BareosBase64Encode(const uint8_t* data, int len);

#endif  // BAREOS_WEBUI_PROXY_BAREOS_BASE64_H_
