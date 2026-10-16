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

import { describe, expect, it } from 'vitest'
import {
  buildPoolDefaultDriftIndex,
  volumeDriftFields,
  POOL_DEFAULT_FIELDS,
  buildApplyPoolDefaultsCommand,
  collectPoolDefaultDrift,
  compareVolumeToPoolDefaults,
  indexPoolsByScope,
  normalisePoolDefaultValue,
  poolScopeKey,
} from '../../src/utils/poolDefaults.js'

function pool(overrides = {}) {
  return {
    director: 'prod-a',
    name: 'Full',
    volretention: '31536000',
    voluseduration: '0',
    maxvoljobs: '0',
    maxvolfiles: '0',
    maxvolbytes: '0',
    recycle: '1',
    actiononpurge: '0',
    recyclepoolid: '0',
    minblocksize: '0',
    maxblocksize: '0',
    ...overrides,
  }
}

function volume(overrides = {}) {
  return {
    director: 'prod-a',
    volumename: 'Full-0001',
    pool: 'Full',
    volretention: '31536000',
    voluseduration: '0',
    maxvoljobs: '0',
    maxvolfiles: '0',
    maxvolbytes: '0',
    recycle: '1',
    actiononpurge: '0',
    recyclepoolid: '0',
    minblocksize: '0',
    maxblocksize: '0',
    ...overrides,
  }
}

describe('poolDefaults', () => {
  it('covers every field that frompool=yes writes', () => {
    expect(POOL_DEFAULT_FIELDS.map(field => field.id)).toEqual([
      'volretention',
      'voluseduration',
      'maxvoljobs',
      'maxvolfiles',
      'maxvolbytes',
      'recycle',
      'actiononpurge',
      'recyclepoolid',
      'minblocksize',
      'maxblocksize',
    ])
  })

  it('treats the catalog spellings of "unset" as equal', () => {
    expect(normalisePoolDefaultValue('0')).toBe(0)
    expect(normalisePoolDefaultValue(0)).toBe(0)
    expect(normalisePoolDefaultValue('')).toBeNull()
    expect(normalisePoolDefaultValue(null)).toBeNull()
    expect(normalisePoolDefaultValue(undefined)).toBeNull()
    expect(normalisePoolDefaultValue(false)).toBe(0)
    expect(normalisePoolDefaultValue(true)).toBe(1)
    expect(normalisePoolDefaultValue('not a number')).toBeNull()
  })

  it('reports no difference for a volume matching its pool', () => {
    expect(compareVolumeToPoolDefaults(volume(), pool())).toEqual([])
  })

  it('reports the fields that deviate', () => {
    const differences = compareVolumeToPoolDefaults(
      volume({ volretention: '2592000', maxvolbytes: '10737418240' }),
      pool()
    )

    expect(differences.map(entry => entry.field.id)).toEqual([
      'volretention',
      'maxvolbytes',
    ])
    expect(differences[0]).toMatchObject({
      volumeValue: 2592000,
      poolValue: 31536000,
    })
  })

  it('skips fields the director does not report', () => {
    const partialVolume = volume()
    const partialPool = pool()
    delete partialVolume.minblocksize
    delete partialPool.minblocksize
    partialVolume.maxblocksize = '65536'

    const differences = compareVolumeToPoolDefaults(partialVolume, partialPool)
    expect(differences.map(entry => entry.field.id)).toEqual(['maxblocksize'])
  })

  it('returns nothing without a volume or a pool', () => {
    expect(compareVolumeToPoolDefaults(null, pool())).toEqual([])
    expect(compareVolumeToPoolDefaults(volume(), null)).toEqual([])
  })

  it('indexes pools per director', () => {
    const index = indexPoolsByScope([
      pool(),
      pool({ director: 'prod-b', volretention: '86400' }),
    ])

    expect(index.size).toBe(2)
    expect(index.get(poolScopeKey('prod-b', 'Full')).volretention).toBe('86400')
  })

  it('collects drifting volumes and skips those without a known pool', () => {
    const drift = collectPoolDefaultDrift(
      [
        volume(),
        volume({ volumename: 'Full-0002', volretention: '86400' }),
        volume({ volumename: 'Other-0001', pool: 'Unknown', recycle: '0' }),
        volume({ volumename: 'Full-0003', director: 'prod-b', maxvoljobs: '5' }),
      ],
      [pool(), pool({ director: 'prod-b' })]
    )

    expect(drift.map(entry => entry.volume.volumename)).toEqual([
      'Full-0002',
      'Full-0003',
    ])
    expect(drift[0].differences.map(entry => entry.field.id)).toEqual([
      'volretention',
    ])
    expect(drift[1].pool.director).toBe('prod-b')
  })

  it('builds the apply command with a quoted volume name', () => {
    expect(buildApplyPoolDefaultsCommand('Full-0001')).toBe(
      'update volume="Full-0001" frompool=yes'
    )
  })
})

describe('buildPoolDefaultDriftIndex', () => {
  const pool = { director: 'd1', name: 'Full', volretention: '31536000', maxvolbytes: '0' }
  const volumes = [
    { director: 'd1', volumename: 'V1', pool: 'Full', volretention: '86400', maxvolbytes: '0' },
    { director: 'd1', volumename: 'V2', pool: 'Full', volretention: '31536000', maxvolbytes: '0' },
    { director: 'd1', volumename: 'V3', pool: 'Other', volretention: '1' },
  ]

  it('indexes only the deviating volumes and fields', () => {
    const index = buildPoolDefaultDriftIndex(volumes, [pool])
    expect(index.size).toBe(1)
    expect([...volumeDriftFields(index, volumes[0]).keys()]).toEqual(['volretention'])
  })

  it('returns an empty map for conforming or unknown volumes', () => {
    const index = buildPoolDefaultDriftIndex(volumes, [pool])
    expect(volumeDriftFields(index, volumes[1]).size).toBe(0)
    expect(volumeDriftFields(index, volumes[2]).size).toBe(0)
    expect(volumeDriftFields(null, volumes[0]).size).toBe(0)
  })
})
