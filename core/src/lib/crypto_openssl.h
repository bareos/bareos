/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2025 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_CRYPTO_OPENSSL_H_
#define BAREOS_LIB_CRYPTO_OPENSSL_H_

#include "crypto.h"

void OpensslPostErrors_impl(const char* file,
                            int line,
                            JobControlRecord* jcr,
                            int type,
                            const char* errstring);


static inline void OpensslPostErrors_impl(const char* file,
                                          int line,
                                          int type,
                                          const char* errstring)
{
  return OpensslPostErrors_impl(file, line, nullptr, type, errstring);
}


#define OpensslPostErrors(...) \
  OpensslPostErrors_impl(__FILE__, __LINE__, __VA_ARGS__)

int OpensslInitThreads(void);
void OpensslCleanupThreads(void);
DIGEST* OpensslDigestNew(JobControlRecord* jcr, crypto_digest_t type);

void LogSSLError(int ssl_error);

#endif  // BAREOS_LIB_CRYPTO_OPENSSL_H_
