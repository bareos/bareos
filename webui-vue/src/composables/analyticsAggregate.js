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

import { directorCollection, normaliseJob } from './useDirectorFetch.js'
import { createDirectorCommandClient } from './directorAggregate.js'

function quoteDirectorString(value) {
  return `"${String(value).replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`
}

function decorateJobs(entries, director) {
  return directorCollection(entries).map((entry) => {
    const job = normaliseJob(entry)
    return {
      ...job,
      director,
      scopeKey: `${director}:${job.id}`,
    }
  })
}

function sortNames(values) {
  return [...new Set(values.filter(Boolean))].sort((left, right) => left.localeCompare(right))
}

function normaliseMetricSeries(result) {
  const dataSourceNames = Array.isArray(result?.data_source_names)
    ? result.data_source_names.filter(Boolean)
    : []

  return {
    available: result?.available === true,
    start: Number(result?.start ?? 0),
    end: Number(result?.end ?? 0),
    step: Number(result?.step ?? 0),
    dataSourceNames,
    points: directorCollection(result?.series).map((point) => {
      const timestamp = Number(point?.timestamp ?? 0)
      const values = Object.fromEntries(dataSourceNames.flatMap((name) => {
        const number = Number(point?.[name])
        return Number.isFinite(number) ? [[name, number]] : []
      }))

      return { timestamp, values }
    }).filter(point => point.timestamp > 0),
  }
}

function mergeMetricSeries(seriesList) {
  const availableSeries = seriesList.filter(Boolean)
  const dataSourceNames = sortNames(availableSeries.flatMap(
    series => series.dataSourceNames ?? []
  ))
  const points = new Map()

  for (const series of availableSeries) {
    for (const point of series.points ?? []) {
      if (!points.has(point.timestamp)) {
        points.set(point.timestamp, { timestamp: point.timestamp, values: {} })
      }

      const target = points.get(point.timestamp)
      for (const [name, value] of Object.entries(point.values ?? {})) {
        target.values[name] = (target.values[name] ?? 0) + value
      }
    }
  }

  return {
    available: availableSeries.some(series => series.available),
    start: availableSeries.reduce((value, series) => (
      value === 0 || (series.start > 0 && series.start < value) ? series.start : value
    ), 0),
    end: availableSeries.reduce((value, series) => Math.max(value, series.end ?? 0), 0),
    step: availableSeries.reduce((value, series) => (
      value === 0 || (series.step > 0 && series.step < value) ? series.step : value
    ), 0),
    dataSourceNames,
    points: [...points.values()].sort((left, right) => left.timestamp - right.timestamp),
  }
}

function sortJobs(jobs) {
  return [...jobs].sort((left, right) => {
    const nameCompare = String(left.name ?? '').localeCompare(String(right.name ?? ''))
    if (nameCompare !== 0) {
      return nameCompare
    }

    const directorCompare = String(left.director ?? '').localeCompare(String(right.director ?? ''))
    if (directorCompare !== 0) {
      return directorCompare
    }

    return Number(right.id ?? 0) - Number(left.id ?? 0)
  })
}

export async function fetchAggregatedAnalytics(credentials, directors) {
  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      const result = await client.call('list jobs')
      return decorateJobs(result?.jobs, director)
    } finally {
      client.disconnect()
    }
  }))

  return {
    jobs: sortJobs(results
      .filter(result => result.status === 'fulfilled')
      .flatMap(result => result.value)),
    directorErrors: results.flatMap((result, index) => (
      result.status === 'rejected'
        ? [{
          director: directors[index],
          message: result.reason?.message ?? 'Failed to load analytics data.',
        }]
        : []
    )),
  }
}

export async function fetchAggregatedAnalyticsHistory(
  credentials,
  directors,
  options = {},
) {
  const hours = Math.max(1, Number(options.hours ?? 24))
  const cf = options.cf ?? 'last'
  const selectedPool = typeof options.pool === 'string' ? options.pool : ''

  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      const [poolsResult, systemResult, poolResult] = await Promise.allSettled([
        client.call('list pools'),
        client.call(`.metrics kind=system hours=${hours} cf=${cf}`),
        selectedPool
          ? client.call(
            `.metrics kind=pool pool=${quoteDirectorString(selectedPool)} hours=${hours} cf=${cf}`
          )
          : Promise.resolve(null),
      ])

      const errors = [
        poolsResult.status === 'rejected'
          ? { director, message: poolsResult.reason?.message ?? 'Failed to load pool list.' }
          : null,
        systemResult.status === 'rejected'
          ? { director, message: systemResult.reason?.message ?? 'Failed to load system metrics.' }
          : null,
        poolResult.status === 'rejected'
          ? { director, message: poolResult.reason?.message ?? 'Failed to load pool metrics.' }
          : null,
      ].filter(Boolean)

      return {
        director,
        errors,
        poolNames: poolsResult.status === 'fulfilled'
          ? directorCollection(poolsResult.value?.pools).map(pool => pool?.name).filter(Boolean)
          : [],
        systemHistory: systemResult.status === 'fulfilled'
          ? normaliseMetricSeries(systemResult.value)
          : null,
        poolHistory: poolResult.status === 'fulfilled' && poolResult.value
          ? normaliseMetricSeries(poolResult.value)
          : null,
      }
    } finally {
      client.disconnect()
    }
  }))

  return {
    poolNames: sortNames(results
      .filter(result => result.status === 'fulfilled')
      .flatMap(result => result.value.poolNames)),
    systemHistory: mergeMetricSeries(results
      .filter(result => result.status === 'fulfilled')
      .map(result => result.value.systemHistory)),
    poolHistory: mergeMetricSeries(results
      .filter(result => result.status === 'fulfilled')
      .map(result => result.value.poolHistory)),
    directorErrors: results.flatMap((result, index) => {
      if (result.status === 'rejected') {
        return [{
          director: directors[index],
          message: result.reason?.message ?? 'Failed to load analytics history.',
        }]
      }

      return result.value.errors
    }),
  }
}
