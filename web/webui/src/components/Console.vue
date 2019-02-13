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
    }
  },
  mounted () {
    console.log('pid : ' + this.terminal.pid + ' is on ready')
    let terminalContainer = document.getElementById('terminal')
    this.term = new Terminal()
    this.term.open(terminalContainer)
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
