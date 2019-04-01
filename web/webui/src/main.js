import Vue from 'vue'
import Vuetify from 'vuetify'
import App from './App.vue'
import router from './router'
import store from './store'

import 'vuetify/dist/vuetify.min.css' // Ensure are using css-loader

import BSocket from './modules/bsocket'

Vue.use(Vuetify)
Vue.use(BSocket, {
  socket: {
    console: process.env.VUE_APP_CONSOLE_SOCKET,
    api2: process.env.VUE_APP_API2_SOCKET,
  },
  store: store,
})

Vue.config.productionTip = false

// Subscribe to store updates
// store.subscribe((mutation, state) => {
//   // Store the state object as a JSON string
//   localStorage.setItem('store', JSON.stringify(state))
// })

new Vue({
  router,
  store,
  render: (h) => h(App),
}).$mount('#app')
