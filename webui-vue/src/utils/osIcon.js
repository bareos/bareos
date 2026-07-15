/**
 * Resolve the MDI v7 icon name and display colour for a client based on
 * the OS type and (where available) the Linux distribution string extracted
 * from the uname field.
 *
 * Supported distro icons from MDI v7:
 *   mdi-debian, mdi-ubuntu, mdi-fedora, mdi-redhat, mdi-centos,
 *   mdi-gentoo, mdi-freebsd, mdi-linux-mint, mdi-manjaro, mdi-raspberry-pi
 * All others fall back to mdi-linux.
 */

const DISTRO_MAP = [
  // Match on a substring of the osInfo string (case-insensitive).
  // Ordered from most-specific to least-specific.
  { match: 'raspberry pi os', icon: 'mdi-raspberry-pi',  color: 'red-7'    },
  { match: 'raspbian',        icon: 'mdi-raspberry-pi',  color: 'red-7'    },
  { match: 'linux mint',      icon: 'mdi-linux-mint',    color: 'green-7'  },
  { match: 'ubuntu',          icon: 'mdi-ubuntu',        color: 'deep-orange-7' },
  { match: 'debian',          icon: 'mdi-debian',        color: 'red-8'    },
  { match: 'fedora',          icon: 'mdi-fedora',        color: 'blue-7'   },
  { match: 'red hat',         icon: 'mdi-redhat',        color: 'red-7'    },
  { match: 'rhel',            icon: 'mdi-redhat',        color: 'red-7'    },
  { match: 'centos',          icon: 'mdi-centos',        color: 'purple-7' },
  { match: 'manjaro',         icon: 'mdi-manjaro',       color: 'green-8'  },
  { match: 'gentoo',          icon: 'mdi-gentoo',        color: 'purple-8' },
  { match: 'freebsd',         icon: 'mdi-freebsd',       color: 'red-9'    },
]

/**
 * @param {{ os: string, osInfo?: string }} client
 * @returns {{ icon: string, color: string }}
 */
export function resolveOsIcon(client) {
  const os     = client.os     ?? 'linux'
  const osInfo = client.osInfo ?? ''

  if (os === 'windows') return { icon: 'mdi-microsoft-windows', color: 'blue-7'  }
  if (os === 'macos')   return { icon: 'mdi-apple',             color: 'grey-8'  }

  // Try to find a distribution-specific icon
  const lower = osInfo.toLowerCase()
  for (const entry of DISTRO_MAP) {
    if (lower.includes(entry.match)) {
      return { icon: entry.icon, color: entry.color }
    }
  }

  return { icon: 'mdi-linux', color: 'orange-8' }
}

/** Convenience: just the icon name */
export function osIconName(client)  { return resolveOsIcon(client).icon  }
/** Convenience: just the colour */
export function osIconColor(client) { return resolveOsIcon(client).color }

/** Human-readable OS label (unchanged from before) */
export function osLabel(os) {
  if (os === 'windows') return 'Windows'
  if (os === 'macos')   return 'macOS'
  return 'Linux'
}
