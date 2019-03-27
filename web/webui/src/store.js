import Vue from 'vue'
import Vuex from 'vuex'

Vue.use(Vuex)

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
      console.log('SOCKET_ONOPEN')
    },
    SOCKET_ONCLOSE(state, event) {
      state.socket.isConnected = false
      console.log('SOCKET_ONCLOSE')
    },
    SOCKET_ONERROR(state, event) {
      console.error(state, event)
      console.log('SOCKET_ONERROR')
    },
    // default handler called for all methods
    SOCKET_ONMESSAGE(state, message) {
      // state.socket.message.push(message.data)
      state.socket.message = message.data
      console.log('SOCKET_ONMESSAGE')
      console.log(message)
    },
    // mutations for reconnect methods
    SOCKET_RECONNECT(state, count) {
      console.info(state, count)
      console.log('SOCKET_ORECONNECT')
    },
    SOCKET_RECONNECT_ERROR(state) {
      state.socket.reconnectError = true
      console.log('SOCKET_ONRECONNECT_ERROR')
    },
  },
  actions: {
    sendMessage(context, message) {
      Vue.prototype.$socket.send(message)
    },
    receiveMessage() {
      return this.state.socket.message
    },
  },
})
