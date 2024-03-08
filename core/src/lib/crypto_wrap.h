
/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2024 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_CRYPTO_WRAP_H_
#define BAREOS_LIB_CRYPTO_WRAP_H_

#include <cinttypes>
#include <string>
#include <optional>

/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @plain: plaintext key to be wrapped, n * 64 bit
 * @cipher: wrapped key, (n + 1) * 64 bit
 */
std::optional<std::string> AesWrap(const uint8_t* kek,
                                   int n,
                                   const uint8_t* plain,
                                   uint8_t* cipher);

/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @cipher: wrapped key to be unwrapped, (n + 1) * 64 bit
 * @plain: plaintext key, n * 64 bit
 */
std::optional<std::string> AesUnwrap(const uint8_t* kek,
                                     int n,
                                     const uint8_t* cipher,
                                     uint8_t* plain);

#endif  // BAREOS_LIB_CRYPTO_WRAP_H_
