/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_EDIT_H_
#define BAREOS_LIB_EDIT_H_

#include <vector>
#include <string>
#include <string_view>
#include "include/bc_types.h"
#include "lib/mem_pool.h"

namespace edit {
/* The biggest 64 bit number -- 2^64-1 -- has 20 digits.
 * As such one needs 27 = 20 + 6 + 1 characters to safely format 64 bit numbers
 * with commata every 3 digits and a null terminator.
 * 64 bit signed integers have at most 19 digits so this also works for them. */
inline constexpr int32_t min_buffer_size{27};
};  // namespace edit

uint64_t str_to_uint64(const char* str);
#define str_to_uint16(str) ((uint16_t)str_to_uint64(str))
#define str_to_uint32(str) ((uint32_t)str_to_uint64(str))
int64_t str_to_int64(const char* str);
#define str_to_int16(str) ((int16_t)str_to_int64(str))
#define str_to_int32(str) ((int32_t)str_to_int64(str))

/* the buffer passed to the following edit_* functions
 * should be able to hold at least min_buffer_size characters */

char* edit_uint64_with_commas(uint64_t val, char* buf);
char* edit_uint64_with_suffix(uint64_t val, char* buf);
char* edit_uint64(uint64_t val, char* buf);
char* edit_int64(int64_t val, char* buf);
char* edit_int64_with_commas(int64_t val, char* buf);

char* add_commas(const char* val, char* buf);
bool DurationToUtime(const char* str, utime_t* value);
bool size_to_uint64(const char* str, uint64_t* value);
bool speed_to_uint64(const char* str, uint64_t* value);
char* edit_utime(utime_t val, char* buf, int buf_len);
char* edit_pthread(pthread_t val, char* buf, int buf_len);
bool Is_a_number(const char* num);
bool Is_a_number_list(const char* n);
bool IsAnInteger(std::string_view view);
bool IsNameValid(const char* name, std::string& msg);
bool IsNameValid(const char* name);
bool IsAclEntryValid(const char* acl, PoolMem& msg);
bool IsAclEntryValid(const char* acl);
std::string SizeAsSiPrefixFormat(uint64_t value_in);

#endif  // BAREOS_LIB_EDIT_H_
