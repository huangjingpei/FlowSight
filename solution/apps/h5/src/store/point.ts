import { defineStore } from 'pinia';
import { ref } from 'vue';

export interface PointItem {
  id: number;
  pointName: string;
  pointCode: string;
  streamStatus: string;
  bizType?: string;
  bizTypeName?: string;
}

export const usePointStore = defineStore('point', () => {
  const points = ref<PointItem[]>([]);
  const activePointId = ref<number | null>(null);

  // 本地优先读取默认座舱偏好缓存
  const savedDefault = localStorage.getItem('flowsight_default_point_id');
  const defaultPointId = ref<number | null>(savedDefault ? Number(savedDefault) : null);
  
  // 核心状态机互斥锁: 当正在播放视频监控切片时，锁定为 true，禁止切换点位
  const isMediaPlaying = ref<boolean>(false);

  const setPoints = (list: PointItem[]) => {
    points.value = list;
    if (list.length > 0) {
      if (activePointId.value === null || !list.some(p => p.id === activePointId.value)) {
        // 优先加载用户设置的默认业务座舱
        if (defaultPointId.value && list.some(p => p.id === defaultPointId.value)) {
          activePointId.value = defaultPointId.value;
        } else {
          activePointId.value = list[0].id;
        }
      }
    } else {
      activePointId.value = null;
    }
  };

  const setActivePoint = (id: number) => {
    if (isMediaPlaying.value) {
      console.warn('视频播放中，禁止切换点位');
      return false;
    }
    activePointId.value = id;
    return true;
  };

  const setDefaultPoint = (id: number | null) => {
    defaultPointId.value = id;
    if (id !== null && id > 0) {
      localStorage.setItem('flowsight_default_point_id', String(id));
    } else {
      localStorage.removeItem('flowsight_default_point_id');
    }
  };

  const setMediaPlaying = (playing: boolean) => {
    isMediaPlaying.value = playing;
  };

  return {
    points,
    activePointId,
    defaultPointId,
    isMediaPlaying,
    setPoints,
    setActivePoint,
    setDefaultPoint,
    setMediaPlaying
  };
});
