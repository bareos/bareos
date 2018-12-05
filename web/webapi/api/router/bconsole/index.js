'use strict'

const path = require('path')
const routeBasePath = path.dirname(module.filename)
const routeBase = path.basename(routeBasePath)

const createConsole = require('../../bconsole').create
const closeConsole = require('../../bconsole').close

const uuidv4 = require('uuid/v4')

const consoles = new Map()

module.exports = router => {
  const Router = require('koa-router')
  const subRouter = new Router({
    prefix: `/${routeBase}`
  })

  subRouter.post('/', (ctx, next) => {
    const id = uuidv4()
    const bc = createConsole()
    bc.stdout.on('data', data => {
      console.log(`console ${id}: ${data}`)
      ctx.app.io.emit(id, data)
    })
    ctx.app.io.on('connection', client => {
      client.on(id, data => {
        console.log(`${id} sent: ${data}`)
        bc.stdin.write(data + '\n')
      })
    })
    consoles.set(id, { bc })
    ctx.body = { id }
    ctx.status = 201
    return next
  })

  subRouter.get('/', (ctx, next) => {
    ctx.body = { consoles: [...consoles.keys()] }
    return next
  })

  subRouter.get('/:id', (ctx, next) => {
    const id = ctx.params.id
    if (consoles.has(id)) {
      const console = consoles.get(id)
      ctx.body = { id, console }
    } else {
      ctx.status = 400
    }

    return next
  })

  subRouter.del('/:id', (ctx, next) => {
    const id = ctx.params.id
    if (consoles.has(id)) {
      const process = consoles.get(id).process
      closeConsole(process)
      consoles.delete(id)
      ctx.status = 200
    } else {
      ctx.status = 400
    }

    return next
  })

  router.use(subRouter.routes())

  return router
}
