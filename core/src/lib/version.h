/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2013 Free Software Foundation Europe e.V.
   Copyright (C) 2010-2017 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_VERSION_H_
#define BAREOS_LIB_VERSION_H_

#include <charconv>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <stddef.h>
#include <stdio.h>
#include <tuple>
#include "include/dll_import_export.h"

struct BareosVersionStrings {
  const char* Full;
  const char* Date;
  const char* ShortDate;
  const char* ProgDateTime;
  const char* FullWithDate;
  const char* Year;
  const char* BinaryInfo;
  const char* ServicesMessage;
  const char* JoblogMessage;
  void (*FormatCopyrightWithFsfAndPlanets)(char* out, size_t len, int FsfYear);
  void (*PrintCopyrightWithFsfAndPlanets)(FILE* fh, int FsfYear);
  void (*FormatCopyright)(char* out, size_t len, int StartYear);
  void (*PrintCopyright)(FILE* fh, int StartYear);
  const char* (*GetOsInfo)(void);
};
BAREOS_IMPORT const struct BareosVersionStrings kBareosVersionStrings;

struct BareosVersion {
  std::uint8_t Major{};
  std::uint8_t Minor{};
  std::uint8_t Patch{};
  std::string prerelease_version{};

  auto operator<=>(const BareosVersion&) const = default;

  static std::optional<BareosVersion> try_parse(std::string_view full_version)
  {
    // the format of the full version is
    // $Major.$Minor.$Patch[~pre$PreRelease][.dirty]

    auto split = [](std::string_view v, char c, bool* found = nullptr)
        -> std::pair<std::string_view, std::string_view> {
      auto pos = v.find(c);

      if (found) { *found = pos != v.npos; }

      if (pos == v.npos) { return {v, std::string_view{}}; }

      return {v.substr(0, pos), v.substr(pos + 1)};
    };

    std::string_view rest = full_version;
    std::string_view major, minor, patch, prerelease;

    constexpr std::string_view dirty_suffix = ".dirty";
    if (rest.ends_with(dirty_suffix)) {
      // we do not care about dirty/not dirty
      rest.remove_suffix(dirty_suffix.size());
    }

    std::tie(major, rest) = split(rest, '.');
    std::tie(minor, rest) = split(rest, '.');

    bool prerelease_expected = false;
    std::tie(patch, prerelease) = split(rest, '~', &prerelease_expected);

    if (major.empty() || minor.empty() || patch.empty()) {
      // these parts are required
      return std::nullopt;
    }

    if (prerelease_expected) {
      if (!prerelease.starts_with("pre")) { return std::nullopt; }
      prerelease.remove_prefix(3);
      if (prerelease.empty()) { return std::nullopt; }
    }

    auto full_parse = [](std::string_view v, std::uint8_t& value) -> bool {
      auto res = std::from_chars(v.data(), v.data() + v.size(), value);

      if (res.ec != std::errc{} || res.ptr != v.data() + v.size()) {
        return false;
      }
      return true;
    };

    BareosVersion result;
    result.prerelease_version = std::string{prerelease};

    if (!full_parse(major, result.Major) || !full_parse(minor, result.Minor)
        || !full_parse(patch, result.Patch)) {
      return std::nullopt;
    }

    return std::optional{std::move(result)};
  }

  static BareosVersion parse(std::string_view full_version)
  {
    std::optional version = try_parse(full_version);
    if (!version) { throw std::runtime_error{"Bad full version"}; }

    return std::move(*version);
  }
};

BAREOS_IMPORT const BareosVersion kBareosVersion;

/* Debug flags not normally turned on */

/* #define TRACE_JCR_CHAIN 1 */
/* #define TRACE_RES 1 */
/* #define DEBUG_BLOCK_CHECKSUM 1 */

/*
 * Set SMALLOC_SANITY_CHECK to zero to turn off, otherwise
 *  it is the maximum memory malloced before Bareos will
 *  abort.  Except for debug situations, this should be zero
 */
#define SMALLOC_SANITY_CHECK 0 /* 500000000  0.5 GB max */


/* Check if header of tape block is zero before writing */
/* #define DEBUG_BLOCK_ZEROING 1 */

/* The following are turned on for performance testing */
/*
 * If you turn on the NO_ATTRIBUTES_TEST and rebuild, the SD
 *  will receive the attributes from the FD, will write them
 *  to disk, then when the data is written to tape, it will
 *  read back the attributes, but they will not be sent to
 *  the Director. So this will eliminate: 1. the comm time
 *  to send the attributes to the Director. 2. the time it
 *  takes the Director to put them in the catalog database.
 */
/* #define NO_ATTRIBUTES_TEST 1 */

/*
 * If you turn on NO_TAPE_WRITE_TEST and rebuild, the SD
 *  will do all normal actions, but will not write to the
 *  Volume.  Note, this means a lot of functions such as
 *  labeling will not work, so you must use it only when
 *  Bareos is going to append to a Volume. This will eliminate
 *  the time it takes to write to the Volume (not the time
 *  it takes to do any positioning).
 */
/* #define NO_TAPE_WRITE_TEST 1 */

/*
 * If you turn on FD_NO_SEND_TEST and rebuild, the FD will
 *  not send any attributes or data to the SD. This will
 *  eliminate the comm time sending to the SD.
 */
/* #define FD_NO_SEND_TEST 1 */
#endif  // BAREOS_LIB_VERSION_H_
