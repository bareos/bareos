/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_SESSION_TOKEN_REGISTRY_H_
#define BAREOS_STORED_SESSION_TOKEN_REGISTRY_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "lib/thread_util.h"

namespace storagedaemon {

/* Decide whether a token may be used to identify a session.
 *
 * The storage daemon wipes the authentication key of a job once the key was
 * used, which turns it into an empty string while the job is still known.
 * Treating such a wiped key as a valid token would let anyone bind to that
 * job by presenting an empty key, so empty tokens are never usable. */
constexpr bool IsUsableSessionToken(std::string_view token)
{
  return !token.empty();
}

/* Maps session tokens to the objects they belong to.
 *
 * Registering a token explicitly avoids searching all jobs for a matching
 * authentication key: only tokens that were handed out on purpose can be
 * looked up, and the lookup does not depend on the number of running jobs. */
template <typename T> class SessionTokenRegistry {
 public:
  /* Register value under token.
   *
   * Returns false if the token is unusable or already registered, so a token
   * can never silently take over the entry of another session. */
  bool Add(std::string_view token, T value)
  {
    if (!IsUsableSessionToken(token)) { return false; }

    auto entries = entries_.lock();

    return entries->emplace(std::string{token}, std::move(value)).second;
  }

  // Remove token. Returns whether it was registered.
  bool Remove(std::string_view token)
  {
    if (!IsUsableSessionToken(token)) { return false; }

    auto entries = entries_.lock();

    return entries->erase(std::string{token}) > 0;
  }

  /* Look up token and call visitor with the registered value.
   *
   * The visitor runs while the registry is locked, so the entry cannot be
   * removed concurrently and the visitor can safely take ownership, for
   * example by incrementing a use count. Returns whether the token was
   * found. */
  template <typename Visitor> bool Visit(std::string_view token, Visitor&& fn)
  {
    if (!IsUsableSessionToken(token)) { return false; }

    auto entries = entries_.lock();

    auto entry = entries->find(std::string{token});
    if (entry == entries->end()) { return false; }

    fn(entry->second);

    return true;
  }

  std::size_t size() const { return entries_.lock()->size(); }

 private:
  synchronized<std::unordered_map<std::string, T>> entries_;
};

}  // namespace storagedaemon

#endif  // BAREOS_STORED_SESSION_TOKEN_REGISTRY_H_
