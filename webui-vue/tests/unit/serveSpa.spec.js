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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

import { spawn } from 'node:child_process'
import { once } from 'node:events'
import { mkdtemp, mkdir, writeFile, rm } from 'node:fs/promises'
import http from 'node:http'
import os from 'node:os'
import path from 'node:path'
import { afterAll, beforeAll, describe, expect, it } from 'vitest'

describe('SPA server under /bareos/', () => {
  let root
  let sibling
  let upstream
  let frontend
  let baseUrl

  beforeAll(async () => {
    root = await mkdtemp(path.join(os.tmpdir(), 'bareos-spa-test-'))
    sibling = `${root}-sibling`
    await mkdir(sibling)
    await writeFile(path.join(sibling, 'outside.txt'), 'outside the document root')
    await mkdir(path.join(root, 'assets'))
    await writeFile(path.join(root, 'index.html'), '<html>SPA</html>')
    await writeFile(path.join(root, 'assets', 'app.js'), 'window.loaded = true')

    upstream = http.createServer((req, res) => {
      let body = ''
      req.on('data', chunk => { body += chunk })
      req.on('end', () => {
        res.setHeader('Content-Type', 'application/json')
        res.end(JSON.stringify({ url: req.url, method: req.method, body }))
      })
    })
    upstream.on('upgrade', (req, socket) => {
      socket.end(
        'HTTP/1.1 101 Switching Protocols\r\nConnection: Upgrade\r\n' +
        `Upgrade: websocket\r\nX-Upstream-Path: ${req.url}\r\n\r\n`,
      )
    })
    upstream.listen(0, '127.0.0.1')
    await once(upstream, 'listening')

    frontend = spawn(process.execPath, [
      'scripts/serve-spa.mjs', '--port', '0', '--root', root,
      '--ws-proxy-host', '127.0.0.1', '--ws-proxy-port', String(upstream.address().port),
    ], { cwd: path.resolve(import.meta.dirname, '../..'), stdio: ['ignore', 'pipe', 'pipe'] })
    baseUrl = await new Promise((resolve, reject) => {
      let output = ''
      frontend.stdout.on('data', chunk => {
        output += chunk
        const match = output.match(/on (http:\/\/127\.0\.0\.1:\d+)/)
        if (match) resolve(match[1])
      })
      frontend.on('error', reject)
      frontend.on('exit', code => reject(new Error(`SPA server exited with ${code}: ${output}`)))
      frontend.stderr.on('data', chunk => { output += chunk })
    })
  })

  afterAll(async () => {
    if (frontend && frontend.exitCode === null) {
      const exited = once(frontend, 'exit')
      frontend.kill()
      await exited
    }
    if (upstream) await new Promise(resolve => upstream.close(resolve))
    if (root) await rm(root, { recursive: true, force: true })
    if (sibling) await rm(sibling, { recursive: true, force: true })
  })

  it('serves JavaScript assets rather than the SPA fallback', async () => {
    const response = await fetch(`${baseUrl}/bareos/assets/app.js`)
    expect(response.headers.get('content-type')).toContain('text/javascript')
    expect(await response.text()).toBe('window.loaded = true')
  })

  it('serves the SPA for base and nested routes', async () => {
    for (const route of ['/bareos/', '/bareos/jobs/42']) {
      expect(await (await fetch(baseUrl + route)).text()).toBe('<html>SPA</html>')
    }
  })

  it('proxies API requests with the prefix stripped and encoding preserved', async () => {
    const response = await fetch(`${baseUrl}/bareos/api/session/site%2Fname?next=a%2Fb`, {
      method: 'POST',
      body: 'login-body',
    })
    expect(await response.json()).toEqual({
      url: '/api/session/site%2Fname?next=a%2Fb', method: 'POST', body: 'login-body',
    })
  })

  it('forwards WebSocket upgrades to the unprefixed upstream route', async () => {
    const headers = await new Promise((resolve, reject) => {
      const request = http.get(`${baseUrl}/bareos/ws?session=a%2Fb`, {
        headers: { Connection: 'Upgrade', Upgrade: 'websocket' },
      })
      request.on('upgrade', (response, socket) => {
        socket.destroy()
        resolve(response.headers)
      })
      request.on('response', response => {
        response.resume()
        reject(new Error(`Expected upgrade, got ${response.statusCode}`))
      })
      request.on('error', reject)
    })
    expect(headers['x-upstream-path']).toBe('/ws?session=a%2Fb')
  })

  it('rejects encoded traversal to a sibling sharing the root prefix', async () => {
    const response = await fetch(`${baseUrl}/bareos/..%2f${path.basename(sibling)}/outside.txt`)
    expect(response.status).toBe(403)
    expect(await response.text()).not.toContain('outside the document root')
  })

  it('rejects malformed encoding without terminating the server', async () => {
    const response = await fetch(`${baseUrl}/bareos/%E0%A4%A`)
    expect(response.status).toBe(400)
    expect(await (await fetch(`${baseUrl}/bareos/assets/app.js`)).text())
      .toBe('window.loaded = true')
  })
})
