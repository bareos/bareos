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
 * Pinia store for configurable multi-dashboard state.
 *
 * Persisted to localStorage under LS_KEY.
 * Future enhancement: replace localStorage serialisation with a server-side
 * API call so layout is shared across browsers/devices.
 *
 * Each dashboard has:
 *   { id, name, widgets[] }
 *
 * Each widget instance has:
 *   { id, type, title, props, layout: { x, y, w, h } }
 */

import { defineStore } from 'pinia'
import { ref, watch } from 'vue'
import { DEFAULT_DASHBOARD } from '../dashboard/defaultDashboard.js'

const LS_KEY = 'bareos_dashboards'

/** Minimum safe grid dimensions for any widget. */
const MIN_W = 2
const MIN_H = 3

function generateId() {
  return `${Date.now().toString(36)}-${Math.random().toString(36).slice(2, 7)}`
}

function normaliseDashboard(raw) {
  if (!raw || typeof raw !== 'object') return null
  const id = String(raw.id ?? '').trim()
  const name = String(raw.name ?? '').trim()
  if (!id || !name) return null

  const widgets = Array.isArray(raw.widgets)
    ? raw.widgets.flatMap(w => normaliseWidget(w) ? [normaliseWidget(w)] : [])
    : []

  return { id, name, widgets }
}

function normaliseWidget(raw) {
  if (!raw || typeof raw !== 'object') return null
  const id = String(raw.id ?? '').trim()
  const type = String(raw.type ?? '').trim()
  if (!id || !type) return null

  return {
    id,
    type,
    title: String(raw.title ?? '').trim(),
    props: raw.props && typeof raw.props === 'object' ? { ...raw.props } : {},
    layout: normaliseLayout(raw.layout, id),
  }
}

function normaliseLayout(raw, i) {
  return {
    x: Number.isInteger(Number(raw?.x)) ? Number(raw.x) : 0,
    y: Number.isInteger(Number(raw?.y)) ? Number(raw.y) : 0,
    w: Math.max(MIN_W, Number.isInteger(Number(raw?.w)) ? Number(raw.w) : 4),
    h: Math.max(MIN_H, Number.isInteger(Number(raw?.h)) ? Number(raw.h) : 5),
    i: String(i),
    minW: MIN_W,
    minH: MIN_H,
  }
}

function loadFromStorage() {
  try {
    const raw = localStorage.getItem(LS_KEY)
    if (raw) {
      const parsed = JSON.parse(raw)
      if (Array.isArray(parsed) && parsed.length > 0) {
        const dashboards = parsed.flatMap(d => {
          const nd = normaliseDashboard(d)
          return nd ? [nd] : []
        })
        if (dashboards.length > 0) {
          return { dashboards, activeDashboardId: dashboards[0].id }
        }
      }
    }
  } catch { /* ignore */ }

  // First run: seed with default dashboard.
  return {
    dashboards: [{ ...DEFAULT_DASHBOARD, id: generateId() }],
    activeDashboardId: null,
  }
}

export const useDashboardStore = defineStore('dashboards', () => {
  const loaded = loadFromStorage()
  const dashboards = ref(loaded.dashboards)
  const activeDashboardId = ref(
    loaded.activeDashboardId ?? dashboards.value[0]?.id ?? null
  )

  function save() {
    localStorage.setItem(LS_KEY, JSON.stringify(dashboards.value))
  }

  watch(dashboards, save, { deep: true })

  // ── active dashboard ─────────────────────────────────────────────────────

  function activeDashboard() {
    return dashboards.value.find(d => d.id === activeDashboardId.value) ?? dashboards.value[0]
  }

  function setActiveDashboard(id) {
    activeDashboardId.value = id
  }

  // ── dashboard CRUD ───────────────────────────────────────────────────────

  function addDashboard(name) {
    const id = generateId()
    dashboards.value = [...dashboards.value, { id, name: String(name).trim(), widgets: [] }]
    activeDashboardId.value = id
    return id
  }

  function renameDashboard(id, name) {
    dashboards.value = dashboards.value.map(d =>
      d.id === id ? { ...d, name: String(name).trim() } : d
    )
  }

  function removeDashboard(id) {
    if (dashboards.value.length <= 1) return
    dashboards.value = dashboards.value.filter(d => d.id !== id)
    if (activeDashboardId.value === id) {
      activeDashboardId.value = dashboards.value[0]?.id ?? null
    }
  }

  // ── widget CRUD ──────────────────────────────────────────────────────────

  function addWidget(dashboardId, { type, title, props, layout }) {
    const id = generateId()
    const widget = {
      id,
      type,
      title: String(title ?? '').trim(),
      props: props && typeof props === 'object' ? { ...props } : {},
      layout: normaliseLayout(layout, id),
    }
    dashboards.value = dashboards.value.map(d =>
      d.id === dashboardId
        ? { ...d, widgets: [...d.widgets, widget] }
        : d
    )
    return id
  }

  function updateWidgetLayout(dashboardId, widgetId, layout) {
    dashboards.value = dashboards.value.map(d => {
      if (d.id !== dashboardId) return d
      return {
        ...d,
        widgets: d.widgets.map(w =>
          w.id === widgetId ? { ...w, layout: normaliseLayout(layout, widgetId) } : w
        ),
      }
    })
  }

  function updateWidgetLayouts(dashboardId, layouts) {
    // layouts: array of vue-grid-layout layout items { i, x, y, w, h }
    dashboards.value = dashboards.value.map(d => {
      if (d.id !== dashboardId) return d
      const byId = Object.fromEntries(layouts.map(l => [l.i, l]))
      return {
        ...d,
        widgets: d.widgets.map(w => {
          const l = byId[w.id]
          return l ? { ...w, layout: normaliseLayout(l, w.id) } : w
        }),
      }
    })
  }

  function updateWidgetTitle(dashboardId, widgetId, title) {
    dashboards.value = dashboards.value.map(d => {
      if (d.id !== dashboardId) return d
      return {
        ...d,
        widgets: d.widgets.map(w =>
          w.id === widgetId ? { ...w, title: String(title ?? '').trim() } : w
        ),
      }
    })
  }

  function updateWidgetProps(dashboardId, widgetId, props) {
    dashboards.value = dashboards.value.map(d => {
      if (d.id !== dashboardId) return d
      return {
        ...d,
        widgets: d.widgets.map(w =>
          w.id === widgetId ? { ...w, props: { ...props } } : w
        ),
      }
    })
  }

  function removeWidget(dashboardId, widgetId) {
    dashboards.value = dashboards.value.map(d => {
      if (d.id !== dashboardId) return d
      return { ...d, widgets: d.widgets.filter(w => w.id !== widgetId) }
    })
  }

  return {
    dashboards,
    activeDashboardId,
    activeDashboard,
    setActiveDashboard,
    addDashboard,
    renameDashboard,
    removeDashboard,
    addWidget,
    updateWidgetLayout,
    updateWidgetLayouts,
    updateWidgetTitle,
    updateWidgetProps,
    removeWidget,
  }
})
