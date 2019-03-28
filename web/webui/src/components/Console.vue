<template>
  <div class="console">
    <div class="console-container" id="console-container">
      <div class="console-row" v-bind:class="[mes.src]"
           v-for="mes in message"
           v-bind:key="mes.count">
        <span v-if="mes.src == 'local'" class="console-local-prompt">&amp;&gt;</span>
        <span v-if="mes.src == 'socket'" class="console-socket-prompt">&gt;</span>
        <pre>{{ mes.data }}</pre>
      </div>
    </div>
    <div class="console-input">
      <v-text-field v-on:keypress.enter="submit"
                    placeholder="help"
                    v-model="command"
      ></v-text-field>
    </div>
  </div>
</template>

<script>
import concat from 'lodash/concat'
import map from 'lodash/map'

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
      this.appendMessage(this.$store.state.socket.message, 'socket')
      this.scrollToEnd()
      return this.buffer
    },
  },
  methods: {
    appendMessage(message, src) {
      let count = this.buffer.length
      const rearrange = map(message.trimRight().split('\n'), (v) => {return { data: v, src: src, id: count++ }})
      this.buffer = concat(this.buffer, rearrange)
    },
    sendCommand: function(data) {
      this.appendMessage(data, 'local')
      this.scrollToEnd()
      this.$store.dispatch('sendMessage', data)
    },
    submit: function() {
      this.sendCommand(this.command)
      this.command = ''
    },
    scrollToEnd: function() {
      this.$nextTick(() => {
        const container = this.$el.querySelector('#console-container')
        container.scrollTop = container.scrollHeight
      })
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
  pre {
    white-space: pre-wrap;       /* Since CSS 2.1 */
    white-space: -moz-pre-wrap;  /* Mozilla, since 1999 */
    word-wrap: break-word;       /* Internet Explorer 5.5+ */
  }

  .console {
    display: flex;
    flex-direction: column;
    max-height: 80vh;
    height: 80vh;
    background-color: blueviolet;
    margin: auto;
  }

  .console-container {
    overflow-y: scroll;
    overflow-x: scroll;
    margin-left:auto;
    margin-right: auto;
    overflow: auto;
    scroll-behavior: smooth;
    width: 80vw;
  }

  .console-local-prompt {
    color: greenyellow;
    margin-right: 1em;
  }

  .console-socket-prompt {
    color: deepskyblue;
    margin-right: 1em;
  }

  .console-row {
    display: flex;
    flex-direction: row;
    background-color: black;
    color: lightgray;
  }

  .console-input {
  }

  .socket {
  }

  .local {

  }
</style>
