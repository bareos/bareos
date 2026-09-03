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

function normaliseItems(items, totalArea, totalValue) {
  return (items ?? [])
    .map((item, index) => ({
      ...item,
      _index: index,
      _area: (item.value / totalValue) * totalArea,
    }))
    .sort((a, b) => (b.value - a.value) || (a._index - b._index))
}

function sumAreas(items) {
  return items.reduce((sum, item) => sum + item._area, 0)
}

function worstAspectRatio(items, sideLength) {
  if (!items.length || sideLength <= 0) return Infinity

  const areas = items.map(item => item._area)
  const rowArea = areas.reduce((sum, area) => sum + area, 0)
  const maxArea = Math.max(...areas)
  const minArea = Math.min(...areas)
  const sideSquared = sideLength * sideLength
  const rowAreaSquared = rowArea * rowArea

  return Math.max(
    (sideSquared * maxArea) / rowAreaSquared,
    rowAreaSquared / (sideSquared * minArea)
  )
}

function layoutRow(items, rect) {
  const rowArea = sumAreas(items)
  const tiles = []

  if (rect.width >= rect.height) {
    const rowWidth = rect.height > 0 ? rowArea / rect.height : 0
    let cursorY = rect.y

    for (const [index, item] of items.entries()) {
      const remainingHeight = rect.y + rect.height - cursorY
      const itemHeight = index === items.length - 1
        ? remainingHeight
        : (rowWidth > 0 ? item._area / rowWidth : 0)

      tiles.push({
        ...item,
        x: rect.x,
        y: cursorY,
        width: rowWidth,
        height: itemHeight,
      })
      cursorY += itemHeight
    }

    return {
      tiles,
      nextRect: {
        x: rect.x + rowWidth,
        y: rect.y,
        width: Math.max(0, rect.width - rowWidth),
        height: rect.height,
      },
    }
  }

  const rowHeight = rect.width > 0 ? rowArea / rect.width : 0
  let cursorX = rect.x

  for (const [index, item] of items.entries()) {
    const remainingWidth = rect.x + rect.width - cursorX
    const itemWidth = index === items.length - 1
      ? remainingWidth
      : (rowHeight > 0 ? item._area / rowHeight : 0)

    tiles.push({
      ...item,
      x: cursorX,
      y: rect.y,
      width: itemWidth,
      height: rowHeight,
    })
    cursorX += itemWidth
  }

  return {
    tiles,
    nextRect: {
      x: rect.x,
      y: rect.y + rowHeight,
      width: rect.width,
      height: Math.max(0, rect.height - rowHeight),
    },
  }
}

export function squarifyTreemap(items, { x = 0, y = 0, width, height }) {
  if (!Array.isArray(items) || items.length === 0) return []
  if (!(width > 0) || !(height > 0)) return []

  const totalValue = items.reduce((sum, item) => sum + item.value, 0)
  if (!(totalValue > 0)) return []

  const totalArea = width * height
  const pending = normaliseItems(items, totalArea, totalValue)
  const tiles = []
  let rect = { x, y, width, height }
  let row = []

  while (pending.length > 0) {
    const nextItem = pending[0]
    const sideLength = Math.min(rect.width, rect.height)
    const currentWorst = row.length > 0 ? worstAspectRatio(row, sideLength) : Infinity
    const nextWorst = worstAspectRatio([...row, nextItem], sideLength)

    if (row.length === 0 || nextWorst <= currentWorst) {
      row.push(nextItem)
      pending.shift()
      continue
    }

    const laidOut = layoutRow(row, rect)
    tiles.push(...laidOut.tiles)
    rect = laidOut.nextRect
    row = []
  }

  if (row.length > 0) {
    tiles.push(...layoutRow(row, rect).tiles)
  }

  return tiles.map(({ _area, _index, ...item }) => item)
}
