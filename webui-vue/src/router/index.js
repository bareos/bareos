import { createRouter, createWebHashHistory } from 'vue-router'
import { useAuthStore } from '../stores/auth.js'
import { useSettingsStore } from '../stores/settings.js'
import { buildStoragesTabQuery } from '../utils/storagesRoute.js'

const routes = [
  {
    path: '/login',
    component: () => import('../layouts/AuthLayout.vue'),
    children: [
      { path: '', name: 'login', component: () => import('../pages/LoginPage.vue'), meta: { title: 'Login' } }
    ]
  },
  {
    path: '/',
    component: () => import('../layouts/MainLayout.vue'),
    meta: { requiresAuth: true },
    children: [
      { path: '', redirect: '/dashboard' },
      { path: 'dashboard', name: 'dashboard', component: () => import('../pages/DashboardPage.vue'), meta: { title: 'Dashboard' } },
      { path: 'jobs', name: 'jobs', component: () => import('../pages/JobsPage.vue'), meta: { title: 'Jobs' } },
      { path: 'jobs/:id', name: 'job-details', component: () => import('../pages/JobDetailsPage.vue'), meta: { title: 'Job Details' } },
      { path: 'restore', name: 'restore', component: () => import('../pages/RestorePage.vue'), meta: { title: 'Restore' } },
      { path: 'clients', name: 'clients', component: () => import('../pages/ClientsPage.vue'), meta: { title: 'Clients' } },
      { path: 'clients/:name', name: 'client-details', component: () => import('../pages/ClientDetailsPage.vue'), meta: { title: 'Client Details' } },
      { path: 'schedules', name: 'schedules', component: () => import('../pages/SchedulesPage.vue'), meta: { title: 'Schedules' } },
      { path: 'storages', name: 'storages', component: () => import('../pages/StoragesPage.vue'), meta: { title: 'Storages' } },
      {
        path: 'autochangers',
        name: 'autochangers',
        redirect: to => ({
          path: '/storages',
          query: buildStoragesTabQuery(to.query, 'autochangers'),
        }),
      },
      { path: 'storages/pools/:name', name: 'pool-details', component: () => import('../pages/PoolDetailsPage.vue'), meta: { title: 'Pool Details' } },
      { path: 'storages/volumes/:name', name: 'volume-details', component: () => import('../pages/VolumeDetailsPage.vue'), meta: { title: 'Volume Details' } },
      { path: 'director',  name: 'director',  component: () => import('../pages/DirectorPage.vue'), meta: { title: 'Director' } },
      { path: 'analytics', name: 'analytics', component: () => import('../pages/AnalyticsPage.vue'), meta: { title: 'Analytics' } },
      { path: 'filesets',  name: 'filesets',  component: () => import('../pages/FilesetsPage.vue'), meta: { title: 'Filesets' } },
      { path: 'acls',      name: 'acls',      component: () => import('../pages/AclPage.vue'), meta: { title: 'Command ACL' } },
      { path: 'settings',  name: 'settings',  component: () => import('../pages/SettingsPage.vue'), meta: { title: 'Settings' } },
    ]
  },
  {
    path: '/console-popup',
    component: () => import('../layouts/ConsolePopupLayout.vue'),
    meta: { requiresAuth: true, title: 'Console' },
    children: [
      { path: '', name: 'console-popup', component: () => import('../pages/ConsolePage.vue'), meta: { title: 'Console' } }
    ]
  },
  { path: '/:pathMatch(.*)*', redirect: '/dashboard' }
]

const router = createRouter({
  history: createWebHashHistory(),
  routes
})

router.beforeEach(async (to) => {
  const auth = useAuthStore()
  const settings = useSettingsStore()
  await auth.restoreSession(false, settings.directorName)
  if (to.meta.requiresAuth && !auth.isLoggedIn) {
    return { name: 'login' }
  }
  if (to.name === 'login' && auth.isLoggedIn && to.query.mode !== 'add') {
    return { name: 'dashboard' }
  }
})

router.afterEach((to) => {
  const pageTitle = to.meta?.title
  document.title = pageTitle ? `${pageTitle} - Bareos` : 'Bareos'
})

export default router
