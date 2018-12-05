const spawn = require('child_process').spawn
const config = require('config')

const bconsoleAsync = async (command, api = 2) => {
  const bconsole = spawn(config.get('bareos.bconsole_executable'))
  bconsole.stdin.write(`.api ${api}\n`)
  bconsole.stdin.write(command + '\n')
  bconsole.stdin.write('exit\n')

  let consoleOutput = ''
  for await (const data of bconsole.stdout) {
    consoleOutput += data.toString()
  }

  const commandPos = consoleOutput.indexOf(command) + command.length
  const exitPos = consoleOutput.indexOf('exit\n{', commandPos)
  let result = consoleOutput.substr(commandPos, exitPos - commandPos)
  return JSON.parse(result).result
}

const create = (api = 0) => {
  const bconsole = spawn(config.get('bareos.bconsole_executable'))
  bconsole.stdin.write(`.api ${api}\n`)

  return bconsole
}

const close = (bconsole) => {
  if (bconsole) {
    bconsole.stdin.write(`exit\n`)
  }
}

module.exports = {
  bconsoleAsync,
  create,
  close
}
