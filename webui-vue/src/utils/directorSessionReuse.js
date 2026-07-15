/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

export function canReuseDirectorCredentials({
  isAddDirectorMode = false,
  sourceDirector = '',
  targetDirector = '',
} = {}) {
  return !!isAddDirectorMode
    && !!sourceDirector
    && !!targetDirector
    && sourceDirector !== targetDirector
}

export function getDirectorReuseResult(reuseResponse, director) {
  return reuseResponse?.results?.find(item => item?.director === director) ?? null
}

export function isSuccessfulDirectorReuse(reuseResponse, director) {
  const status = getDirectorReuseResult(reuseResponse, director)?.status
  return ['authenticated', 'already_authenticated'].includes(status)
}

export function getDirectorReuseErrorMessage(reuseResult) {
  const status = reuseResult?.status
  const message = reuseResult?.message

  // Map known status values to user-friendly messages
  const statusMessages = {
    'authentication_failed': 'Authentication failed',
    'connection_error': `Cannot connect to director (${message || 'unknown error'})`,
    'invalid_credentials': 'Invalid credentials for this director',
    'director_not_found': 'Director not found',
    'unknown_error': `Credential reuse failed (${message || 'unknown error'})`,
  }

  // Return mapped message or include status in generic message
  return statusMessages[status] || `Credential reuse failed (status: ${status}, ${message || 'no details'})`
}
