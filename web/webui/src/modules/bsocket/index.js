// const uuidv4 = require('uuid/v4')

export default {
  install(Vue, opts) {
    const that = this
    this.opts = opts

    if (!!opts && !!opts.socket) {
      this.sockets = new Map()
      for (const socketName in opts.socket) {
        if (typeof opts.socket[socketName] === 'string') {
          this.connect(socketName)
        }
      }
      this.store = opts.store
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

  connect(socketName) {
    let socket = this.sockets[socketName]
    const address = this.opts.socket[socketName]
    if (socket) {
      console.log('connect existing socket. closing it')
      socket.close()
    } else {
      socket = new WebSocket(address)
      this.registerEvents(socket, socketName)
      this.sockets.set(socketName, socket)
    }
  },

  registerEvents(socket, key) {
    ['onmessage', 'onclose', 'onerror', 'onopen'].forEach((eventType) => {
      socket[eventType] = (event) => {
        if (event.type === 'error') {
          event.currentTarget.close()
        } else if (event.type === 'close') {
          setTimeout(() => {
            this.connect(key)
          }, 1000)
        }
        this.passToStore(key, eventType, event.data)
      }
    })
  },

  passToStore(socket, eventType, data) {
    const target = (this.eventPrefix + eventType).toUpperCase()
    if (this.store) {
      this.store.dispatch(target, { socket, eventType, data })
    }
  },
}
