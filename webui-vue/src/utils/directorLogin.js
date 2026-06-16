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

export function directorListLoadErrorMessage(error, t) {
  return t('Director list request failed: {message}', {
    message: error?.message || t('Unknown error'),
  })
}

export function shouldAutoLoginAllDirectors({
  isAddDirectorMode = false,
  requestedDirector = '',
  availableDirectors = [],
} = {}) {
  const director = String(requestedDirector ?? '').trim()
  const directors = [...new Set(
    (Array.isArray(availableDirectors) ? availableDirectors : [])
      .map(value => String(value ?? '').trim())
      .filter(Boolean)
  )]

  return !isAddDirectorMode && !director && directors.length > 1
}

export function summarizeDirectorLoginAttempts(attempts = []) {
  const successfulDirectors = []
  const failedAttempts = []

  for (const attempt of Array.isArray(attempts) ? attempts : []) {
    const director = String(attempt?.director ?? '').trim()
    if (!director) {
      continue
    }

    if (attempt?.success) {
      successfulDirectors.push(director)
      continue
    }

    failedAttempts.push({
      director,
      message: String(attempt?.message ?? '').trim(),
    })
  }

  return {
    successfulDirectors,
    failedAttempts,
    failedDirectors: failedAttempts.map(attempt => attempt.director),
  }
}

export function getLastSuccessfulDirector(attempts = []) {
  let lastSuccessfulDirector = ''

  for (const attempt of Array.isArray(attempts) ? attempts : []) {
    if (attempt?.success) {
      const director = String(attempt?.director ?? '').trim()
      if (director) {
        lastSuccessfulDirector = director
      }
    }
  }

  return lastSuccessfulDirector
}
