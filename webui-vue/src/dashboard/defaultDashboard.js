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

export const ANALYTICS_DASHBOARD = {
  id: 'analytics',
  name: 'Analytics',
  widgets: [
    {
      id: 'w-analytics-summary',
      type: 'analytics-summary',
      title: 'Analytics Summary',
      props: {},
      layout: {
        x: 0,
        y: 0,
        w: 12,
        h: 5,
        i: 'w-analytics-summary',
        minW: 2,
        minH: 3,
      },
    },
    {
      id: 'w-analytics-treemap',
      type: 'analytics-treemap',
      title: 'Stored Data per Job',
      props: {},
      layout: {
        x: 0,
        y: 5,
        w: 8,
        h: 10,
        i: 'w-analytics-treemap',
        minW: 2,
        minH: 3,
      },
    },
    {
      id: 'w-analytics-status',
      type: 'analytics-status-breakdown',
      title: 'Job Status Breakdown',
      props: {},
      layout: {
        x: 0,
        y: 15,
        w: 8,
        h: 8,
        i: 'w-analytics-status',
        minW: 2,
        minH: 3,
      },
    },
    {
      id: 'w-analytics-client-bytes',
      type: 'analytics-client-bytes',
      title: 'Bytes per Client',
      props: {},
      layout: {
        x: 8,
        y: 5,
        w: 4,
        h: 10,
        i: 'w-analytics-client-bytes',
        minW: 2,
        minH: 3,
      },
    },
    {
      id: 'w-analytics-levels',
      type: 'analytics-level-distribution',
      title: 'Job Level Distribution',
      props: {},
      layout: {
        x: 8,
        y: 15,
        w: 4,
        h: 8,
        i: 'w-analytics-levels',
        minW: 2,
        minH: 3,
      },
    },
  ],
}

export const PRECONFIGURED_DASHBOARDS = [
  DEFAULT_DASHBOARD,
  ANALYTICS_DASHBOARD,
]
