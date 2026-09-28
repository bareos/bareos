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

import { messageId } from '../i18n/messageId.js'

/*
 * The director reports how it was built in "status director" ->
 * header.binary_info. The value comes from the build definitions used for
 * the packages (see core/src/lib/version.cc for the self-compiled default).
 */
export const BUILD_KINDS = Object.freeze({
  subscription: 'subscription',
  community: 'community',
  prerelease: 'prerelease',
  selfcompiled: 'selfcompiled',
  unknown: 'unknown',
})

export const BUILD_KIND_STORAGE_KEY = 'bareos-webui.buildKind'
export const RIBBON_DISMISSED_STORAGE_KEY = 'bareos-webui.unsupportedRibbonDismissed'

export const SERVICES_URL = 'https://www.bareos.com/services/'
export const EVALUATION_URL = 'https://www.bareos.com/try/'
export const SUBSCRIPTION_URL = 'https://www.bareos.com/product/subscription/'
export const SUPPORT_URL = 'https://www.bareos.com/product/support/'
export const EXPERT_CIRCLE_URL = 'https://www.bareos.com/meet/'

export const SUBSCRIPTION_ONLY_PLUGINS = ['Proxmox', 'Hyper-V', 'Barri']

export const UNSUPPORTED_BUILD_TEXT = 'Unsupported build – not for production use'

export const COMMERCIAL_OFFERINGS = Object.freeze([
  {
    id: 'evaluation',
    icon: 'rocket_launch',
    label: messageId('Try the subscription release'),
    description: messageId('Request free trial access to the tested subscription packages and plugins for your evaluation.'),
    url: EVALUATION_URL,
    highlight: true,
  },
  {
    id: 'subscription',
    icon: 'verified',
    label: messageId('Subscription packages'),
    description: messageId('Maintained and tested packages for all major platforms, plus subscription-only plugins.'),
    url: SUBSCRIPTION_URL,
  },
  {
    id: 'support',
    icon: 'support_agent',
    label: messageId('Professional support'),
    description: messageId('Direct help from the Bareos developers for production and regulated environments.'),
    url: SUPPORT_URL,
  },
  {
    id: 'training',
    icon: 'school',
    label: messageId('Training'),
    description: messageId('Administration courses and workshops for daily operation and restore workflows.'),
    url: 'https://www.bareos.com/learn/training/',
  },
  {
    id: 'consulting',
    icon: 'engineering',
    label: messageId('Consulting'),
    description: messageId('Migration support, setup reviews and best practices for stable operations.'),
    url: 'https://www.bareos.com/contact/',
  },
  {
    id: 'development',
    icon: 'extension',
    label: messageId('Sponsored development'),
    description: messageId('Get the feature, integration, plugin or platform support you need.'),
    url: 'https://www.bareos.com/contact/',
  },
  {
    id: 'expert-circle',
    icon: 'groups',
    label: messageId('Bareos Expert Circle'),
    description: messageId('Meet users, customers and Bareos experts online, discuss real use cases and get an early look at new features.'),
    url: EXPERT_CIRCLE_URL,
  },
])

/** Short list for the menus, most important first. */
export const MENU_OFFERINGS = Object.freeze(
  ['evaluation', 'subscription', 'support', 'expert-circle']
    .map(id => COMMERCIAL_OFFERINGS.find(offering => offering.id === id)),
)

export function classifyBinaryInfo(binaryInfo) {
  const text = String(binaryInfo ?? '').trim().toLowerCase()
  if (!text) return BUILD_KINDS.unknown
  if (text.includes('subscription')) return BUILD_KINDS.subscription
  if (text.includes('pre-release') || text.includes('prerelease')) return BUILD_KINDS.prerelease
  if (text.includes('community')) return BUILD_KINDS.community
  if (text.includes('self-compiled') || text.includes('self compiled')) return BUILD_KINDS.selfcompiled
  return BUILD_KINDS.unknown
}

/** Promote unless every known director is a subscription build. */
export function shouldPromote(kinds) {
  const list = [...(kinds ?? [])]
  if (!list.length) return true
  return list.some(kind => kind !== BUILD_KINDS.subscription)
}

/** The kind to describe the setup with: anything unsupported wins. */
export function summariseBuildKinds(kinds) {
  const list = [...(kinds ?? [])]
  for (const kind of [BUILD_KINDS.selfcompiled, BUILD_KINDS.prerelease,
    BUILD_KINDS.community, BUILD_KINDS.unknown]) {
    if (list.includes(kind)) return kind
  }
  return list.length ? BUILD_KINDS.subscription : BUILD_KINDS.unknown
}

export function buildKindLabel(kind) {
  switch (kind) {
    case BUILD_KINDS.subscription: return 'Subscription build'
    case BUILD_KINDS.community: return 'Community build'
    case BUILD_KINDS.prerelease: return 'Pre-release build'
    case BUILD_KINDS.selfcompiled: return 'Self-compiled build'
    default: return 'Community build'
  }
}

/**
 * Quasar Notify options shown once the unsupported-build ribbon is closed.
 * `t` translates, `open` opens an external page (window.open by default).
 */
export function closedRibbonNotification(t = text => text, open = openExternal) {
  return {
    message: t('For production use: try it for free or buy a subscription'),
    icon: 'rocket_launch',
    color: 'grey-9',
    textColor: 'white',
    position: 'bottom-right',
    timeout: 8000,
    classes: 'unsupported-build-toast',
    actions: [
      { label: t('Try for free'), color: 'amber', noCaps: true, handler: () => open(EVALUATION_URL) },
      { label: t('Buy a subscription'), color: 'amber', noCaps: true, handler: () => open(SUBSCRIPTION_URL) },
    ],
  }
}

function openExternal(url) {
  globalThis.open?.(url, '_blank', 'noopener,noreferrer')
}

/** Browser tab title, marked while an unsupported build is in use. */
export function formatDocumentTitle(pageTitle, unsupported = false) {
  const base = pageTitle ? `${pageTitle} - Bareos` : 'Bareos'
  return unsupported ? `${base} (unsupported build)` : base
}

function storage(storageImpl) {
  if (storageImpl !== undefined) return storageImpl
  try {
    return globalThis.localStorage ?? null
  } catch {
    return null
  }
}

export function loadCachedBuildKind(storageImpl) {
  try {
    const value = storage(storageImpl)?.getItem(BUILD_KIND_STORAGE_KEY)
    return Object.values(BUILD_KINDS).includes(value) ? value : BUILD_KINDS.unknown
  } catch {
    return BUILD_KINDS.unknown
  }
}

function sessionStore(storageImpl) {
  if (storageImpl !== undefined) return storageImpl
  try {
    return globalThis.sessionStorage ?? null
  } catch {
    return null
  }
}

/** The ribbon stays closed until the next login in this browser tab. */
export function loadRibbonDismissed(storageImpl) {
  try {
    return sessionStore(storageImpl)?.getItem(RIBBON_DISMISSED_STORAGE_KEY) === '1'
  } catch {
    return false
  }
}

export function storeRibbonDismissed(dismissed, storageImpl) {
  try {
    const store = sessionStore(storageImpl)
    if (dismissed) store?.setItem(RIBBON_DISMISSED_STORAGE_KEY, '1')
    else store?.removeItem(RIBBON_DISMISSED_STORAGE_KEY)
  } catch {
    // without storage the ribbon just stays closed until reload
  }
}

export function storeBuildKind(kind, storageImpl) {
  try {
    storage(storageImpl)?.setItem(BUILD_KIND_STORAGE_KEY, kind)
  } catch {
    // private mode or quota: the login note simply falls back to "unknown"
  }
}
