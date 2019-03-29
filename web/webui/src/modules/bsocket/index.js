// const uuidv4 = require('uuid/v4')

export default {
  install(Vue, opts) {
    const that = this

    if (!!opts && !!opts.socket) {
      this.sockets = new Map()
      for (const socketName in opts.socket) {
        if (typeof opts.socket[socketName] === 'string') {
          this.sockets.set(socketName, new WebSocket(opts.socket[socketName]))
        }
      }

      this.store = opts.store
      this.sockets.forEach((socket, key) => this.registerEvents(socket, key))
    }

    this.eventPrefix = 'SOCKET_'

    Vue.prototype.$wsSend = (socketName, data) => {
      const socket = that.sockets.get(socketName)
      if (!socket) {
        return null
      }
      socket.send(data)
    }
  },

  registerEvents(socket, key) {
    ['onmessage', 'onclose', 'onerror', 'onopen'].forEach((eventType) => {
      socket[eventType] = (event) => {
        this.passToStore(key, eventType, event.data)
      }
    })
  },


  passToStore(socket, eventType, data) {
    const target = (this.eventPrefix + eventType).toUpperCase()
    if (this.store) {
      this.store.commit(target, { socket, eventType, data })
    }
  },
}
