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

/** Accessible colour palette for pie/doughnut charts. */
export const PIE_PALETTE = [
  '#0075be', // bareos blue
  '#e06c2e', // orange
  '#2eb87a', // green
  '#b82e8a', // magenta
  '#2eb8b8', // teal
  '#b8b82e', // yellow-olive
  '#6c2eb8', // purple
  '#2e6cb8', // mid blue
  '#b82e2e', // red
  '#2eb82e', // bright green
  '#b86c2e', // amber
  '#2e2eb8', // indigo
]

/**
 * Returns '#222' or '#fff', whichever gives better contrast when used as
 * text/label colour on top of the given hex background colour. Used for
 * in-slice data labels so they stay readable across the whole palette.
 */
export function getContrastTextColor(hexColor) {
  const hex = hexColor.replace('#', '')
  const linear = (channel) => {
    const value = channel / 255
    return value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4
  }
  const r = linear(parseInt(hex.substring(0, 2), 16))
  const g = linear(parseInt(hex.substring(2, 4), 16))
  const b = linear(parseInt(hex.substring(4, 6), 16))
  const luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b
  const darkContrast = (luminance + 0.05) / (linear(34) + 0.05)
  const whiteContrast = 1.05 / (luminance + 0.05)
  return darkContrast > whiteContrast ? '#222' : '#fff'
}
