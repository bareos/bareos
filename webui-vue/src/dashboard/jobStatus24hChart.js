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

export const JOB_STATUS_24H_CHART_SEGMENTS = [
  { status: 'R', labelKey: 'Running', color: '#17a2b8' },
  { status: 'C', labelKey: 'Waiting', color: '#9e9e9e' },
  { status: 'T', labelKey: 'Successful', color: '#28a745' },
  { status: 'W', labelKey: 'Warning', color: '#ffc107' },
  { status: 'f', labelKey: 'Failed', color: '#dc3545' },
]

function normaliseCount(value) {
  const count = Number(value ?? 0)
  return Number.isFinite(count) ? count : 0
}

export function buildJobStatus24hChartSegments(statusCounts = {}) {
  return JOB_STATUS_24H_CHART_SEGMENTS
    .map(segment => ({
      ...segment,
      count: normaliseCount(statusCounts?.[segment.status]),
    }))
    .filter(segment => segment.count > 0)
}
