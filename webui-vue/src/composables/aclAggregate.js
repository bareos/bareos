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

import { createDirectorCommandClient } from './directorAggregate.js'
import { normaliseDirectorCommandPermissions } from './useDirectorFetch.js'

function sortPermissions(commands) {
  return [...commands].sort((left, right) => {
    const commandCompare = String(left.command ?? '').localeCompare(String(right.command ?? ''))
    if (commandCompare !== 0) {
      return commandCompare
    }

    return String(left.director ?? '').localeCompare(String(right.director ?? ''))
  })
}

export async function fetchAggregatedAcl(credentials, directors) {
  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      const commands = await client.call('.help full=yes')
      return normaliseDirectorCommandPermissions(commands).map(item => ({
        ...item,
        director,
        scopeKey: `${director}:${item.command}`,
      }))
    } finally {
      client.disconnect()
    }
  }))

  return {
    commands: sortPermissions(results
      .filter(result => result.status === 'fulfilled')
      .flatMap(result => result.value)),
    directorErrors: results.flatMap((result, index) => (
      result.status === 'rejected'
        ? [{
          director: directors[index],
          message: result.reason?.message ?? 'Failed to load command permissions.',
        }]
        : []
    )),
  }
}
