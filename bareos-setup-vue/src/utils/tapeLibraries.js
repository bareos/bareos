function formatCount(count, singular, plural = `${singular}s`) {
  if (count === 1) return `1 ${singular}`
  return `${count ?? '-'} ${plural}`
}

export function formatChangerLabel(changer) {
  const parts = [changer?.vendor, changer?.model].filter(Boolean)
  return parts.join(' ') || changer?.identifier || changer?.display_name || changer?.path || '-'
}

export function formatChangerDetails(changer) {
  const identifier = changer?.identifier || changer?.display_name || changer?.path || '-'
  return `Firmware ${changer?.firmware_version || '-'} · Identifier ${identifier}`
}

export function formatChangerSummary(changer) {
  return [
    formatCount(changer?.status?.slots, 'slot'),
    formatCount(changer?.status?.ie_slots, 'i/e slot'),
    formatCount(changer?.status?.robot_arms, 'robot arm'),
    formatCount(changer?.status?.drives, 'tape drive'),
  ].join(' · ')
}

export function formatChangerDriveLabel(index, changerDrive, tapeDrive) {
  const parts = [
    `${index}.`,
    tapeDrive?.vendor || '-',
    tapeDrive?.model || '-',
    tapeDrive?.serial_number || '-',
    `Element ${changerDrive?.element_address ?? '-'}`,
  ]

  return parts.join(' ')
}

export function resolveChangerDrives(changer, tapeDrives) {
  return (changer?.drive_identifiers ?? []).map((changerDrive, index) => {
    const identifiers = new Set(changerDrive?.identifiers ?? [])
    const tapeDrive = (tapeDrives ?? []).find((drive) =>
      (drive?.device_identifiers ?? []).some((identifier) => identifiers.has(identifier))
    )

    return {
      key: `${changer?.path || 'changer'}-${changerDrive?.element_address ?? index}`,
      path: tapeDrive?.path || null,
      label: formatChangerDriveLabel(index + 1, changerDrive, tapeDrive),
      tapeDrive,
    }
  })
}
