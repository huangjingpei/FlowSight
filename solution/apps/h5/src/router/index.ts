import { createRouter, createWebHashHistory } from 'vue-router';
import Login from '../views/Login.vue';
import Dashboard from '../views/Dashboard.vue';
import Trends from '../views/Trends.vue';
import Playback from '../views/Playback.vue';
import Settings from '../views/Settings.vue';
import DigitalScreen from '../views/DigitalScreen.vue';

const routes = [
  { path: '/', redirect: '/dashboard' },
  { path: '/login', component: Login },
  {
    path: '/screen',
    component: DigitalScreen,
    meta: { title: 'FlowSight 智能视觉数字全景大屏' }
  },
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
  // 支持投屏免密 Token 注入
  if (to.query && to.query.token) {
    localStorage.setItem('flowsight_token', to.query.token as string);
    localStorage.setItem('flowsight_admin_token', to.query.token as string);
  }
  const token = localStorage.getItem('flowsight_token');
  if (to.path !== '/login' && !token) {
    next('/login');
  } else {
    next();
  }
});

export default router;
