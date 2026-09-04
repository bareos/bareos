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

import { describe, expect, it } from 'vitest'
import {
  getAllWidgetDefinitions,
  getWidgetDefinition,
} from '../../src/dashboard/widgetRegistry.js'

describe('widgetRegistry', () => {
  it('getAllWidgetDefinitions returns a non-empty array', () => {
    const defs = getAllWidgetDefinitions()
    expect(Array.isArray(defs)).toBe(true)
    expect(defs.length).toBeGreaterThan(0)
  })

  it('every widget definition has required fields', () => {
    for (const def of getAllWidgetDefinitions()) {
      expect(typeof def.type,         `${def.type}.type`).toBe('string')
      expect(def.type.length,         `${def.type} type must not be empty`).toBeGreaterThan(0)
      expect(typeof def.label,        `${def.type}.label`).toBe('string')
      expect(typeof def.description,  `${def.type}.description`).toBe('string')
      expect(typeof def.icon,         `${def.type}.icon`).toBe('string')
      expect(typeof def.defaultTitle, `${def.type}.defaultTitle`).toBe('string')
      expect(typeof def.defaultLayout,`${def.type}.defaultLayout`).toBe('object')
      expect(typeof def.defaultLayout.w, `${def.type}.defaultLayout.w`).toBe('number')
      expect(typeof def.defaultLayout.h, `${def.type}.defaultLayout.h`).toBe('number')
      expect(Array.isArray(def.requiredProps), `${def.type}.requiredProps`).toBe(true)
      expect(Array.isArray(def.optionalProps), `${def.type}.optionalProps`).toBe(true)
      // component must be defined (async component or plain object)
      expect(def.component, `${def.type}.component`).toBeTruthy()
    }
  })

  it('all type keys are unique', () => {
    const defs = getAllWidgetDefinitions()
    const types = defs.map(d => d.type)
    const unique = new Set(types)
    expect(unique.size).toBe(types.length)
  })

  it('getWidgetDefinition returns the correct definition by type', () => {
    const defs = getAllWidgetDefinitions()
    for (const def of defs) {
      const found = getWidgetDefinition(def.type)
      expect(found).toBe(def)
    }
  })

  it('getWidgetDefinition returns null for unknown types', () => {
    expect(getWidgetDefinition('non-existent-type')).toBeNull()
    expect(getWidgetDefinition('')).toBeNull()
    expect(getWidgetDefinition(undefined)).toBeNull()
  })

  it('includes the expected built-in widget types', () => {
    const types = new Set(getAllWidgetDefinitions().map(d => d.type))
    expect(types.has('jobs-past-24h')).toBe(true)
    expect(types.has('recent-jobs-table')).toBe(true)
    expect(types.has('running-jobs')).toBe(true)
    expect(types.has('job-totals')).toBe(true)
    expect(types.has('pool-bytes-chart')).toBe(true)
    expect(types.has('pool-volumes-chart')).toBe(true)
    expect(types.has('database-status')).toBe(true)
    expect(types.has('analytics-summary')).toBe(true)
    expect(types.has('analytics-treemap')).toBe(true)
    expect(types.has('analytics-status-breakdown')).toBe(true)
    expect(types.has('analytics-client-bytes')).toBe(true)
    expect(types.has('analytics-level-distribution')).toBe(true)
  })

  it('pool chart widgets have appropriate default layout dimensions', () => {
    const bytesChart = getWidgetDefinition('pool-bytes-chart')
    const volChart   = getWidgetDefinition('pool-volumes-chart')

    expect(bytesChart.defaultLayout.w).toBeGreaterThanOrEqual(2)
    expect(bytesChart.defaultLayout.h).toBeGreaterThanOrEqual(3)
    expect(volChart.defaultLayout.w).toBeGreaterThanOrEqual(2)
    expect(volChart.defaultLayout.h).toBeGreaterThanOrEqual(3)
  })
})
