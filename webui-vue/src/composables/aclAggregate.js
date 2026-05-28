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

import { normaliseDirectorCommandPermissions } from './useDirectorFetch.js'
import {
  directorAggregateErrors,
  fulfilledDirectorValues,
  runDirectorAggregates,
} from './directorAggregateRunner.js'

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
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    const commands = await client.call('.help full=yes')
    return normaliseDirectorCommandPermissions(commands).map(item => ({
      ...item,
      director,
      scopeKey: `${director}:${item.command}`,
    }))
  })

  return {
    commands: sortPermissions(fulfilledDirectorValues(results).flatMap(value => value)),
    directorErrors: directorAggregateErrors(
      results,
      directors,
      'Failed to load command permissions.'
    ),
  }
}
