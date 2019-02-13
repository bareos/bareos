<template>
	<section>
	    <div id="log" style="margin-top:20px;">
			    <my-terminal :terminal="terminal" ref="xterm"></my-terminal>
	    </div>
	</section>
</template>

<script>

import Console from '../components/Console'

export default {
  data () {
    return {

      env: '',
      podName: '',
      contaName: '',
      logtxt: '',
      input: 0,
      command: '',
			 terminal: {
			  pid: 1,
			  name: 'terminal',
			  cols: 1000,
        rows: 1000,
        cursorBlink: true
      }
      
    }
  },
  methods: {

  },
  mounted () {
    this.$refs.xterm.term.write('* ')

    this.$refs.xterm.term.on('data', (data) => {
      const code = data.charCodeAt(0)

      if (code == 13) {
        // CR received
        console.log(this.command)
        this.$socket.send(this.command)

        this.$store.state.socket.message.forEach(element => {
          element = element.replace(/\n/g, '\r\n')
          this.$refs.xterm.term.write(element)
        })
        
        this.$refs.xterm.term.writeln('')
        this.$refs.xterm.term.fit()

        this.$store.state.socket.message = []

        // this.$refs.xterm.term.write("\r\nYou typed: '" + this.command + "'\r\n")
        this.$refs.xterm.term.write('* ')
        this.command = ''
      } else if (code < 32 || code == 127) {
        // Control codes

      } else {
        // Visible
        this.$refs.xterm.term.write(data)
        this.command += data
      }

    })
    
  },
  created () {
    this.$store.state.socket.message = []
  },
  destroyed () {
    this.$store.state.socket.message = []
  },
  components: {
		    'my-terminal': Console
  },
  computed: {

  }
}
</script>

<style scoped>

</style>
