import Vue from 'vue'
import Router from 'vue-router'
import Dashboard from './views/Dashboard.vue'
import BackupHistory from './views/BackupHistory.vue'
import Clients from './views/Clients.vue'
import Jobs from './views/Jobs.vue'

Vue.use(Router)

export default new Router({
  mode: 'history',
  base: process.env.BASE_URL,
  routes: [
    {
      path: '/',
      name: 'Dashboard',
      component: Dashboard
    },
    {
      path: '/backup/history',
      name: 'BackupHistory',
      component: BackupHistory
    },
    {
      path: '/backup/clients',
      name: 'Clients',
      component: Clients
    },
    {
      path: '/backup/jobs',
      name: 'Jobs',
      component: Jobs
    },
    {
      path: '/bconsole',
      name: 'bconsole',
      // route level code-splitting
      // this generates a separate chunk (about.[hash].js) for this route
      // which is lazy-loaded when the route is visited.
      component: () => import(/* webpackChunkName: "about" */ './views/BConsole.vue')
    },
    {
      path: '/console',
      name: 'console',
      // route level code-splitting
      // this generates a separate chunk (about.[hash].js) for this route
      // which is lazy-loaded when the route is visited.
      component: () => import(/* webpackChunkName: "about" */ './views/Console.vue')
    },
    {
      path: '/bconsole/:id',
      name: 'dedicated-bconsole',
      // route level code-splitting
      // this generates a separate chunk (about.[hash].js) for this route
      // which is lazy-loaded when the route is visited.
      component: () => import(/* webpackChunkName: "about" */ './views/BConsole.vue')
    }
  ]
})
