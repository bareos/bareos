/* eslint-disable new-cap */
const Koa = require('koa')
const app = new Koa()

app.server = require('http').createServer(app.callback())
app.listen = (...args) => {
  app.server.listen.call(app.server, ...args)
  return app.server
}

app.io = require('socket.io')(app.server, {
  transports: ['websocket']
})

const cors = require('@koa/cors')
app.use(cors())

const router = require('./router')()
app.use(router.routes())

const Boom = require('boom')
app.use(router.allowedMethods({
  throw: true,
  notImplemented: () => new Boom.notImplemented(),
  methodNotAllowed: () => new Boom.methodNotAllowed()
}))

console.log(router.stack.map(i => `[${i.methods.join()}]: ${i.path}`))

app.io.on('connection', function (socket) { console.log('a user connected') })

module.exports.app = app
