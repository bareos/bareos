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

/**
 * Seed layout used when no dashboard configuration exists yet.
 * Reproduces the pre-refactor dashboard layout.
 */

export const DEFAULT_DASHBOARD = {
  id: 'default',
  name: 'Overview',
  widgets: [
    {
      id: 'w-jobs-24h',
      type: 'jobs-past-24h',
      title: 'Jobs Past 24 h',
      props: {},
      layout: { x: 0, y: 0, w: 8, h: 5, i: 'w-jobs-24h', minW: 2, minH: 3 },
    },
    {
      id: 'w-job-totals',
      type: 'job-totals',
      title: 'Job Totals',
      props: {},
      layout: { x: 8, y: 0, w: 4, h: 5, i: 'w-job-totals', minW: 2, minH: 3 },
    },
    {
      id: 'w-recent-jobs',
      type: 'recent-jobs-table',
      title: 'Recent Jobs',
      props: {},
      layout: { x: 0, y: 5, w: 8, h: 10, i: 'w-recent-jobs', minW: 2, minH: 3 },
    },
    {
      id: 'w-running-jobs',
      type: 'running-jobs',
      title: 'Running Jobs',
      props: {},
      layout: { x: 8, y: 5, w: 4, h: 10, i: 'w-running-jobs', minW: 2, minH: 3 },
    },
  ],
}
