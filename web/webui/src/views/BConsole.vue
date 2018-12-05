<template>
  <div class="container">
    <section>
      <pre>{{ consoleOutput }}</pre>
    </section>
    <section>
      <b-field label="Command">
        <b-input
          v-model="command"
          @keydown.native.enter="onSubmit"
        />
      </b-field>
    </section>
  </div>
</template>
<script>

export default {
  name: 'BConsole',
  data () {
    return {
      command: '',
      consoleOutput: ''
    }
  },
  created () {
    // const enc = new TextDecoder('utf-8')
    //    this.$connect('ws://localhost:9000/dingens')
    // this.$socket.send('some data')
    //    this.$ws.on('message', data => {
    //      console.log(data)
    //    })
    // this.sockets.subscribe(this.$route.params.id, data => {
    //   const result = enc.decode(data)
    //   this.consoleOutput += result
    //   console.log(result)
    // })
  },
  destroyed () {
    this.$disconnect()
  },
  methods: {
    onSubmit: function () {
      console.log(this.command)
      // if (this.$route.params.id) {
      //   console.log(this.$route.params.id)
      //   this.$socket.emit(this.$route.params.id, this.command)
      // }
      this.command = ''
    }
  }
}
</script>

<style scoped>
</style>
