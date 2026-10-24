import axios from 'axios';
import { showToast } from 'vant';

const api = axios.create({
  baseURL: '/api/v1',
  timeout: 10000
});

// 请求拦截器：注入 JWT
api.interceptors.request.use((config) => {
  const token = localStorage.getItem('flowsight_token');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
}, (error) => {
  return Promise.reject(error);
});

// 响应拦截器：统一处理 401 和错误
api.interceptors.response.use((response) => {
  const res = response.data;
  if (res.code === 200) {
    return res.data;
  }
  showToast(res.message || '操作失败');
  return Promise.reject(new Error(res.message || 'Error'));
}, (error) => {
  if (error.response && error.response.status === 401) {
    localStorage.removeItem('flowsight_token');
    showToast('登录已过期，请重新登录');
    window.location.hash = '#/login';
  } else {
    showToast(error.message || '网络连接异常');
  }
  return Promise.reject(error);
});

export default api;

export interface LoginParams {
  phone: string;
  password: string;
}

export const authApi = {
  login: (data: LoginParams) => api.post('/auth/login', { ...data, clientType: 'H5' })
};

export const h5Api = {
  getMyPoints: () => api.get('/h5/points'),
  getDashboardSummary: (pointId: number) => api.get('/h5/dashboard/summary', { params: { pointId } }),
  getTrends: (pointId: number, dimension = 'hour', days = 7) => api.get('/h5/dashboard/trends', { params: { pointId, dimension, days } }),
  getEvents: (pointId: number, page = 1, size = 20) => api.get('/h5/events', { params: { pointId, page, size } })
};
