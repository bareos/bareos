<template>
  <div class="console" v-on:click="focusInput()">
    <div class="console-container" id="console-container">
      <div class="console-row" v-bind:class="[mes.src]"
           v-for="mes in message"
           v-bind:key="mes.id">
        <span v-if="mes.src == 'local'" class="console-local-prompt">*</span>
        <pre>{{ mes.data }}</pre>
      </div>
      <div class="console-input">
        <span class="console-local-prompt">*</span>
        <input class="console-prompt"
               ref="consolePrompt"
               v-on:keypress.enter="submit"
               placeholder="help" type="text"
               v-model="command"
        >
      </div>
    </div>
  </div>
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
      this.scrollToEnd()
      return this.$store.getters.messages('console')
    },
  },
  methods: {
    focusInput() {
      const el = this.$refs.consolePrompt
      this.$nextTick(function() {el.focus()})
    },
    sendCommand: async function(data) {
      try {
        await this.$store.dispatch('sendMessage', data)
      } catch (e) {
        this.$log.error(e)
      }
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
  beforeDestroy() {
  },
}
</script>

<style lang="scss">
  pre {
    // white-space: pre-wrap;       /* Since CSS 2.1 */
    // white-space: -moz-pre-wrap;  /* Mozilla, since 1999 */
    // word-wrap: break-word;       /* Internet Explorer 5.5+ */
  }

  .console {
    display: flex;
    flex-direction: column;
    max-height: calc(100vh - 120px);
    // height: 100vh;
    margin: auto;
    overflow: no-content;
    background-color: black;
  }

  .console-container {
    overflow-y: scroll;
    overflow-x: scroll;
    margin-left: 10px;
    margin-right: 10px;
    overflow: auto;
    scroll-behavior: smooth;
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
    display: flex;
    flex-direction: row;
    background-color: black;

  }

  .console-prompt {
    margin: auto;
    width: 100%;
    background-color: black;
    color: white;
    font-family: monospace;
    border: 0;
  }

  .socket {
  }

  .local {

  }

  textarea:focus, input:focus{
    outline: none;
  }
</style>
