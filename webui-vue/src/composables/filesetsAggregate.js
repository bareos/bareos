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

import { directorCollection } from './useDirectorFetch.js'
import { createDirectorCommandClient } from './directorAggregate.js'

function decorateFilesets(entries, director) {
  return directorCollection(entries).map(entry => ({
    name: entry.fileset ?? entry.name ?? '',
    description: entry.description ?? '',
    createtime: entry.createtime ?? '',
    md5: entry.md5 ?? '',
    include: entry.include ?? [],
    exclude: entry.exclude ?? [],
    options: entry.options ?? '',
    director,
    scopeKey: `${director}:${entry.fileset ?? entry.name ?? ''}`,
  }))
}

function sortFilesets(filesets) {
  return [...filesets].sort((left, right) => {
    const nameCompare = String(left.name ?? '').localeCompare(String(right.name ?? ''))
    if (nameCompare !== 0) {
      return nameCompare
    }

    return String(left.director ?? '').localeCompare(String(right.director ?? ''))
  })
}

export async function fetchAggregatedFilesets(credentials, directors) {
  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      const result = await client.call('list filesets')
      return {
        director,
        filesets: decorateFilesets(result?.filesets, director),
      }
    } finally {
      client.disconnect()
    }
  }))

  return {
    filesets: sortFilesets(results
      .filter(result => result.status === 'fulfilled')
      .flatMap(result => result.value.filesets)),
    directorErrors: results.flatMap((result, index) => (
      result.status === 'rejected'
        ? [{
          director: directors[index],
          message: result.reason?.message ?? 'Failed to load filesets.',
        }]
        : []
    )),
  }
}
