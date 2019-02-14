<template>
    <div class="console" id="terminal"></div>
</template>

<script>

import Terminal from '../Xterm'

export default {
  name: 'Console',
  props: {
    terminal: {
      type: Object,
      default: {}
    }
  },
  data () {
    return {
      term: null,
      terminalSocket: null
    }
  },
  methods: {
    runRealTerminal () {
      console.log('webSocket is finished')
    },
    errorRealTerminal () {
      console.log('error')
    },
    closeRealTerminal () {
      console.log('close')
    },
    ondataRealTerminal (data) {
      console.log('ondata')
      console.log(data)
    },
    onmessageRealTerminal (data) {
      console.log('onmessage')
      console.log(data.data)
    }
  },
  mounted () {
    console.log('pid : ' + this.terminal.pid + ' is on ready')
    let terminalContainer = document.getElementById('terminal')
    this.term = new Terminal({
      // cursorBlink: true,
      cols: 128,
      rows: 32
    })
    this.term.open(terminalContainer)
    this.terminalSocket = this.$socket
    this.terminalSocket.onopen = this.runRealTerminal
    this.terminalSocket.onclose = this.closeRealTerminal
    this.terminalSocket.onerror = this.errorRealTerminal
    this.term.attach(this.terminalSocket)
    this.terminalSocket.onmessage = (data) => { this.onmessageRealTerminal(data) }
    this.term.on('data', (data) => { this.ondataRealTerminal(data) })
    this.term._initialized = true
    console.log('mounted is going on')
    this.term.fit()
  },
  beforeDestroy () {
    this.terminalSocket.close()
    this.term.destroy()
  }
}
</script>
