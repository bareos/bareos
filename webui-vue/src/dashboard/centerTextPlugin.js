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
 * Chart.js plugin that draws one or more lines of text centred inside a
 * doughnut chart's hole.
 *
 * Enabled per-chart-instance via the `centerText` plugin option, e.g.:
 *   options: {
 *     plugins: {
 *       centerText: {
 *         lines: ['1.2 TB', 'Total'],
 *       },
 *     },
 *   }
 *
 * Does nothing (and adds no overhead) for charts that don't set this option,
 * so it is safe to register globally alongside Pie/Doughnut charts.
 */
export const CenterTextPlugin = {
  id: 'centerText',
  afterDraw(chart, _args, pluginOptions) {
    const lines = pluginOptions?.lines
    if (!lines?.length) {
      return
    }

    const { ctx, chartArea } = chart
    if (!chartArea) {
      return
    }

    const centerX = (chartArea.left + chartArea.right) / 2
    const centerY = (chartArea.top + chartArea.bottom) / 2
    const lineHeight = pluginOptions.lineHeight ?? 16

    ctx.save()
    ctx.textAlign = 'center'
    ctx.textBaseline = 'middle'

    lines.forEach((line, i) => {
      ctx.font = pluginOptions.fonts?.[i] ?? pluginOptions.font ?? '600 14px sans-serif'
      ctx.fillStyle = pluginOptions.colors?.[i] ?? pluginOptions.color ?? '#333'
      const offset = (i - (lines.length - 1) / 2) * lineHeight
      ctx.fillText(line, centerX, centerY + offset)
    })

    ctx.restore()
  },
}
