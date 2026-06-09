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

export function toUserVisibleDirectorError(
  message,
  {
    authenticationMessage = 'Authentication failed',
    connectionMessage = 'Could not connect to director.',
  } = {},
) {
  const normalized = String(message ?? '').trim()
  if (!normalized) {
    return connectionMessage
  }

  const lower = normalized.toLowerCase()
  if (
    lower.includes('authentication failed')
    || lower.includes('authorization failed')
    || lower.includes('tls-psk handshake failed')
    || lower.includes('proxy session expired')
    || lower.includes('proxy session missing')
    || lower.includes('please log in again')
  ) {
    return authenticationMessage
  }

  return connectionMessage
}
