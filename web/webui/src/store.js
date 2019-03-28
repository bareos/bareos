import Vue from 'vue'
import Vuex from 'vuex'

import concat from 'lodash/concat'
import map from 'lodash/map'


Vue.use(Vuex)

const appendMessage = (state, message, src) => {
  let count = state.socket.message
  const rearranged = map(message.trimRight().split('\n'),
      (v) => {return { data: v, src: src, id: count++ }})
  state.socket.message = concat(state.socket.message, rearranged)
}


export default new Vuex.Store({
  state: {
    socket: {
      isConnected: false,
      message: [],
      reconnectError: false,
    },
  },
  mutations: {
    initializeStore(state) {
      // Check if the ID exists
      if (localStorage.getItem('store')) {
        // Replace the state object with the stored item
        this.replaceState(
            Object.assign(state, JSON.parse(localStorage.getItem('store')))
        )
      }
    },
    SOCKET_ONOPEN(state, event) {
      Vue.prototype.$socket = event.currentTarget
      state.socket.isConnected = true
      // Vue.prototype.$socket.send('.api 2')
    },
    SOCKET_ONCLOSE(state, event) {
      state.socket.isConnected = false
    },
    SOCKET_ONERROR(state, event) {
      console.error(state, event)
      console.log('SOCKET_ONERROR')
    },
    // default handler called for all methods
    SOCKET_ONMESSAGE(state, message) {
      // state.socket.message.push(message.data)
      appendMessage(state, message.data, 'socket')
    },
    // mutations for reconnect methods
    SOCKET_RECONNECT(state, count) {
      console.log('SOCKET_ORECONNECT')
    },
    SOCKET_RECONNECT_ERROR(state) {
      state.socket.reconnectError = true
      console.log('SOCKET_ONRECONNECT_ERROR')
    },
  },
  actions: {
    sendMessage(context, message) {
      appendMessage(context.state, message, 'local')
      Vue.prototype.$socket.send(message)
    },
    receiveMessage(context) {
      return context.state.socket.message
    },
  },
})
