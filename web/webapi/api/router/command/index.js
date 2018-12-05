'use strict'

const fs = require('fs')
const path = require('path')
const routeBasePath = path.dirname(module.filename)
const thisModuleFileName = path.basename(module.filename)
const routeBase = path.basename(routeBasePath)

module.exports = router => {
  const Router = require('koa-router')
  const subRouter = new Router({
    prefix: `/${routeBase}`
  })

  fs.readdirSync(path.join(routeBasePath))
    .filter(file => file !== thisModuleFileName)
    .forEach(file => {
      // console.log(`register route module '${routeBase}/${file}'`)
      require(path.join(routeBasePath, file))(subRouter)
    })

  router.use(subRouter.routes())

  return router
}
