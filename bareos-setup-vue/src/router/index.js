import { createRouter, createWebHashHistory } from 'vue-router'
import WizardPage from '../pages/WizardPage.vue'

export default createRouter({
  history: createWebHashHistory(),
  routes: [{ path: '/', component: WizardPage }]
})
