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

export interface EventQueryParams {
  pointId?: number | null;
  startDate?: string;
  endDate?: string;
  direction?: string;
  page?: number;
  size?: number;
}

export interface DashboardSummaryVO {
  pointId: number;
  pointName: string;
  bizType?: string;
  clientName: string;
  appVersion: string;
  lastHeartbeat: string;
  streamStatus: string;
  currentOccupancy: number;
  todayEntry: number;
  todayExit: number;
  todayTotal: number;
  snapshotUrl?: string;
  demographics: {
    maleCount: number;
    femaleCount: number;
    maleRatio: number;
    femaleRatio: number;
    topAgeGroup: string;
  };
  hourlyChart: any[];
}

export interface EventItemVO {
  id: number;
  pointId: number;
  pointName: string;
  trackId: number;
  direction: string;
  confidence: number;
  gender: string;
  ageGroup: string;
  plateNumber?: string;
  plateColor?: string;
  vehicleType?: string;
  vehicleColor?: string;
  timestampMs: number;
  datetime: string;
  playUrl?: string;
}

export interface PairingCodeVO {
  pointId: number;
  pointName: string;
  pairingCode: string;
  expiresInSeconds: number;
  expireAtMs: number;
}

export interface GlobalMetricsVO {
  totalPoints: number;
  onlinePoints: number;
  offlinePoints: number;
  pedestrianPoints: number;
  vehiclePoints: number;
  totalPedestrianOccupancy: number;
  totalPedestrianEntry: number;
  totalPedestrianExit: number;
  totalVehicleOccupancy: number;
  totalVehicleEntry: number;
  totalVehicleExit: number;
}

export interface DashboardPointCardVO {
  pointId: number;
  pointName: string;
  pointCode: string;
  bizType: string;
  bizTypeName: string;
  streamStatus: string;
  currentOccupancy: number;
  todayEntry: number;
  todayExit: number;
  todayTotal: number;
  snapshotUrl?: string;
  isDefault: boolean;
  lastHeartbeat?: string;
  clientName?: string;
}

export interface DashboardOverviewVO {
  defaultPointId: number | null;
  globalMetrics: GlobalMetricsVO;
  pointCards: DashboardPointCardVO[];
}

export const h5Api = {
  getMyPoints: () => api.get('/h5/points'),
  getDashboardOverview: () => api.get<any, DashboardOverviewVO>('/h5/dashboard/overview'),
  setDefaultPoint: (pointId: number | null) => api.post('/h5/user/default-point', { pointId }),
  getDashboardSummary: (pointId: number) => api.get('/h5/dashboard/summary', { params: { pointId } }),
  getTrends: (pointId: number, dimension = 'hour', days = 7) => api.get('/h5/dashboard/trends', { params: { pointId, dimension, days } }),
  getEvents: (paramsOrPointId: number | EventQueryParams, page = 1, size = 10) => {
    if (typeof paramsOrPointId === 'object') {
      return api.get('/h5/events', { params: paramsOrPointId });
    }
    return api.get('/h5/events', { params: { pointId: paramsOrPointId, page, size } });
  },
  changePassword: (data: { oldPassword: string; newPassword: string }) => api.post('/h5/user/password', data),
  getPointCredential: (pointId: number) => api.get(`/h5/points/${pointId}/credential`),
  resetDevicePassword: (pointId: number, data: { newDevicePassword: string }) => api.post(`/h5/points/${pointId}/device-password`, data),
  generatePairingCode: (pointId: number) => api.post(`/h5/points/${pointId}/pairing-code`)
};
