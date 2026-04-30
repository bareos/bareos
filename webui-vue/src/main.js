import { createApp } from 'vue'
import { Quasar, Notify, Dialog, Screen } from 'quasar'
import { createPinia } from 'pinia'
import '@quasar/extras/material-icons/material-icons.css'
import '@quasar/extras/mdi-v7/mdi-v7.css'
import 'quasar/src/css/index.sass'
import './css/app.scss'
import App from './App.vue'
import router from './router/index.js'
import { i18n } from './i18n/index.js'
import { useSettingsStore } from './stores/settings.js'

const app = createApp(App)
const pinia = createPinia()

app.use(Quasar, {
  plugins: { Notify, Dialog, Screen },
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
app.use(i18n)
app.use(pinia)
useSettingsStore(pinia)
app.use(router)
app.mount('#app')
