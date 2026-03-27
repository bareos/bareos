import { createRouter, createWebHashHistory } from 'vue-router'

const routes = [
  {
    path: '/',
    component: () => import('../layouts/WizardLayout.vue'),
    children: [
      { path: '',          redirect: '/welcome' },
      { path: 'welcome',   name: 'welcome',    component: () => import('../pages/WelcomePage.vue') },
      { path: 'os',        name: 'os',         component: () => import('../pages/OsDetectionPage.vue') },
      { path: 'components',name: 'components', component: () => import('../pages/ComponentsPage.vue') },
      { path: 'repo',      name: 'repo',       component: () => import('../pages/RepoSetupPage.vue') },
      { path: 'install',   name: 'install',    component: () => import('../pages/InstallPage.vue') },
      { path: 'database',  name: 'database',   component: () => import('../pages/DbConfigPage.vue') },
      { path: 'passwords', name: 'passwords',  component: () => import('../pages/PasswordPage.vue') },
      { path: 'summary',   name: 'summary',    component: () => import('../pages/SummaryPage.vue') },
    ]
  },
  { path: '/:pathMatch(.*)*', redirect: '/welcome' }
]

export default createRouter({
  history: createWebHashHistory(),
  routes,
})
