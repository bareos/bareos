/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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
 * Directory tree load/save routines
 */

#ifndef BAREOS_LIB_TREE_SAVE_H_
#define BAREOS_LIB_TREE_SAVE_H_

#include "lib/tree.h"

bool SaveTree(const char *path, TREE_ROOT* root);
TREE_ROOT* LoadTree(const char *path, std::size_t* size, bool mark_on_load);


#endif  // BAREOS_LIB_TREE_SAVE_H_
