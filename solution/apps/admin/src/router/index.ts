import { createRouter, createWebHashHistory } from 'vue-router';
import Login from '../views/Login.vue';
import Layout from '../views/Layout.vue';
import Overview from '../views/Overview.vue';
import UserManage from '../views/UserManage.vue';
import PointManage from '../views/PointManage.vue';

const routes = [
  { path: '/login', component: Login },
  {
    path: '/',
    component: Layout,
    redirect: '/overview',
    children: [
      { path: 'overview', component: Overview, meta: { title: '运营总览' } },
      { path: 'users', component: UserManage, meta: { title: '用户管理' } },
      { path: 'points', component: PointManage, meta: { title: '点位管理' } }
    ]
  }
];

const router = createRouter({
  history: createWebHashHistory(),
  routes
});

router.beforeEach((to, from, next) => {
  const token = localStorage.getItem('flowsight_admin_token');
  if (to.path !== '/login' && !token) {
    next('/login');
  } else {
    next();
  }
});

export default router;
