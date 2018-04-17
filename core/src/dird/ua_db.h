/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#ifndef DIRD_UA_DB_H_
#define DIRD_UA_DB_H_

bool open_client_db(UaContext *ua, bool use_private = false);
bool open_db(UaContext *ua, bool use_private = false);
void close_db(UaContext *ua);
int create_pool(JobControlRecord *jcr, BareosDb *db, PoolResource *pool, e_pool_op op);
void set_pool_dbr_defaults_in_media_dbr(MediaDbRecord *mr, PoolDbRecord *pr);
bool set_pooldbr_references(JobControlRecord *jcr, BareosDb *db, PoolDbRecord *pr, PoolResource *pool);
void set_pooldbr_from_poolres(PoolDbRecord *pr, PoolResource *pool, e_pool_op op);
int update_pool_references(JobControlRecord *jcr, BareosDb *db, PoolResource *pool);

#endif // DIRD_UA_DB_H_
