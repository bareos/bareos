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

function hashDirectorName(name) {
  return [...String(name ?? '')].reduce(
    (hash, character) => ((hash * 33) + character.charCodeAt(0)) >>> 0,
    5381,
  )
}

const DIRECTOR_COLOR_PALETTE = [
  { background: '#1976D2', text: '#FFFFFF', border: '#1565C0' },
  { background: '#00897B', text: '#FFFFFF', border: '#00695C' },
  { background: '#EF6C00', text: '#FFFFFF', border: '#E65100' },
  { background: '#8E24AA', text: '#FFFFFF', border: '#6A1B9A' },
  { background: '#039BE5', text: '#FFFFFF', border: '#0277BD' },
  { background: '#C2185B', text: '#FFFFFF', border: '#880E4F' },
  { background: '#7CB342', text: '#FFFFFF', border: '#558B2F' },
  { background: '#546E7A', text: '#FFFFFF', border: '#37474F' },
]

function normalizeDirectorValues(values) {
  return [...new Set(
    (values ?? [])
      .map(value => String(value ?? '').trim())
      .filter(Boolean)
  )]
}

export function resolveDirectorColors(name, knownDirectors = []) {
  const normalizedName = String(name ?? '').trim()
  if (!normalizedName) {
    return {
      background: '#ECEFF1',
      text: '#37474F',
      border: '#B0BEC5',
    }
  }

  const orderedDirectors = normalizeDirectorValues(knownDirectors)
  const paletteIndex = orderedDirectors.indexOf(normalizedName)
  if (paletteIndex >= 0) {
    return DIRECTOR_COLOR_PALETTE[paletteIndex % DIRECTOR_COLOR_PALETTE.length]
  }

  return DIRECTOR_COLOR_PALETTE[
    hashDirectorName(normalizedName) % DIRECTOR_COLOR_PALETTE.length
  ]
}
