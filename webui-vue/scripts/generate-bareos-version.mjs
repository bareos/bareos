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

import { mkdir, readFile, writeFile } from 'node:fs/promises'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const scriptDir = path.dirname(fileURLToPath(import.meta.url))
const webuiVueDir = path.resolve(scriptDir, '..')
const templatePath = path.join(webuiVueDir, 'src/generated/bareos-version.js.in')
const outputPath = path.join(webuiVueDir, 'src/generated/bareos-version.js')

const bareosVersion = process.env.BAREOS_FULL_VERSION ?? 'dev'
const template = await readFile(templatePath, 'utf8')
const generated = template.replace('@BAREOS_FULL_VERSION@', bareosVersion)

await mkdir(path.dirname(outputPath), { recursive: true })

let current = null
try {
  current = await readFile(outputPath, 'utf8')
} catch {}

if (current !== generated) {
  await writeFile(outputPath, generated)
}
