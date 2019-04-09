/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, June 2013
 */
/**
 * @file
 * FileSet include handling
 */
#ifndef BAREOS_DIRD_INC_CONF_H_
#define BAREOS_DIRD_INC_CONF_H_ 1

#include "lib/keyword_table_s.h"

namespace directordaemon {

void FindUsedCompressalgos(PoolMem* compressalgos, JobControlRecord* jcr);
bool print_incexc_schema_json(PoolMem& buffer,
                              int level,
                              const int type,
                              const bool last = false);
bool print_options_schema_json(PoolMem& buffer,
                               int level,
                               const int type,
                               const bool last = false);
#ifdef HAVE_JANSSON
json_t* json_incexc(const int type);
json_t* json_options(const int type);
#endif

} /* namespace directordaemon */

#endif /* BAREOS_DIRD_INC_CONF_H_ */
