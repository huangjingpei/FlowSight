import axios from 'axios';
import { ElMessage } from 'element-plus';

const api = axios.create({
  baseURL: '/api/v1',
  timeout: 10000
});

api.interceptors.request.use((config) => {
  const token = localStorage.getItem('flowsight_admin_token');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

api.interceptors.response.use((response) => {
  const res = response.data;
  if (res.code === 200) {
    return res.data;
  }
  ElMessage.error(res.message || '操作失败');
  return Promise.reject(new Error(res.message || 'Error'));
}, (error) => {
  if (error.response && error.response.status === 401) {
    localStorage.removeItem('flowsight_admin_token');
    ElMessage.error('登录失效，请重新登录');
    window.location.hash = '#/login';
  } else {
    ElMessage.error(error.message || '网络连接失败');
  }
  return Promise.reject(error);
});

export default api;

export const adminApi = {
  login: (data: any) => api.post('/auth/login', { ...data, clientType: 'ADMIN' }),
  getUsers: (page = 1, size = 10, keyword = '') => api.get('/admin/users', { params: { page, size, keyword } }),
  createUser: (data: any) => api.post('/admin/users', data),
  updateUser: (id: number, data: any) => api.put(`/admin/users/${id}`, data),
  deleteUser: (id: number) => api.delete(`/admin/users/${id}`),

  getPoints: (page = 1, size = 10, userId?: number, keyword = '', bizType = '') => api.get('/points', { params: { page, size, userId, keyword, bizType } }),
  createPoint: (data: any) => api.post('/points', data),
  updatePoint: (id: number, data: any) => api.put(`/points/${id}`, data),
  deletePoint: (id: number) => api.delete(`/points/${id}`),
  refreshToken: (id: number) => api.post(`/points/${id}/token/refresh`),
  generatePointCode: (bizType = 'PEDESTRIAN') => api.get('/points/generate-code', { params: { bizType } })
};
