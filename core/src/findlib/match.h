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
#ifndef BAREOS_FINDLIB_MATCH_H_
#define BAREOS_FINDLIB_MATCH_H_

void InitIncludeExcludeFiles(FindFilesPacket *ff);
void TermIncludeExcludeFiles(FindFilesPacket *ff);
void AddFnameToIncludeList(FindFilesPacket *ff, int prefixed, const char *fname);
void AddFnameToExcludeList(FindFilesPacket *ff, const char *fname);
bool FileIsExcluded(FindFilesPacket *ff, const char *file);
bool FileIsIncluded(FindFilesPacket *ff, const char *file);
struct s_included_file *get_next_included_file(FindFilesPacket *ff,
                                               struct s_included_file *inc);
bool ParseSizeMatch(const char *size_match_pattern,
                      struct s_sz_matching *size_matching);

#endif // BAREOS_FINDLIB_MATCH_H_
