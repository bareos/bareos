'use strict'

module.exports = () => {
  const Router = require('koa-router')
  const router = new Router({
    prefix: '/api2'
  })

  require('./command')(router)
  require('./bconsole')(router)

  return router
}
