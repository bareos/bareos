import { createRouter, createWebHashHistory } from 'vue-router'
import { useAuthStore } from '../stores/auth.js'

const routes = [
  {
    path: '/login',
    component: () => import('../layouts/AuthLayout.vue'),
    children: [
      { path: '', name: 'login', component: () => import('../pages/LoginPage.vue') }
    ]
  },
  {
    path: '/',
    component: () => import('../layouts/MainLayout.vue'),
    meta: { requiresAuth: true },
    children: [
      { path: '', redirect: '/dashboard' },
      { path: 'dashboard', name: 'dashboard', component: () => import('../pages/DashboardPage.vue') },
      { path: 'jobs', name: 'jobs', component: () => import('../pages/JobsPage.vue') },
      { path: 'jobs/:id', name: 'job-details', component: () => import('../pages/JobDetailsPage.vue') },
      { path: 'restore', name: 'restore', component: () => import('../pages/RestorePage.vue') },
      { path: 'clients', name: 'clients', component: () => import('../pages/ClientsPage.vue') },
      { path: 'clients/:name', name: 'client-details', component: () => import('../pages/ClientDetailsPage.vue') },
      { path: 'schedules', name: 'schedules', component: () => import('../pages/SchedulesPage.vue') },
      { path: 'storages', name: 'storages', component: () => import('../pages/StoragesPage.vue') },
      { path: 'autochangers', name: 'autochangers', redirect: { path: '/storages', query: { tab: 'autochangers' } } },
      { path: 'storages/pools/:name', name: 'pool-details', component: () => import('../pages/PoolDetailsPage.vue') },
      { path: 'storages/volumes/:name', name: 'volume-details', component: () => import('../pages/VolumeDetailsPage.vue') },
      { path: 'director',  name: 'director',  component: () => import('../pages/DirectorPage.vue') },
      { path: 'analytics', name: 'analytics', component: () => import('../pages/AnalyticsPage.vue') },
      { path: 'filesets',  name: 'filesets',  component: () => import('../pages/FilesetsPage.vue') },
      { path: 'acls',      name: 'acls',      component: () => import('../pages/AclPage.vue') },
      { path: 'settings',  name: 'settings',  component: () => import('../pages/SettingsPage.vue') },
    ]
  },
  {
    path: '/console-popup',
    component: () => import('../layouts/ConsolePopupLayout.vue'),
    meta: { requiresAuth: true },
    children: [
      { path: '', name: 'console-popup', component: () => import('../pages/ConsolePage.vue') }
    ]
  },
  { path: '/:pathMatch(.*)*', redirect: '/dashboard' }
]

const router = createRouter({
  history: createWebHashHistory(),
  routes
})

router.beforeEach((to) => {
  const auth = useAuthStore()
  if (to.meta.requiresAuth && !auth.isLoggedIn) {
    return { name: 'login' }
  }
  if (to.name === 'login' && auth.isLoggedIn) {
    return { name: 'dashboard' }
  }
})

export default router
