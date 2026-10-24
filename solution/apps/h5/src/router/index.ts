import { createRouter, createWebHashHistory } from 'vue-router';
import Login from '../views/Login.vue';
import Dashboard from '../views/Dashboard.vue';
import Trends from '../views/Trends.vue';
import Playback from '../views/Playback.vue';
import Settings from '../views/Settings.vue';

const routes = [
  { path: '/', redirect: '/dashboard' },
  { path: '/login', component: Login },
  { path: '/dashboard', component: Dashboard },
  { path: '/trends', component: Trends },
  { path: '/playback', component: Playback },
  { path: '/settings', component: Settings },
  { path: '/insights', redirect: '/settings' }
];

const router = createRouter({
  history: createWebHashHistory(),
  routes
});

router.beforeEach((to, from, next) => {
  const token = localStorage.getItem('flowsight_token');
  if (to.path !== '/login' && !token) {
    next('/login');
  } else {
    next();
  }
});

export default router;
