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


import { ref } from 'vue'
import { describe, expect, it } from 'vitest'
import { useFilteredOptions } from '../../src/composables/useFilteredOptions.js'

const run = (filter, value) => filter(value, callback => callback())

describe('useFilteredOptions', () => {
  it('starts with all options', () => {
    const options = ref([{ label: 'Alpha' }, { label: 'Beta' }])
    const { filtered } = useFilteredOptions(options)
    expect(filtered.value).toEqual(options.value)
  })

  it('filters case-insensitively by label', () => {
    const options = ref([{ label: 'Alpha' }, { label: 'Beta' }])
    const { filtered, filter } = useFilteredOptions(options)

    run(filter, 'AL')
    expect(filtered.value).toEqual([{ label: 'Alpha' }])
  })

  it('restores all options for an empty search', () => {
    const options = ref([{ label: 'Alpha' }, { label: 'Beta' }])
    const { filtered, filter } = useFilteredOptions(options)

    run(filter, 'beta')
    run(filter, '')
    expect(filtered.value).toHaveLength(2)
  })

  it('follows changes of the options', async () => {
    const options = ref([{ label: 'Alpha' }])
    const { filtered } = useFilteredOptions(options)

    options.value = [{ label: 'Gamma' }]
    await Promise.resolve()
    expect(filtered.value).toEqual([{ label: 'Gamma' }])
  })
})
