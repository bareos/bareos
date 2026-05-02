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

import { createServer } from 'node:http'
import { promises as fs } from 'node:fs'
import path from 'node:path'

function parseArgs(argv) {
  const options = {
    host: '127.0.0.1',
    port: 4173,
    root: path.resolve('dist'),
  }

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i]
    if (arg === '--host') options.host = argv[++i]
    if (arg === '--port') options.port = Number(argv[++i])
    if (arg === '--root') options.root = path.resolve(argv[++i])
  }

  return options
}

const mimeTypes = {
  '.css': 'text/css; charset=utf-8',
  '.html': 'text/html; charset=utf-8',
  '.ico': 'image/x-icon',
  '.js': 'text/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
}

async function readFileOrNull(filePath) {
  try {
    return await fs.readFile(filePath)
  } catch {
    return null
  }
}

const { host, port, root } = parseArgs(process.argv.slice(2))

const server = createServer(async (req, res) => {
  const url = new URL(req.url ?? '/', `http://${req.headers.host ?? `${host}:${port}`}`)
  const requestPath = decodeURIComponent(url.pathname)
  const candidate = path.join(root, requestPath)
  const safeCandidate = path.resolve(candidate)

  if (!safeCandidate.startsWith(root)) {
    res.writeHead(403)
    res.end('Forbidden')
    return
  }

  let filePath = safeCandidate
  let body = await readFileOrNull(filePath)
  if (!body && requestPath !== '/') {
    filePath = path.join(root, 'index.html')
    body = await readFileOrNull(filePath)
  }
  if (!body && requestPath === '/') {
    filePath = path.join(root, 'index.html')
    body = await readFileOrNull(filePath)
  }

  if (!body) {
    res.writeHead(404)
    res.end('Not Found')
    return
  }

  const ext = path.extname(filePath)
  res.writeHead(200, {
    'Content-Type': mimeTypes[ext] || 'application/octet-stream',
  })
  res.end(body)
})

server.listen(port, host, () => {
  console.log(`Serving ${root} on http://${host}:${port}`)
})
