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

export const SERVICES_URL = 'https://www.bareos.com/services/'
export const EVALUATION_URL = 'https://www.bareos.com/try/'
export const SUBSCRIPTION_URL = 'https://www.bareos.com/product/subscription/'
export const SUPPORT_URL = 'https://www.bareos.com/product/support/'
export const EXPERT_CIRCLE_URL = 'https://www.bareos.com/meet/'

export const SUBSCRIPTION_ONLY_PLUGINS = ['Proxmox', 'Hyper-V', 'Barri']

export const UNSUPPORTED_BUILD_TEXT = 'Unsupported build – no official subscription'

export const COMMERCIAL_OFFERINGS = Object.freeze([
  {
    id: 'evaluation',
    icon: 'rocket_launch',
    label: 'Try the subscription release',
    description: 'Request free trial access to the tested subscription packages and plugins for your evaluation.',
    url: EVALUATION_URL,
    highlight: true,
  },
  {
    id: 'subscription',
    icon: 'verified',
    label: 'Subscription packages',
    description: 'Maintained and tested packages for all major platforms, plus subscription-only plugins.',
    url: SUBSCRIPTION_URL,
  },
  {
    id: 'support',
    icon: 'support_agent',
    label: 'Professional support',
    description: 'Direct help from the Bareos developers for production and regulated environments.',
    url: SUPPORT_URL,
  },
  {
    id: 'training',
    icon: 'school',
    label: 'Training',
    description: 'Administration courses and workshops for daily operation and restore workflows.',
    url: 'https://www.bareos.com/learn/training/',
  },
  {
    id: 'consulting',
    icon: 'engineering',
    label: 'Consulting',
    description: 'Migration support, setup reviews and best practices for stable operations.',
    url: 'https://www.bareos.com/contact/',
  },
  {
    id: 'development',
    icon: 'extension',
    label: 'Sponsored development',
    description: 'Get the feature, integration, plugin or platform support you need.',
    url: 'https://www.bareos.com/contact/',
  },
  {
    id: 'expert-circle',
    icon: 'groups',
    label: 'Bareos Expert Circle',
    description: 'Meet users, customers and Bareos experts online, discuss real use cases and get an early look at new features.',
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

export function storeBuildKind(kind, storageImpl) {
  try {
    storage(storageImpl)?.setItem(BUILD_KIND_STORAGE_KEY, kind)
  } catch {
    // private mode or quota: the login note simply falls back to "unknown"
  }
}
