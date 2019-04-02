import Vue from 'vue'
import Vuex from 'vuex'

import concat from 'lodash/concat'
import map from 'lodash/map'


Vue.use(Vuex)

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
    },
    INIT_SOCKET(state, event) {
      state.socket[event.socket] = {
        message: [],
        isConnected: true,
      }
    },
    APPEND_MESSAGE(state, event) {
      if (!state.socket[event.socket]) {
        state.socket[event.socket] = {
          message: [],
        }
      }
      const buffer = state.socket[event.socket]
      let count = buffer.message.length
      const rearranged = map(event.message.trimRight().split('\n'),
        (v) => {return { data: v, src: event.src, id: count++ }})
      buffer.message = concat(buffer.message, rearranged)
    },
    APPEND_OBJECT(state, event) {
      const buffer = state.socket[event.socket]
      let count = buffer.message.length
      const o = { data: JSON.parse(event.message), src: event.src, id: count++ }
      buffer.message = concat(buffer.message, o)
    },
    // default handler called for all methods
  },
  actions: {
    SOCKET_ONOPEN(context, event) {
      context.commit('INIT_SOCKET', event)

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
    SOCKET_ONMESSAGE(context, message) {
      if (message.socket === 'console') {
        context.commit('APPEND_MESSAGE', {
          socket: message.socket,
          message: message.data,
          src: 'socket',
        })
      } else if (message.socket === 'api2') {
        context.commit('APPEND_OBJECT', {
          socket: message.socket,
          message: message.data,
          src: 'socket',
        })
      }
    },
    async sendMessage(context, message) {
      context.commit('APPEND_MESSAGE', {
        socket: 'console',
        message,
        src: 'local',
      })
      Vue.prototype.$wsSend('console', message)
    },
  },
  getters: {
    messages: (state) => (socketName) =>
      state.socket[socketName] ? state.socket[socketName].message : null,
  },
})
