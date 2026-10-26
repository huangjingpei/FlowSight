import { defineStore } from 'pinia';
import { ref } from 'vue';

export interface PointItem {
  id: number;
  pointName: string;
  pointCode: string;
  streamStatus: string;
  bizType?: string;
}

export const usePointStore = defineStore('point', () => {
  const points = ref<PointItem[]>([]);
  const activePointId = ref<number | null>(null);
  
  // 核心状态机互斥锁: 当正在播放视频监控切片时，锁定为 true，禁止切换点位
  const isMediaPlaying = ref<boolean>(false);

  const setPoints = (list: PointItem[]) => {
    points.value = list;
    if (list.length > 0 && activePointId.value === null) {
      activePointId.value = list[0].id;
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

  const setMediaPlaying = (playing: boolean) => {
    isMediaPlaying.value = playing;
  };

  return {
    points,
    activePointId,
    isMediaPlaying,
    setPoints,
    setActivePoint,
    setMediaPlaying
  };
});
