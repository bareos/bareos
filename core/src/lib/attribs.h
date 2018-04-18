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
#ifndef LIB_ATTRIBS_H_
#define LIB_ATTRIBS_H_
#include "include/baconfig.h"

DLL_IMP_EXP void encode_stat(char *buf, struct stat *statp, int stat_size, int32_t LinkFI, int data_stream);
DLL_IMP_EXP int decode_stat(char *buf, struct stat *statp, int stat_size, int32_t *LinkFI);
DLL_IMP_EXP int32_t decode_LinkFI(char *buf, struct stat *statp, int stat_size);

#endif // LIB_ATTRIBS_H_
