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
#ifndef BAREOS_FINDLIB_HARDLINK_H_
#define BAREOS_FINDLIB_HARDLINK_H_

#include <unordered_map>
#include <vector>
#include <string>
#include <type_traits>

/**
 * Structure for keeping track of hard linked files, we
 * keep an entry for each hardlinked file that we save,
 * which is the first one found. For all the other files that
 * are linked to this one, we save only the directory
 * entry so we can link it.
 */
struct CurLink {
  uint32_t FileIndex{0};      /**< Bareos FileIndex of this file */
  int32_t digest_stream{0};   /**< Digest type if needed */
  std::vector<char> digest{}; /**< Checksum of the file if needed */
  std::string name;           /**< The name */

  CurLink(const char* fname) : name{fname} {}
  void set_digest(int32_t digest_stream, const char* digest, uint32_t len);
};

struct Hardlink {
  // work around for windows; mingw defines ino_t to be 16bit
  // but the compat layer needs 64bits instead, so
  // on windows our struct stat does not use ino_t.
  using inode_type = decltype(stat::st_ino);
  using device_type = decltype(stat::st_dev);

  device_type dev; /**< Device */
  inode_type ino;  /**< Inode with device is unique */
};

inline bool operator==(const Hardlink& l, const Hardlink& r)
{
  return l.dev == r.dev && l.ino == r.ino;
}

template <> struct std::hash<Hardlink> {
  std::size_t operator()(const Hardlink& link) const
  {
    auto hash1 = std::hash<Hardlink::device_type>{}(link.dev);
    auto hash2 = std::hash<Hardlink::inode_type>{}(link.ino);

    // change this when N3876 (std::hash_combine) or something similar
    // is finally implemented.
    return hash1 + 67 * hash2;
  }
};

using LinkHash = std::unordered_map<Hardlink, CurLink>;

#endif  // BAREOS_FINDLIB_HARDLINK_H_
