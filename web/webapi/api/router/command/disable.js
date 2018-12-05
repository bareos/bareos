const path = require('path')
const routeBasePath = path.basename(module.filename, '.js')

module.exports = router => {
  router.get(`/${routeBasePath}`, async (ctx, data) => {
    ctx.body = `The Module '${routeBasePath}' is not implemented`
  })
}

// actions
// todo: disable job=<name>
