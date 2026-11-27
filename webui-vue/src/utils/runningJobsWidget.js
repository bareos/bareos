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

export const QUEUED_GROUP_THRESHOLD = 50

export function resolveQueuedViewMode(
  queuedCount,
  override = null,
  threshold = QUEUED_GROUP_THRESHOLD
) {
  if (override === 'flat' || override === 'grouped') {
    return override
  }

  return queuedCount > threshold ? 'grouped' : 'flat'
}

export function extractCancelableGroupJobs(group) {
  return (group?.jobs ?? [])
    .map(job => ({ job, id: job?.id ?? job?.jobid ?? null }))
    .filter(({ id }) => id !== null && id !== '')
}
