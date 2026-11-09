/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

import { describe, expect, it } from 'vitest'
import {
  buildDashboardSlugMap,
  dashboardSlugFor,
  resolveDashboardIdFromSlug,
  slugify,
} from '../../src/utils/dashboardRoute.js'

describe('dashboard route helpers', () => {
  // ── slugify ──────────────────────────────────────────────────────────────

  it('slugifies simple names', () => {
    expect(slugify('Operations')).toBe('operations')
    expect(slugify('System Overview')).toBe('system-overview')
  })

  it('collapses runs of non-alphanumeric characters', () => {
    expect(slugify('  My   Dashboard!! ')).toBe('my-dashboard')
    expect(slugify('Client / Job Report')).toBe('client-job-report')
  })

  it('falls back to a default slug for empty or punctuation-only names', () => {
    expect(slugify('')).toBe('dashboard')
    expect(slugify('   ')).toBe('dashboard')
    expect(slugify('!!!')).toBe('dashboard')
    expect(slugify(undefined)).toBe('dashboard')
    expect(slugify(null)).toBe('dashboard')
  })

  it('handles unicode by stripping non-ascii-alphanumeric characters', () => {
    expect(slugify('Übersicht')).toBe('bersicht')
  })

  // ── buildDashboardSlugMap / dashboardSlugFor ────────────────────────────

  it('maps each dashboard id to its slugified name', () => {
    const dashboards = [
      { id: 'default', name: 'Operations' },
      { id: 'analytics', name: 'System Overview' },
    ]

    const map = buildDashboardSlugMap(dashboards)
    expect(map.get('default')).toBe('operations')
    expect(map.get('analytics')).toBe('system-overview')

    expect(dashboardSlugFor(dashboards, 'default')).toBe('operations')
    expect(dashboardSlugFor(dashboards, 'analytics')).toBe('system-overview')
  })

  it('returns an empty string for an unknown dashboard id', () => {
    const dashboards = [{ id: 'default', name: 'Operations' }]
    expect(dashboardSlugFor(dashboards, 'missing')).toBe('')
  })

  it('de-duplicates colliding slugs by appending a numeric suffix in order', () => {
    const dashboards = [
      { id: 'a', name: 'Reports' },
      { id: 'b', name: 'Reports' },
      { id: 'c', name: 'reports!!' },
      { id: 'd', name: 'Reports' },
    ]

    const map = buildDashboardSlugMap(dashboards)
    expect(map.get('a')).toBe('reports')
    expect(map.get('b')).toBe('reports-2')
    expect(map.get('c')).toBe('reports-3')
    expect(map.get('d')).toBe('reports-4')
  })

  it('ignores dashboards without a usable id', () => {
    const dashboards = [{ name: 'No Id' }, { id: 'ok', name: 'Fine' }]
    const map = buildDashboardSlugMap(dashboards)
    expect(map.size).toBe(1)
    expect(map.get('ok')).toBe('fine')
  })

  it('handles a non-array input gracefully', () => {
    expect(buildDashboardSlugMap(undefined).size).toBe(0)
    expect(buildDashboardSlugMap(null).size).toBe(0)
  })

  // ── resolveDashboardIdFromSlug ──────────────────────────────────────────

  it('resolves a known slug back to its dashboard id', () => {
    const dashboards = [
      { id: 'default', name: 'Operations' },
      { id: 'analytics', name: 'System Overview' },
    ]

    expect(resolveDashboardIdFromSlug(dashboards, 'operations')).toBe('default')
    expect(resolveDashboardIdFromSlug(dashboards, 'system-overview')).toBe('analytics')
  })

  it('returns null for an unknown or stale slug', () => {
    const dashboards = [{ id: 'default', name: 'Operations' }]
    expect(resolveDashboardIdFromSlug(dashboards, 'does-not-exist')).toBeNull()
  })

  it('returns null for an empty slug', () => {
    const dashboards = [{ id: 'default', name: 'Operations' }]
    expect(resolveDashboardIdFromSlug(dashboards, '')).toBeNull()
    expect(resolveDashboardIdFromSlug(dashboards, null)).toBeNull()
    expect(resolveDashboardIdFromSlug(dashboards, undefined)).toBeNull()
  })

  it('round-trips slug -> id -> slug for de-duplicated names', () => {
    const dashboards = [
      { id: 'a', name: 'Reports' },
      { id: 'b', name: 'Reports' },
    ]

    const idForSecond = resolveDashboardIdFromSlug(dashboards, 'reports-2')
    expect(idForSecond).toBe('b')
    expect(dashboardSlugFor(dashboards, idForSecond)).toBe('reports-2')
  })
})
