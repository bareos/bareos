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

import { API_BASE_URL } from './directorCommandSocket.js'

export const SESSION_AUTH_PASSWORD = '__proxy_session__'

async function parseJsonResponse(response) {
  const text = await response.text()
  if (!text) {
    return null
  }

  try {
    return JSON.parse(text)
  } catch {
    throw new Error('Invalid session API response')
  }
}

export async function loginDirectorProxySession({ username, password, director }) {
  const response = await fetch(`${API_BASE_URL}/session/directors/${encodeURIComponent(director)}/login`, {
    method: 'POST',
    credentials: 'same-origin',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ username, password }),
  })

  const payload = await parseJsonResponse(response)
  if (!response.ok) {
    throw new Error(payload?.message || 'Authentication failed')
  }

  return payload
}

export async function fetchCurrentSession() {
  const response = await fetch(`${API_BASE_URL}/session`, {
    method: 'GET',
    credentials: 'same-origin',
    cache: 'no-store',
  })

  if (response.status === 401) {
    return null
  }

  const payload = await parseJsonResponse(response)
  if (!response.ok) {
    throw new Error(payload?.message || 'Could not restore session')
  }

  return payload
}

export async function deleteProxySession() {
  await fetch(`${API_BASE_URL}/session`, {
    method: 'DELETE',
    credentials: 'same-origin',
    cache: 'no-store',
  })
}

export async function reuseProxySessionCredentials({ directors, sourceDirector }) {
  const response = await fetch(`${API_BASE_URL}/session/reuse`, {
    method: 'POST',
    credentials: 'same-origin',
    cache: 'no-store',
    headers: {
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({
      directors,
      ...(sourceDirector ? { sourceDirector } : {}),
    }),
  })

  const payload = await parseJsonResponse(response)
  if (!response.ok) {
    throw new Error(payload?.message || 'Could not reuse current credentials')
  }

  return payload
}
