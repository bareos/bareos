<template>
  <v-container>

    <vue-term></vue-term>

    <pre style="overflow: auto; width: 1080px; height: 650px; background: #000; color: #fff;">
        <span v-for="m in consoleOutput" :key="m.id">{{ m }}</span>
    </pre>

    <v-text-field v-model="command" @keydown.native.enter="onSubmit"
      label="Enter Command"
    >
    </v-text-field>

  </v-container>
</template>

<script>

export default {
  name: 'BConsole',
  data () {
    return {
      command: ''
      // consoleOutput: ''
    }
  },
  created () {
    // const enc = new TextDecoder('utf-8')
    //    this.$connect('ws://localhost:9000/dingens')
    //    this.$socket.send(command)
    //    this.$ws.on('message', data => {
    //      console.log(data)
    //    })
    // this.sockets.subscribe(this.$route.params.id, data => {
    //   const result = enc.decode(data)
    //   this.consoleOutput += result
    //   console.log(result)
    // })
    this.$store.state.socket.message = []
  },
  destroyed () {
    // this.$disconnect()
    this.$store.state.socket.message = []
  },
  methods: {
    onSubmit: function () {
      console.log(this.command)
      // if (this.$route.params.id) {
      //   console.log(this.$route.params.id)
      //   this.$socket.emit(this.$route.params.id, this.command)
      // }
      this.$socket.send(this.command)
      this.command = ''
    }
  },
  computed: {
    consoleOutput () {
      console.dir(this.$store.state.socket.message)
      return this.$store.state.socket.message
    }
  }
}
</script>

<style scoped>
</style>
