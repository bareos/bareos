export function projectScheduleSelection(options, intent, customKeys) {
  const optionKeys = options.map(option => option.key)
  if (intent === 'all') return optionKeys
  if (intent === 'none') return []
  return optionKeys.filter(key => customKeys.has(key))
}
