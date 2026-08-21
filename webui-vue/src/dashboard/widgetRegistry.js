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
 * Central widget registry.
 *
 * Each entry describes a widget type:
 *   type        – unique string key used in persisted config
 *   label       – human-readable name shown in the picker dialog
 *   description – short description shown in the picker dialog
 *   icon        – Quasar/MDI icon name
 *   component   – async import of the Vue component
 *   defaultTitle – default title for new instances
 *   defaultLayout – { w, h } default grid size
 *   requiredProps – array of prop descriptors that must be filled before
 *                   the widget is placed on the dashboard:
 *     { key, label, type: 'text'|'select', options?: fn(credentials,directors) }
 *   optionalProps – same shape, shown only in the configure dialog
 */

import { defineAsyncComponent } from 'vue'

const WIDGETS = [
  {
    type: 'jobs-past-24h',
    label: 'Jobs Past 24 h',
    description: 'Summary counts of jobs started in the last 24 hours by status.',
    icon: 'mdi-clock-outline',
    component: defineAsyncComponent(() => import('./widgets/JobsPast24hWidget.vue')),
    defaultTitle: 'Jobs Past 24 h',
    defaultLayout: { w: 8, h: 5 },
    requiredProps: [],
    optionalProps: [],
  },
  {
    type: 'recent-jobs-table',
    label: 'Recent Jobs',
    description: 'Table showing the most recent job run per job name.',
    icon: 'mdi-table',
    component: defineAsyncComponent(() => import('./widgets/RecentJobsTableWidget.vue')),
    defaultTitle: 'Recent Jobs',
    defaultLayout: { w: 8, h: 10 },
    requiredProps: [],
    optionalProps: [],
  },
  {
    type: 'running-jobs',
    label: 'Running Jobs',
    description: 'Live list of currently running jobs with progress.',
    icon: 'mdi-play-circle-outline',
    component: defineAsyncComponent(() => import('./widgets/RunningJobsWidget.vue')),
    defaultTitle: 'Running Jobs',
    defaultLayout: { w: 4, h: 10 },
    requiredProps: [],
    optionalProps: [],
  },
  {
    type: 'job-totals',
    label: 'Job Totals',
    description: 'Cumulative job, file, and byte totals across all directors.',
    icon: 'mdi-sigma',
    component: defineAsyncComponent(() => import('./widgets/JobTotalsWidget.vue')),
    defaultTitle: 'Job Totals',
    defaultLayout: { w: 4, h: 4 },
    requiredProps: [],
    optionalProps: [],
  },
  {
    type: 'pool-bytes-chart',
    label: 'Pool Storage (Bytes)',
    description: 'Pie chart showing bytes stored across pools.',
    icon: 'mdi-chart-pie',
    component: defineAsyncComponent(() => import('./widgets/PoolBytesChartWidget.vue')),
    defaultTitle: 'Pool Storage (Bytes)',
    defaultLayout: { w: 4, h: 8 },
    requiredProps: [],
    optionalProps: [],
  },
  {
    type: 'pool-volumes-chart',
    label: 'Pool Storage (Volumes)',
    description: 'Pie chart showing volume count across pools.',
    icon: 'mdi-chart-pie',
    component: defineAsyncComponent(() => import('./widgets/PoolVolumesChartWidget.vue')),
    defaultTitle: 'Pool Storage (Volumes)',
    defaultLayout: { w: 4, h: 8 },
    requiredProps: [],
    optionalProps: [],
  },
]

/** Map from type key → definition (for O(1) lookup). */
const WIDGET_MAP = Object.fromEntries(WIDGETS.map(w => [w.type, w]))

export function getAllWidgetDefinitions() {
  return WIDGETS
}

export function getWidgetDefinition(type) {
  return WIDGET_MAP[type] ?? null
}
