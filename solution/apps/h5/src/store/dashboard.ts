import { defineStore } from 'pinia';
import { ref } from 'vue';
import { DashboardSummaryVO } from '../api';

export interface PointHistoryState {
  timeline: string[];
  entryDelta: number[];
  exitDelta: number[];
  lastEntryCount: number;
  lastExitCount: number;
  lastSummary: DashboardSummaryVO | null;
}

const readValidStorageRate = (): number => {
  try {
    const raw = localStorage.getItem('flowsight_poll_rate');
    const val = Number(raw);
    if ([5, 10, 30].includes(val)) {
      return val;
    }
  } catch (e) {}
  return 5;
};

export const useDashboardStore = defineStore('dashboard', () => {
  // 实时座舱数据刷新频率 (秒)，仅支持 5, 10, 30，默认 5s，与 localStorage 双向绑定
  const pollRateSec = ref<number>(readValidStorageRate());

  // 滑动时间序列缓存 (按 pointId 隔离存储，防止切换通道或导航切页导致历史丢失)
  const historyMap = ref<Record<number, PointHistoryState>>({});

  const setPollRate = (sec: number) => {
    const validSec = [5, 10, 30].includes(Number(sec)) ? Number(sec) : 5;
    pollRateSec.value = validSec;
    try {
      localStorage.setItem('flowsight_poll_rate', String(validSec));
    } catch (e) {}
  };

  const syncFromStorage = () => {
    const latest = readValidStorageRate();
    if (pollRateSec.value !== latest) {
      pollRateSec.value = latest;
    }
  };

  /**
   * 获取指定点位的历史时间序列与汇总状态
   * 优先内存 -> 次选 sessionStorage -> 最后优雅生成初始时间基线
   */
  const getPointHistory = (pointId: number): PointHistoryState => {
    if (historyMap.value[pointId]) {
      return historyMap.value[pointId];
    }

    const storageKey = `flowsight_dash_hist_${pointId}`;
    try {
      const raw = sessionStorage.getItem(storageKey);
      if (raw) {
        const parsed = JSON.parse(raw);
        if (parsed && Array.isArray(parsed.timeline) && parsed.timeline.length > 0) {
          historyMap.value[pointId] = parsed;
          return historyMap.value[pointId];
        }
      }
    } catch (e) {}

    // 初始化时间基线 (按当前频率生成 8 个历史点)
    const now = new Date();
    const stepMs = Math.max(1, pollRateSec.value) * 1000;
    const initTimeline: string[] = [];
    const initEntry: number[] = [];
    const initExit: number[] = [];

    for (let i = 8; i >= 1; i--) {
      const t = new Date(now.getTime() - i * stepMs);
      const ts = [
        String(t.getHours()).padStart(2, '0'),
        String(t.getMinutes()).padStart(2, '0'),
        String(t.getSeconds()).padStart(2, '0')
      ].join(':');
      initTimeline.push(ts);
      initEntry.push(0);
      initExit.push(0);
    }

    const newState: PointHistoryState = {
      timeline: initTimeline,
      entryDelta: initEntry,
      exitDelta: initExit,
      lastEntryCount: -1,
      lastExitCount: -1,
      lastSummary: null
    };

    historyMap.value[pointId] = newState;
    return newState;
  };

  /**
   * 记录一次轮询时序数据步进
   */
  const recordTick = (
    pointId: number,
    timeStr: string,
    inDelta: number,
    outDelta: number,
    summary: DashboardSummaryVO
  ) => {
    const hist = getPointHistory(pointId);
    hist.lastEntryCount = summary.todayEntry;
    hist.lastExitCount = summary.todayExit;
    hist.lastSummary = summary;

    hist.timeline.push(timeStr);
    hist.entryDelta.push(inDelta);
    hist.exitDelta.push(outDelta);

    // 保留最近 16 个点，提供更饱满平滑的波形
    const maxPoints = 16;
    while (hist.timeline.length > maxPoints) {
      hist.timeline.shift();
      hist.entryDelta.shift();
      hist.exitDelta.shift();
    }

    try {
      sessionStorage.setItem(`flowsight_dash_hist_${pointId}`, JSON.stringify(hist));
    } catch (e) {}
  };

  const clearCache = () => {
    historyMap.value = {};
    try {
      Object.keys(sessionStorage).forEach((k) => {
        if (k.startsWith('flowsight_dash_hist_')) {
          sessionStorage.removeItem(k);
        }
      });
    } catch (e) {}
  };

  return {
    pollRateSec,
    historyMap,
    setPollRate,
    syncFromStorage,
    getPointHistory,
    recordTick,
    clearCache
  };
});
