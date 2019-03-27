import Vue from 'vue'
import Vuetify from 'vuetify'
import App from './App.vue'
import router from './router'
import store from './store'
import VueNativeSock from 'vue-native-websocket'

import 'vuetify/dist/vuetify.min.css' // Ensure are using css-loader
import 'xterm/dist/xterm.css'

Vue.use(Vuetify)

Vue.use(VueNativeSock, process.env.VUE_APP_SOCKET, {
  store: store,
  connectManually: false,
})

Vue.config.productionTip = false

// Subscribe to store updates
store.subscribe((mutation, state) => {
  // Store the state object as a JSON string
  localStorage.setItem('store', JSON.stringify(state))
})

new Vue({
  router,
  store,
  render: (h) => h(App),
}).$mount('#app')
