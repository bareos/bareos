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
#ifndef LIB_PATH_LIST_H_
#define LIB_PATH_LIST_H_

DLL_IMP_EXP htable *path_list_init();
DLL_IMP_EXP bool path_list_lookup(htable *path_list, const char *fname);
DLL_IMP_EXP bool path_list_add(htable *path_list, uint32_t len, const char *fname);
DLL_IMP_EXP void free_path_list(htable *path_list);

#endif // LIB_PATH_LIST_H_
