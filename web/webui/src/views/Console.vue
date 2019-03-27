<template>
  <section>

      <!-- <my-terminal :terminal="terminal" ref="xterm"></my-terminal> -->
      <my-terminal :terminal="terminal"></my-terminal>

  </section>
</template>

<script>

import Console from '../components/Console'

export default {
  data() {
    return {
      terminal: {},
      // output: '',
      // command: '',
      // terminal: {
      //   pid: 1,
      //   name: 'terminal',
      //   cols: 1000,
      //   rows: 1000
      // }

    }
  },
  methods: {
    // getOutput () {
    //   this.$socket.onmessage = (msg) => {
    //     this.$refs.xterm.term.write(msg.data)
    //   }
    //   this.$refs.xterm.term.write('* ')
    //   this.output = ''
    //   this.command = ''
    // }
  },
  beforeMounted() {
    console.log('debug: beforeMounted()')
    // delete this.$socket.onmessage
  },
  mounted() {
    console.log('debug: mounted()')
    // this.$refs.xterm.term.write('* ')
    // this.$refs.xterm.term.on('data', (data) => {
    //   const code = data.charCodeAt(0)
    //   if ((code < 32 || code === 127)) {
    //     switch (code) {
    //       case 13:
    //           console.log(this.command)
    //           this.$socket.send(this.command)
    //           this.$refs.xterm.term.write('\r\n')
    //           // this.getOutput()
    //         break
    //       default:
    //     }
    //   } else {
    //     // Visible
    //     this.$refs.xterm.term.write(data)
    //     this.command += data
    //   }
    // })
  },
  beforeUpdated() {
    console.log('debug: beforeUpdated()')
  },
  updated() {
    console.log('debug: updated()')
  },
  created() {
    console.log('debug: created()')
  },
  destroyed() {
    console.log('debug: destroyed()')
  },
  components: {
    'my-terminal': Console,
  },
  computed: {

  },
}
</script>

<style scoped>

</style>
