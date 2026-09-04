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

import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { nextTick } from 'vue'
import { useDashboardStore } from '../../src/stores/dashboards.js'

describe('dashboards store', () => {
  beforeEach(() => {
    localStorage.clear()
    setActivePinia(createPinia())
  })

  // ── initial state ──────────────────────────────────────────────────────────

  it('seeds a default dashboard on first run', () => {
    const store = useDashboardStore()
    expect(store.dashboards).toHaveLength(2)
    expect(store.dashboards[0].name).toBe('Overview')
    expect(store.dashboards[0].widgets.length).toBeGreaterThan(0)
    expect(store.dashboards[1].name).toBe('Analytics')
    expect(store.dashboards[1].widgets).toEqual(expect.arrayContaining([
      expect.objectContaining({ type: 'analytics-summary' }),
      expect.objectContaining({ type: 'analytics-treemap' }),
      expect.objectContaining({ type: 'analytics-status-breakdown' }),
      expect.objectContaining({ type: 'analytics-client-bytes' }),
      expect.objectContaining({ type: 'analytics-level-distribution' }),
    ]))
  })

  it('sets the active dashboard to the first dashboard by default', () => {
    const store = useDashboardStore()
    expect(store.activeDashboardId).toBe(store.dashboards[0].id)
  })

  // ── persistence ────────────────────────────────────────────────────────────

  it('persists dashboards to localStorage after mutation', async () => {
    const store = useDashboardStore()
    store.addDashboard('My Board')

    await nextTick()

    const raw = JSON.parse(localStorage.getItem('bareos_dashboards'))
    expect(Array.isArray(raw.dashboards)).toBe(true)
    expect(raw.dashboards.some(d => d.name === 'My Board')).toBe(true)
    expect(typeof raw.activeDashboardId).toBe('string')
  })

  it('restores dashboards from localStorage on mount', () => {
    localStorage.setItem('bareos_dashboards', JSON.stringify({
      activeDashboardId: 'saved-1',
      dashboards: [
        {
          id: 'saved-1',
          name: 'Restored Board',
          widgets: [
            {
              id: 'w1',
              type: 'job-totals',
              title: 'Totals',
              props: {},
              layout: { x: 0, y: 0, w: 4, h: 5, i: 'w1', minW: 2, minH: 3 },
            },
          ],
        },
      ],
    }))

    setActivePinia(createPinia())
    const store = useDashboardStore()

    expect(store.dashboards).toHaveLength(3)
    expect(store.dashboards[0].id).toBe('saved-1')
    expect(store.dashboards[0].name).toBe('Restored Board')
    expect(store.dashboards[0].widgets).toHaveLength(1)
    expect(store.dashboards[0].widgets[0].type).toBe('job-totals')
    expect(store.dashboards).toEqual(expect.arrayContaining([
      expect.objectContaining({ name: 'Overview' }),
      expect.objectContaining({ name: 'Analytics' }),
    ]))
  })

  it('falls back to the default dashboard when localStorage is corrupt', () => {
    localStorage.setItem('bareos_dashboards', 'not-valid-json{{')
    setActivePinia(createPinia())
    const store = useDashboardStore()
    expect(store.dashboards).toEqual(expect.arrayContaining([
      expect.objectContaining({ name: 'Overview' }),
      expect.objectContaining({ name: 'Analytics' }),
    ]))
  })

  it('falls back to default when localStorage contains an empty array', () => {
    localStorage.setItem('bareos_dashboards', JSON.stringify({ dashboards: [], activeDashboardId: null }))
    setActivePinia(createPinia())
    const store = useDashboardStore()
    expect(store.dashboards[0].name).toBe('Overview')
  })

  it('discards widgets with missing id or type when restoring', () => {
    localStorage.setItem('bareos_dashboards', JSON.stringify({
      activeDashboardId: 'db1',
      dashboards: [
        {
          id: 'db1',
          name: 'Board',
          widgets: [
            { id: '', type: 'job-totals', title: '', props: {}, layout: {} },
            { id: 'w1', type: '', title: '', props: {}, layout: {} },
            { id: 'w2', type: 'job-totals', title: 'OK', props: {}, layout: { x: 0, y: 0, w: 4, h: 5 } },
          ],
        },
      ],
    }))
    setActivePinia(createPinia())
    const store = useDashboardStore()
    expect(store.dashboards[0].widgets).toHaveLength(1)
    expect(store.dashboards[0].widgets[0].id).toBe('w2')
  })

  // ── dashboard CRUD ─────────────────────────────────────────────────────────

  it('adds a new dashboard and makes it active', () => {
    const store = useDashboardStore()
    const id = store.addDashboard('Ops Board')

    expect(store.dashboards).toHaveLength(3)
    expect(store.dashboards[2].name).toBe('Ops Board')
    expect(store.activeDashboardId).toBe(id)
  })

  it('renames a dashboard', () => {
    const store = useDashboardStore()
    const id = store.dashboards[0].id
    store.renameDashboard(id, 'Renamed')
    expect(store.dashboards[0].name).toBe('Renamed')
  })

  it('removes a dashboard and switches active to the first remaining one', () => {
    const store = useDashboardStore()
    const firstId = store.dashboards[0].id
    const secondId = store.addDashboard('Second')
    store.setActiveDashboard(secondId)

    store.removeDashboard(secondId)

    expect(store.dashboards).toHaveLength(2)
    expect(store.activeDashboardId).toBe(firstId)
  })

  it('does not remove the last remaining dashboard', () => {
    const store = useDashboardStore()
    const id = store.dashboards[0].id
    store.removeDashboard(id)
    expect(store.dashboards).toHaveLength(1)
  })

  it('activeDashboard() returns the dashboard matching activeDashboardId', () => {
    const store = useDashboardStore()
    const id = store.addDashboard('Second')
    store.setActiveDashboard(id)
    expect(store.activeDashboard().name).toBe('Second')
  })

  // ── widget CRUD ────────────────────────────────────────────────────────────

  it('adds a widget to a dashboard', () => {
    const store = useDashboardStore()
    const dbId = store.dashboards[0].id
    const widgetId = store.addWidget(dbId, {
      type: 'job-totals',
      title: 'My Totals',
      props: { foo: 'bar' },
      layout: { x: 0, y: 0, w: 4, h: 5 },
    })

    const widget = store.dashboards[0].widgets.find(w => w.id === widgetId)
    expect(widget).toBeDefined()
    expect(widget.type).toBe('job-totals')
    expect(widget.title).toBe('My Totals')
    expect(widget.props).toEqual({ foo: 'bar' })
    expect(widget.layout.w).toBe(4)
    expect(widget.layout.h).toBe(5)
  })

  it('updates a widget title', () => {
    const store = useDashboardStore()
    const dbId = store.dashboards[0].id
    const wId = store.addWidget(dbId, {
      type: 'job-totals',
      title: 'Old',
      props: {},
      layout: { x: 0, y: 0, w: 4, h: 5 },
    })
    store.updateWidgetTitle(dbId, wId, 'New Title')
    const widget = store.dashboards[0].widgets.find(w => w.id === wId)
    expect(widget.title).toBe('New Title')
  })

  it('updates widget props', () => {
    const store = useDashboardStore()
    const dbId = store.dashboards[0].id
    const wId = store.addWidget(dbId, {
      type: 'job-totals',
      title: '',
      props: { a: 1 },
      layout: { x: 0, y: 0, w: 4, h: 5 },
    })
    store.updateWidgetProps(dbId, wId, { b: 2 })
    const widget = store.dashboards[0].widgets.find(w => w.id === wId)
    expect(widget.props).toEqual({ b: 2 })
  })

  it('removes a widget from a dashboard', () => {
    const store = useDashboardStore()
    const dbId = store.dashboards[0].id
    const wId = store.addWidget(dbId, {
      type: 'running-jobs',
      title: 'Running',
      props: {},
      layout: { x: 0, y: 0, w: 4, h: 6 },
    })
    store.removeWidget(dbId, wId)
    expect(store.dashboards[0].widgets.find(w => w.id === wId)).toBeUndefined()
  })

  it('updates widget layouts in bulk', () => {
    const store = useDashboardStore()
    const dbId = store.dashboards[0].id
    const wId = store.addWidget(dbId, {
      type: 'job-totals',
      title: '',
      props: {},
      layout: { x: 0, y: 0, w: 4, h: 5 },
    })
    store.updateWidgetLayouts(dbId, [{ i: wId, x: 3, y: 2, w: 6, h: 8 }])
    const widget = store.dashboards[0].widgets.find(w => w.id === wId)
    expect(widget.layout.x).toBe(3)
    expect(widget.layout.y).toBe(2)
    expect(widget.layout.w).toBe(6)
    expect(widget.layout.h).toBe(8)
  })

  // ── layout normalisation ───────────────────────────────────────────────────

  it('enforces minimum widget width and height', () => {
    const store = useDashboardStore()
    const dbId = store.dashboards[0].id
    const wId = store.addWidget(dbId, {
      type: 'job-totals',
      title: '',
      props: {},
      layout: { x: 0, y: 0, w: 0, h: 1 },  // below minimums
    })
    const widget = store.dashboards[0].widgets.find(w => w.id === wId)
    expect(widget.layout.w).toBeGreaterThanOrEqual(2)
    expect(widget.layout.h).toBeGreaterThanOrEqual(3)
  })
})
