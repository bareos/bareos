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
#ifndef LIB_SCAN_H_
#define LIB_SCAN_H_

DLL_IMP_EXP void strip_leading_space(char *str);
DLL_IMP_EXP void strip_trailing_junk(char *str);
DLL_IMP_EXP void strip_trailing_newline(char *str);
DLL_IMP_EXP void strip_trailing_slashes(char *dir);
DLL_IMP_EXP bool skip_spaces(char **msg);
DLL_IMP_EXP bool skip_nonspaces(char **msg);
DLL_IMP_EXP int fstrsch(const char *a, const char *b);
DLL_IMP_EXP char *next_arg(char **s);
DLL_IMP_EXP int parse_args(POOLMEM *cmd, POOLMEM *&args, int *argc,
               char **argk, char **argv, int max_args);
DLL_IMP_EXP int parse_args_only(POOLMEM *cmd, POOLMEM *&args, int *argc,
                    char **argk, char **argv, int max_args);
DLL_IMP_EXP void split_path_and_filename(const char *fname, POOLMEM *&path,
                             int *pnl, POOLMEM *&file, int *fnl);
DLL_IMP_EXP int bsscanf(const char *buf, const char *fmt, ...);

#endif // LIB_SCAN_H_
