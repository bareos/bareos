/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_DIRD_UA_ACCT_H_
#define BAREOS_DIRD_UA_ACCT_H_

#include "dird/ua.h"

namespace directordaemon {

/**
 * Implements 'status subscriptions accounting [client=<name>]
 * [fileset=<name>]' -- computes the real, on-disk subscription/accounting
 * byte total from File.LStat (st_size/st_blocks), instead of the
 * guessed/estimated numbers used by the rest of 'status subscriptions'.
 *
 * Returns true on success (even if some tuples had to be excluded for
 * lack of file information), false on a hard error (e.g. database not
 * available, invalid argument).
 */
bool DoSubscriptionAccounting(UaContext* ua);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_ACCT_H_
