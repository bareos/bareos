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
import { squarifyTreemap } from '../../src/utils/treemapLayout.js'

function tileArea(tile) {
  return tile.width * tile.height
}

function sumTileAreas(tiles) {
  return tiles.reduce((sum, tile) => sum + tileArea(tile), 0)
}

function overlapArea(a, b) {
  const width = Math.max(0, Math.min(a.x + a.width, b.x + b.width) - Math.max(a.x, b.x))
  const height = Math.max(0, Math.min(a.y + a.height, b.y + b.height) - Math.max(a.y, b.y))
  return width * height
}

describe('squarifyTreemap', () => {
  it('returns an empty array for empty input', () => {
    expect(squarifyTreemap([], { width: 400, height: 300 })).toEqual([])
  })

  it('fills the whole rectangle for a single item', () => {
    const [tile] = squarifyTreemap([{ key: 'only', value: 7 }], {
      x: 5,
      y: 10,
      width: 320,
      height: 180,
    })

    expect(tile).toMatchObject({
      key: 'only',
      value: 7,
      x: 5,
      y: 10,
      width: 320,
      height: 180,
    })
  })

  it('keeps tile areas proportional to values and fills the target area', () => {
    const width = 300
    const height = 200
    const totalArea = width * height
    const values = [6, 3, 1]
    const tiles = squarifyTreemap(values.map((value, index) => ({ key: `k${index}`, value })), {
      width,
      height,
    })

    expect(tiles).toHaveLength(values.length)
    expect(sumTileAreas(tiles)).toBeCloseTo(totalArea, 6)

    const totalValue = values.reduce((sum, value) => sum + value, 0)
    for (const tile of tiles) {
      const expectedArea = totalArea * (tile.value / totalValue)
      expect(tileArea(tile)).toBeCloseTo(expectedArea, 6)
    }
  })

  it('tiles many equal-value items without overlaps or gaps', () => {
    const width = 480
    const height = 320
    const tiles = squarifyTreemap(
      Array.from({ length: 16 }, (_, index) => ({ key: `eq-${index}`, value: 1 })),
      { width, height }
    )

    expect(tiles).toHaveLength(16)
    expect(sumTileAreas(tiles)).toBeCloseTo(width * height, 6)

    for (const tile of tiles) {
      expect(tile.width).toBeGreaterThan(0)
      expect(tile.height).toBeGreaterThan(0)
      expect(tile.x).toBeGreaterThanOrEqual(0)
      expect(tile.y).toBeGreaterThanOrEqual(0)
      expect(tile.x + tile.width).toBeLessThanOrEqual(width + 1e-6)
      expect(tile.y + tile.height).toBeLessThanOrEqual(height + 1e-6)
      expect(tileArea(tile)).toBeCloseTo((width * height) / 16, 6)
    }

    for (let i = 0; i < tiles.length; i += 1) {
      for (let j = i + 1; j < tiles.length; j += 1) {
        expect(overlapArea(tiles[i], tiles[j])).toBeLessThan(1e-6)
      }
    }
  })

  it('handles very different magnitudes without errors', () => {
    const width = 500
    const height = 300
    const values = [1000, 2, 2, 1, 1, 1]
    const tiles = squarifyTreemap(values.map((value, index) => ({ key: `v-${index}`, value })), {
      width,
      height,
    })

    expect(tiles).toHaveLength(values.length)
    expect(sumTileAreas(tiles)).toBeCloseTo(width * height, 6)

    const largest = tiles.find(tile => tile.key === 'v-0')
    const smallest = tiles.find(tile => tile.key === 'v-3')

    expect(largest.width).toBeGreaterThan(0)
    expect(largest.height).toBeGreaterThan(0)
    expect(smallest.width).toBeGreaterThan(0)
    expect(smallest.height).toBeGreaterThan(0)
    expect(tileArea(largest)).toBeGreaterThan(tileArea(smallest))
  })
})
