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

/**
 * URL slug helpers for the multi-dashboard feature.
 *
 * Dashboards are identified internally by a random, stable `id`, but that
 * id is not a good fit for a bookmarkable URL. Instead we derive a
 * human-readable slug from the (user-editable, renamable) dashboard name.
 * The slug is *derived*, not stored: it is recomputed from the current
 * dashboards list on every navigation, so renaming a dashboard changes its
 * slug and any old bookmarked URL simply falls back to the default
 * dashboard (see resolveDashboardIdFromSlug()).
 */

const FALLBACK_SLUG = 'dashboard'

/**
 * Turns a dashboard name into a URL-friendly slug: lowercase, runs of
 * non-alphanumeric characters collapsed to a single '-', leading/trailing
 * '-' stripped. Falls back to FALLBACK_SLUG if the result would be empty
 * (e.g. a name made up entirely of punctuation or emoji).
 */
export function slugify(name) {
  const slug = String(name ?? '')
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '-')
    .replace(/^-+|-+$/g, '')

  return slug || FALLBACK_SLUG
}

/**
 * Builds a Map<id, slug> for the given dashboards, in array order.
 * Dashboards whose name slugifies to the same value as an earlier
 * dashboard get a numeric suffix ('-2', '-3', ...) appended, so slugs stay
 * unique regardless of duplicate/similar dashboard names.
 */
export function buildDashboardSlugMap(dashboards) {
  const list = Array.isArray(dashboards) ? dashboards : []
  const slugById = new Map()
  const usedCounts = new Map()

  for (const dashboard of list) {
    const id = dashboard?.id
    if (id === undefined || id === null) continue

    const base = slugify(dashboard?.name)
    const count = usedCounts.get(base) ?? 0
    usedCounts.set(base, count + 1)

    const slug = count === 0 ? base : `${base}-${count + 1}`
    slugById.set(id, slug)
  }

  return slugById
}

/** Returns the slug for a given dashboard id, or '' if the id is unknown. */
export function dashboardSlugFor(dashboards, id) {
  return buildDashboardSlugMap(dashboards).get(id) ?? ''
}

/**
 * Resolves a URL slug back to a dashboard id. Returns null if no dashboard
 * matches (e.g. the dashboard was deleted, or the slug is stale because the
 * dashboard was renamed).
 */
export function resolveDashboardIdFromSlug(dashboards, slug) {
  if (!slug) return null

  const slugById = buildDashboardSlugMap(dashboards)
  for (const [id, candidate] of slugById) {
    if (candidate === slug) return id
  }
  return null
}
