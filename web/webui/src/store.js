import Vue from 'vue'
import Vuex from 'vuex'

import concat from 'lodash/concat'
import map from 'lodash/map'


Vue.use(Vuex)

const appendMessage = (state, socket, message, src) => {
  const buffer = state.socket[socket]
  let count = buffer.message.length
  const rearranged = map(message.trimRight().split('\n'),
      (v) => {return { data: v, src: src, id: count++ }})
  buffer.message = concat(buffer.message, rearranged)
}

const appendMessageObject = (state, socket, message, src) => {
  const buffer = state.socket[socket]
  let count = buffer.message.length
  const o = { data: JSON.parse(message), src: src, id: count++ }
  buffer.message = concat(buffer.message, o)
}


export default new Vuex.Store({
  state: {
    socket: {},
  },
  mutations: {
    initializeStore(state) {
      // checkout store data from localStorage
      // if (localStorage.getItem('store')) {
      //   // Replace the state object with the stored item
      //   this.replaceState(
      //       Object.assign(state, JSON.parse(localStorage.getItem('store'))),
      //   )
      // }

      state.messages = []
    },
    SOCKET_ONOPEN(state, event) {
      let socketBuffer = state.socket[event.socket]
      if (!socketBuffer) {
        socketBuffer = {
          message: [],
        }
      }
      socketBuffer.isConnected = true
      state.socket[event.socket] = socketBuffer

      if (event.socket == 'api2') {
        console.log('api2 connected... switching to api2')
        Vue.prototype.$wsSend(event.socket, '.api 2')
      }
    },
    SOCKET_ONCLOSE(state, event) {
      state.socket[event.socket].isConnected = false
    },
    SOCKET_ONERROR(state, event) {
      console.error(state, event)
      console.log('SOCKET_ERROR')
    },
    // default handler called for all methods
    SOCKET_ONMESSAGE(state, message) {
      if (message.socket === 'console') {
        appendMessage(state, message.socket, message.data, 'socket')
      } else if (message.socket === 'api2') {
        appendMessageObject(state, message.socket, message.data, 'socket')
      }
    },
  },
  actions: {
    async sendMessage(context, message) {
      appendMessage(context.state, 'console', message, 'local')
      Vue.prototype.$wsSend('console', message)
    },
  },
  getters: {
    messages: (state) => (socketName) => state.socket[socketName].message,
  },
})
