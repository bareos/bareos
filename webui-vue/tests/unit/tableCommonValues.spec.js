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
  collectCommonColumnValues,
  columnIsUniform,
  columnValueOf,
  distinctColumnValues,
  normaliseCommonValue,
} from '../../src/utils/tableCommonValues.js'

const columns = [
  { name: 'volumename', label: 'Name', field: 'volumename' },
  { name: 'pool', label: 'Pool', field: 'pool' },
  { name: 'mediatype', label: 'Media Type', field: 'mediatype' },
  { name: 'retention', label: 'Retention', field: (row) => row.volretention },
]

const rows = [
  { volumename: 'Full-0001', pool: 'Full', mediatype: 'File', volretention: 31536000 },
  { volumename: 'Full-0002', pool: 'Full', mediatype: 'File', volretention: 31536000 },
  { volumename: 'Full-0003', pool: 'Full', mediatype: 'File', volretention: 86400 },
]

describe('normaliseCommonValue', () => {
  it('maps null and undefined to an empty string', () => {
    expect(normaliseCommonValue(null)).toBe('')
    expect(normaliseCommonValue(undefined)).toBe('')
  })

  it('trims strings and stringifies everything else', () => {
    expect(normaliseCommonValue('  File ')).toBe('File')
    expect(normaliseCommonValue(42)).toBe('42')
    expect(normaliseCommonValue(false)).toBe('false')
  })
})

describe('columnValueOf', () => {
  it('reads plain fields and calls function fields', () => {
    expect(columnValueOf(rows[0], columns[1])).toBe('Full')
    expect(columnValueOf(rows[0], columns[3])).toBe(31536000)
  })

  it('tolerates missing rows or columns', () => {
    expect(columnValueOf(null, columns[0])).toBeUndefined()
    expect(columnValueOf(rows[0], null)).toBeUndefined()
  })
})

describe('distinctColumnValues', () => {
  it('stops collecting once the limit is exceeded', () => {
    expect(distinctColumnValues(rows, columns[0])).toHaveLength(3)
    expect(distinctColumnValues(rows, columns[1])).toEqual(['Full'])
  })
})

describe('columnIsUniform', () => {
  it('needs at least two rows', () => {
    expect(columnIsUniform([rows[0]], columns[1])).toBe(false)
    expect(columnIsUniform(rows, columns[1])).toBe(true)
    expect(columnIsUniform(rows, columns[3])).toBe(false)
  })
})

describe('collectCommonColumnValues', () => {
  it('returns only the columns that hold one single value', () => {
    const common = collectCommonColumnValues(rows, columns)
    expect(common.map((entry) => entry.name)).toEqual(['pool', 'mediatype'])
    expect(common[0]).toMatchObject({ label: 'Pool', value: 'Full', display: 'Full' })
  })

  it('honours the exclude list', () => {
    const common = collectCommonColumnValues(rows, columns, { exclude: ['pool'] })
    expect(common.map((entry) => entry.name)).toEqual(['mediatype'])
  })

  it('applies a formatter to the collapsed value', () => {
    const uniform = rows.map((row) => ({ ...row, volretention: 86400 }))
    const common = collectCommonColumnValues(uniform, columns, {
      format: { retention: (value) => `${value / 86400} days` },
    })
    expect(common.find((entry) => entry.name === 'retention').display).toBe('1 days')
  })

  it('skips empty values and short row sets', () => {
    expect(collectCommonColumnValues([rows[0]], columns)).toEqual([])
    const blank = rows.map((row) => ({ ...row, pool: '', mediatype: 'File' }))
    expect(collectCommonColumnValues(blank, columns).map((e) => e.name)).toEqual(['mediatype'])
  })
})
