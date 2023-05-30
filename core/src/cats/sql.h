/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2023 Bareos GmbH & Co. KG

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
#ifndef BAREOS_CATS_SQL_H_
#define BAREOS_CATS_SQL_H_
int db_int64_handler(void* ctx, int num_fields, char** row);
int DbListHandler(void* ctx, int num_fields, char** row);
int DbIdListHandler(void* ctx, int num_fields, char** row);
void DbDebugPrint(JobControlRecord* jcr, FILE* fp);
int DbIntHandler(void* ctx, int num_fields, char** row);

// This template allows you to easily use an object with operator() or a lambda
//      SqlQuery(query, ObjectHandler<decltype(obj)>, &obj)
template <typename T> int ObjectHandler(void* ctx, int num_fields, char** row)
{
  return static_cast<T*>(ctx)->operator()(num_fields, row) ? 0 : 1;
}

#endif  // BAREOS_CATS_SQL_H_
