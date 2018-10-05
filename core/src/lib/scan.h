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
#ifndef BAREOS_LIB_SCAN_H_
#define BAREOS_LIB_SCAN_H_

void StripLeadingSpace(char *str);
void StripTrailingJunk(char *str);
void StripTrailingNewline(char *str);
void StripTrailingSlashes(char *dir);
bool SkipSpaces(char **msg);
bool SkipNonspaces(char **msg);
int fstrsch(const char *a, const char *b);
char *next_arg(char **s);
int ParseArgs(POOLMEM *cmd, POOLMEM *&args, int *argc,
               char **argk, char **argv, int max_args);
int ParseArgsOnly(POOLMEM *cmd, POOLMEM *&args, int *argc,
                    char **argk, char **argv, int max_args);
void SplitPathAndFilename(const char *fname, POOLMEM *&path,
                             int *pnl, POOLMEM *&file, int *fnl);
int bsscanf(const char *buf, const char *fmt, ...);

#endif // BAREOS_LIB_SCAN_H_
