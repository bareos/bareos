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

    // this.$refs.xterm.term.write('* help\n')
    // this.$socket.send("help")
    // console.log(this.$store.state.socket.message)
    // this.$store.state.socket.message.forEach(element => {
    //   element = element.replace(/\n/g, '\r\n')
    //   this.$refs.xterm.term.write(element)
    // })

    //this.$refs.xterm.term.fit()

    this.$refs.xterm.term.on('key', (key, ev) => {
      console.log(key.charCodeAt(0))
      
      if(key.charCodeAt(this.input) == 118) {
        this.command += 'v'
        this.$refs.xterm.term.write('v')
      }

      if(key.charCodeAt(this.input) == 101) {
        this.command += 'e'
        this.$refs.xterm.term.write('e')
      }

      if(key.charCodeAt(this.input) == 114) {
        this.command += 'r'
        this.$refs.xterm.term.write('r')
      }

      if(key.charCodeAt(this.input) == 115) {
        this.command += 's'
        this.$refs.xterm.term.write('s')
      }

      if(key.charCodeAt(this.input) == 105) {
        this.command += 'i'
        this.$refs.xterm.term.write('i')
      }

      if(key.charCodeAt(this.input) == 111) {
        this.command += 'o'
        this.$refs.xterm.term.write('o')
      }

      if(key.charCodeAt(this.input) == 110) {
        this.command += 'n'
        this.$refs.xterm.term.write('n')
      }

      if(key.charCodeAt(this.input) == 13) {
        this.$refs.xterm.term.write('\n')
        this.$socket.send(this.command)
        this.input = 0
        this.command = ''
        this.$store.state.socket.message.forEach(element => {
          element = element.replace(/\n/g, '\r\n')
          this.$refs.xterm.term.write(element)
        })
        this.$refs.xterm.term.fit()
        this.$store.state.socket.message = []
      }

    })
    
  },
  created () {
    // this.$store.state.socket.message = []
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
