<template>
  <div>
    <div>
      <pre v-bind:class="[mes.src]" v-for="mes in message" v-bind:key="mes.data">{{ mes.data }}</pre>
    </div>

    <v-text-field v-on:keypress.enter="submit"
      label="Outline"
      placeholder="Placeholder"
      v-model="command"
      outline
    ></v-text-field>  </div>
</template>

<script>
export default {
  name: 'Console',
  data() {
    return {
      term: null,
      terminalSocket: null,
      buffer: [],
      command: '',
    }
  },
  computed: {
    message() {
      this.buffer.push({ data: this.$store.state.socket.message, src: 'socket' })
      return this.buffer
    },
  },
  methods: {
    sendCommand: function(data) {
      this.buffer.push({ data, src: 'local' })
      this.$store.dispatch('sendMessage', data)
    },
    submit: function() {
      this.sendCommand(this.command)
      this.command = ''
    },
  },
  mounted() {
    this.sendCommand('list clients')
  },
  beforeDestroy() {
  },
}
</script>

<style lang="scss">
  .socket {
    background-color: palegreen;
  }

  .local {
    background-color: indianred;
  }
</style>
