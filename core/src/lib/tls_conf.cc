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

#include "include/bareos.h"
#include "lib/tls_conf.h"

namespace {
bool bad_policy(TlsPolicy pol)
{
  switch (pol) {
    case kBnetTlsNone:
      [[fallthrough]];
    case kBnetTlsEnabled:
      [[fallthrough]];
    case kBnetTlsRequired:
      [[fallthrough]];
    case kBnetTlsAuto: {
      return false;
    } break;
    case kBnetTlsUnknown:
      [[fallthrough]];
    default: {
      return true;
    } break;
  }
}

bool requires_tls(TlsPolicy pol)
{
  switch (pol) {
    case kBnetTlsRequired: {
      return true;
    } break;
    default: {
      return false;
    } break;
  }
}

bool allows_tls(TlsPolicy pol)
{
  switch (pol) {
    case kBnetTlsEnabled:
      [[fallthrough]];
    case kBnetTlsRequired:
      [[fallthrough]];
    case kBnetTlsAuto: {
      return true;
    } break;
    default: {
      return false;
    } break;
  }
}
}  // namespace

bool TlsResource::IsTlsConfigured() const { return tls_enable_; }

TlsPolicy TlsResource::GetPolicy() const
{
  if (!tls_enable_) { return TlsPolicy::kBnetTlsNone; }
  if (!tls_require_) { return TlsPolicy::kBnetTlsEnabled; }
  return TlsPolicy::kBnetTlsRequired;
}

TlsStatus select_tls_status(TlsPolicy left, TlsPolicy right)
{
  if (bad_policy(left) || bad_policy(right)) { return TlsStatus::Error; }

  bool left_require = requires_tls(left);
  bool left_allow = allows_tls(left);

  bool right_require = requires_tls(right);
  bool right_allow = allows_tls(right);

  if (left_require || right_require) {
    if (!right_allow || !left_allow) {
      // if one side requires it, but the other does not allow it,
      // then we cannot continue
      return TlsStatus::Error;
    }

    // since one side requires it, and the other side is fine with it,
    // we need to create the tls connection
    return TlsStatus::Enabled;
  }

  if (!left_allow || !right_allow) {
    // as neither side requires it, but one of them does not allow it
    // we can only unify them, by disabling Tls

    return TlsStatus::Disabled;
  }

  // at this point we know:
  // - neither side requires tls
  // - both sides allow tls
  // as such both Enabled/Disabled is ok.  By default we choose to use tls
  return TlsStatus::Enabled;
}
