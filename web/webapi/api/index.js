const { app } = require('./server')
const config = require('config')

const bconsole = config.get('bareos.bconsole_executable')
const fs = require('fs')

if (!bconsole) {
  console.log('config: bareos.bconsole_executable not set')
} else if (!fs.existsSync(bconsole)) {
  console.log(`BCONSOLE not found: ${bconsole}`)
} else {
  console.log(`using bconsole: '${bconsole}'`)
  console.log('listening on http://localhost:3000')
  app.listen(3000)
}
