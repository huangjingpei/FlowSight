import { createRouter, createWebHashHistory } from 'vue-router';
import Login from '../views/Login.vue';
import Dashboard from '../views/Dashboard.vue';
import Trends from '../views/Trends.vue';
import Playback from '../views/Playback.vue';
import Settings from '../views/Settings.vue';
import DigitalScreen from '../views/DigitalScreen.vue';
import { useSystemStore } from '../store/system';

const routes = [
  { path: '/', redirect: '/dashboard' },
  { path: '/login', component: Login },
  {
    path: '/screen',
    component: DigitalScreen,
    meta: { title: '智能视觉数字全景大屏' }
  },
  {
    path: '/screen/:userId',
    component: DigitalScreen,
    meta: { title: '智能视觉数字全景大屏' }
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

router.beforeEach(async (to, from, next) => {
  // 支持投屏免密 Token 注入
  if (to.query && to.query.token) {
    localStorage.setItem('flowsight_token', to.query.token as string);
    localStorage.setItem('flowsight_admin_token', to.query.token as string);
  }
  const token = localStorage.getItem('flowsight_token');
  if (to.path !== '/login' && !token) {
    next('/login');
    return;
  }

  // 大屏访问规范控制：大屏必须在某个用户下面，URL 必须关联到用户 (禁止无归属的裸 /screen URL)
  if (to.path === '/screen') {
    const hasUser = !!(to.query && to.query.userId) || !!(to.params && to.params.userId);
    if (!hasUser) {
      // 检查当前登录商户账号，尝试自动矫正并重定向至带商户归属的合法大屏 URL
      const userStr = localStorage.getItem('flowsight_user');
      let merchantUserId = 0;
      if (userStr) {
        try {
          const u = JSON.parse(userStr);
          if (u.role !== 'ROLE_ADMIN' && u.userId) {
            merchantUserId = u.userId;
          }
        } catch (e) {}
      }
      if (merchantUserId > 0) {
        next({ path: '/screen', query: { ...to.query, userId: merchantUserId } });
        return;
      } else {
        // 无有效商户关联，彻底拦截裸 /screen 访问
        next('/dashboard');
        return;
      }
    }
  }

  // 关键控制：若视频播放/切片功能未开启，拦截 /playback 页面直接重定向至 /dashboard
  if (to.path === '/playback') {
    const systemStore = useSystemStore();
    if (!systemStore.loaded) {
      await systemStore.fetchFeatures();
    }
    if (!systemStore.videoPlaybackEnabled) {
      next('/dashboard');
      return;
    }
  }

  next();
});

export default router;
