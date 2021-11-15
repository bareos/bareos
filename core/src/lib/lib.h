/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
 * Library includes for BAREOS lib directory
 *
 * This file contains an include for each library file
 * that we use within Bareos. include/bareos.h includes this
 * file and thus picks up everything we need in lib.
 */

#ifndef BAREOS_LIB_LIB_H_
#define BAREOS_LIB_LIB_H_

#include "lockmgr.h"
#include "base64.h"
#include "bits.h"
#include "btime.h"
#include "crypto.h"
#include "mem_pool.h"
#include "rwlock.h"
#include "serial.h"
#include "lex.h"
#include "fnmatch.h"
#include <openssl/md5.h>
#include <openssl/sha.h>
#include "bpipe.h"
#include "attr.h"
#include "var.h"
#include "guid_to_name.h"
#include "jcr.h"

#endif  // BAREOS_LIB_LIB_H_
