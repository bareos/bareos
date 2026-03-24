import { createApp } from 'vue'
import { Quasar, Notify, Dialog } from 'quasar'
import { createPinia } from 'pinia'
import '@quasar/extras/material-icons/material-icons.css'
import 'quasar/src/css/index.sass'
import './css/app.scss'
import App from './App.vue'
import router from './router/index.js'

const app = createApp(App)
app.use(Quasar, {
  plugins: { Notify, Dialog },
  config: {
    brand: {
      primary: '#0075be',
      secondary: '#5a6773',
      accent: '#f5a623',
      positive: '#28a745',
      negative: '#dc3545',
      info: '#17a2b8',
      warning: '#ffc107',
    }
  }
})
app.use(createPinia())
app.use(router)
app.mount('#app')
