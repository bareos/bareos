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
#ifndef FINDLIB_MATCH_H_
#define FINDLIB_MATCH_H_

DLL_IMP_EXP void init_include_exclude_files(FindFilesPacket *ff);
DLL_IMP_EXP void term_include_exclude_files(FindFilesPacket *ff);
DLL_IMP_EXP void add_fname_to_include_list(FindFilesPacket *ff, int prefixed, const char *fname);
DLL_IMP_EXP void add_fname_to_exclude_list(FindFilesPacket *ff, const char *fname);
DLL_IMP_EXP bool file_is_excluded(FindFilesPacket *ff, const char *file);
DLL_IMP_EXP bool file_is_included(FindFilesPacket *ff, const char *file);
DLL_IMP_EXP struct s_included_file *get_next_included_file(FindFilesPacket *ff,
                                               struct s_included_file *inc);
DLL_IMP_EXP bool parse_size_match(const char *size_match_pattern,
                      struct s_sz_matching *size_matching);

#endif // FINDLIB_MATCH_H_
