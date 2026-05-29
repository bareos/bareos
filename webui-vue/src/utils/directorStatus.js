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

import { resolveOsIcon } from './osIcon.js'

export function resolveDirectorOsChip(full = '') {
  const lower = full.toLowerCase()
  if (lower.includes('darwin') || lower.includes('apple') || lower.includes('macos')) {
    return resolveOsIcon({ os: 'macos', osInfo: full })
  }
  if (lower.includes('windows') || lower.includes('win')) {
    return resolveOsIcon({ os: 'windows', osInfo: full })
  }
  return resolveOsIcon({ os: 'linux', osInfo: full })
}

export function buildDirectorStatusCards(statusSnapshots, configStatusAccessByDirector) {
  return statusSnapshots.map(({ director, header }) => ({
    ...header,
    scopeDirector: director,
    reportedDirector: header?.director || director,
    osIcon: resolveDirectorOsChip(header?.os ?? ''),
    configStatusAvailable: configStatusAccessByDirector[director]?.available === true,
    configStatusUnavailableReason: configStatusAccessByDirector[director]?.message ?? '',
  }))
}

export function isAclPermissionError(message = '') {
  return /acl|permission|authori[sz]ed/i.test(String(message))
}

export function configStatusChipColor(card) {
  if (!card.configStatusAvailable) {
    return 'grey-6'
  }
  return card.config_warnings ? 'negative' : 'positive'
}

export function configStatusChipIcon(card) {
  if (!card.configStatusAvailable) {
    return 'block'
  }
  return card.config_warnings ? 'error' : 'check_circle'
}

export function configStatusChipLabel(card, t) {
  if (!card.configStatusAvailable) {
    return t('Config Unavailable')
  }
  return card.config_warnings ? t('Config Warning') : t('Config OK')
}

export function configStatusChipTooltip(card, t) {
  if (card.configStatusAvailable) {
    return t('Click to show configuration status for this director')
  }
  if (isAclPermissionError(card.configStatusUnavailableReason)) {
    return t('Configuration status unavailable because the required ACL is missing.')
  }
  if (card.configStatusUnavailableReason) {
    return `${t('Configuration status unavailable')}: ${card.configStatusUnavailableReason}`
  }
  return t('Configuration status unavailable.')
}
