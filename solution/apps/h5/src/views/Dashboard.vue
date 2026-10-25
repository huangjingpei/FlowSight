<template>
  <div class="dashboard-viewport">
    <!-- 1. 顶部嵌入式标题栏 -->
    <header class="flat-header">
      <div class="header-left">
        <span class="header-dot"></span>
        <span class="company-name">{{ currentCompanyName }}</span>
      </div>
      <div class="header-right">
        <span class="live-status-pill" :class="summary.streamStatus === 'ONLINE' ? 'online' : 'offline'">
          {{ summary.streamStatus === 'ONLINE' ? '● 边缘在线' : '○ 边缘离线' }}
        </span>
      </div>
    </header>

    <!-- 2. 主体：单屏纯平实时监控 (无圈套、5秒近实时动态曲线、数据极度聚焦) -->
    <main class="flat-main">
      <!-- 行 1: 通道点位选择条 -->
      <div class="flat-point-bar">
        <span class="point-label">监控通道:</span>
        <div class="point-select-wrap">
          <select v-model="activePointId" class="flat-select" @change="onPointChange">
            <option v-for="p in pointOptions" :key="p.value" :value="p.value">
              {{ p.text }}
            </option>
          </select>
        </div>
        <span
          class="poll-badge poll-badge-btn"
          @click="showRateModal = true"
          title="点击切换座舱数据刷新频率"
        >
          {{ pollRateSec }}s实时同步 ▾
        </span>
      </div>

      <!-- 行 2: 四大核心客流指标矩阵 (扁平化高反差色块) -->
      <div class="kpi-matrix">
        <!-- 1. 瞬时滞留 -->
        <div class="kpi-block border-cyan">
          <div class="kpi-top">
            <span class="kpi-title">瞬时在场</span>
            <span class="kpi-badge badge-cyan">实时</span>
          </div>
          <div class="kpi-num text-cyan">{{ summary.currentOccupancy }}<span class="kpi-unit">人</span></div>
          <div class="kpi-footer">当前通道内滞留感知</div>
        </div>

        <!-- 2. 今日进入 -->
        <div class="kpi-block border-green">
          <div class="kpi-top">
            <span class="kpi-title">累计进入 (In)</span>
            <span class="kpi-badge badge-green">进场</span>
          </div>
          <div class="kpi-num text-green">{{ summary.todayEntry }}<span class="kpi-unit">人次</span></div>
          <div class="kpi-footer">今日累计进门总人次</div>
        </div>

        <!-- 3. 今日离开 -->
        <div class="kpi-block border-orange">
          <div class="kpi-top">
            <span class="kpi-title">累计离开 (Out)</span>
            <span class="kpi-badge badge-orange">出场</span>
          </div>
          <div class="kpi-num text-orange">{{ summary.todayExit }}<span class="kpi-unit">人次</span></div>
          <div class="kpi-footer">今日累计出门总人次</div>
        </div>

        <!-- 4. 双向总流 -->
        <div class="kpi-block border-purple">
          <div class="kpi-top">
            <span class="kpi-title">双向总客流</span>
            <span class="kpi-badge badge-purple">通量</span>
          </div>
          <div class="kpi-num text-purple">{{ summary.todayTotal }}<span class="kpi-unit">人次</span></div>
          <div class="kpi-footer">双向累计综合总人次</div>
        </div>
      </div>

      <!-- 行 3: 安全状态指示条 -->
      <div class="flat-status-bar" :class="isSurged ? 'status-surged' : (summary.currentOccupancy > 20 ? 'status-warn' : 'status-ok')">
        <span class="status-icon">{{ isSurged ? '🚨' : (summary.currentOccupancy > 20 ? '⚠️' : '✓') }}</span>
        <span class="status-msg">
          {{ isSurged ? `客流超限警戒：瞬时在场达到 ${summary.currentOccupancy} 人 (警戒线: 25人)，流速曲线已切换应急态！` : (summary.currentOccupancy > 20 ? '当前人流密度偏高，已触发人流疏导指引' : '通道通行状态感知与安全基线正常') }}
        </span>
      </div>

      <!-- 行 3.5: 现场实时画面快照感知条 (60秒自动回传) -->
      <div v-if="summary.snapshotUrl" class="flat-snapshot-strip" @click="showSnapshotModal = true">
        <div class="snap-thumb-box">
          <img :src="summary.snapshotUrl" alt="现场快照" class="snap-thumb-img" />
          <span class="snap-live-tag">LIVE</span>
        </div>
        <div class="snap-info">
          <div class="snap-info-top">
            <span class="snap-info-title">现场通道实时画面快照</span>
            <span class="snap-roi-pill">ROI防区已标定</span>
          </div>
          <span class="snap-info-sub">边端自动上报现场视线 · 点击放大查看全景</span>
        </div>
        <span class="snap-zoom-icon">🔍</span>
      </div>

      <!-- 行 4: 实时动态流速折线图 (5秒近实时刷新动态绘制，自适应填满剩余空间) -->
      <div class="realtime-wave-card">
        <div class="wave-header">
          <div class="wave-title-box">
            <span class="wave-dot" :class="{ 'wave-dot-flame': isSurged }"></span>
            <span class="wave-title">通道实时流速脉冲 ({{ pollRateSec }}秒步进动态曲线)</span>
          </div>
          <div class="wave-legend">
            <span class="w-leg-item">
              <i class="w-dot" :class="isSurged ? 'dot-flame' : 'cyan'"></i>
              <span :class="{ 'flame-text': isSurged }">{{ isSurged ? '进入流速 (超限警戒)' : '进入流速' }}</span>
            </span>
            <span class="w-leg-item"><i class="w-dot pink"></i>离开流速</span>
          </div>
        </div>
        <div ref="pulseChartRef" class="pulse-chart-container"></div>
      </div>
    </main>

    <!-- 现场全景快照弹窗 -->
    <div v-if="showSnapshotModal" class="snapshot-modal-overlay" @click.self="showSnapshotModal = false">
      <div class="snapshot-modal-card">
        <div class="smc-header">
          <div class="smc-title-box">
            <span class="live-dot-red"></span>
            <span class="smc-title">{{ summary.pointName || '通道' }} - 现场实时画面快照</span>
          </div>
          <button class="smc-close" @click="showSnapshotModal = false">✕</button>
        </div>
        <div class="smc-body">
          <img :src="summary.snapshotUrl" alt="现场快照大图" class="modal-large-img" />
        </div>
        <div class="smc-footer">
          <span class="smc-tip">● 绊线检测区与防区标定由边端 AI 动态合成并加密同步</span>
        </div>
      </div>
    </div>

    <!-- 实时刷新频率快速切换弹窗 (与系统设置双向实时联动) -->
    <div v-if="showRateModal" class="rate-modal-overlay" @click.self="showRateModal = false">
      <div class="rate-modal-card">
        <div class="rm-header">
          <div class="rm-title-box">
            <span class="rm-icon">⏱️</span>
            <span class="rm-title">实时座舱数据刷新频率</span>
          </div>
          <button class="rm-close" @click="showRateModal = false">✕</button>
        </div>
        <div class="rm-body">
          <p class="rm-desc">
            切换实时监控流速脉冲波形与在场人数的步进采集周期：
          </p>
          <div class="rate-select-list">
            <div
              v-for="r in refreshRates"
              :key="r.sec"
              :class="['rate-card', pollRateSec === r.sec ? 'rate-card-active' : '']"
              @click="handleSelectRate(r.sec)"
            >
              <div class="rc-left">
                <span class="rc-radio">{{ pollRateSec === r.sec ? '●' : '○' }}</span>
                <div class="rc-text">
                  <span class="rc-title">{{ r.label }}</span>
                  <span class="rc-desc">{{ r.desc }}</span>
                </div>
              </div>
              <span v-if="pollRateSec === r.sec" class="rc-tag">当前生效</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, onActivated, onDeactivated, nextTick, watch } from 'vue';
import { storeToRefs } from 'pinia';
import { showToast } from 'vant';
import * as echarts from 'echarts';
import { h5Api, DashboardSummaryVO } from '../api';
import { usePointStore } from '../store/point';
import { useDashboardStore } from '../store/dashboard';
import { useThemeStore } from '../store/theme';

const pointStore = usePointStore();
const dashboardStore = useDashboardStore();
const themeStore = useThemeStore();
const activePointId = ref<number>(0);

const { pollRateSec } = storeToRefs(dashboardStore);

// 快速切换刷新频率选项卡片
const showRateModal = ref(false);
const refreshRates = [
  { sec: 5, label: '5 秒 (近实时推荐)', desc: '动态流速波形每 5 秒刷新，感知极度灵敏' },
  { sec: 10, label: '10 秒 (标准模式)', desc: '标准中台步进周期，平衡设备与数据通量' },
  { sec: 30, label: '30 秒 (省流模式)', desc: '低频后台巡检，适合长期待机与节约流量' }
];

const handleSelectRate = (sec: number) => {
  dashboardStore.setPollRate(sec);
  showRateModal.value = false;
  showToast({ message: `座舱刷新已设为 ${sec} 秒`, position: 'top', duration: 1000 });
};

const onStorageChange = (e: StorageEvent) => {
  if (e.key === 'flowsight_poll_rate') {
    dashboardStore.syncFromStorage();
  }
};

const currentCompanyName = computed(() => {
  try {
    const raw = localStorage.getItem('flowsight_user');
    if (raw) {
      const u = JSON.parse(raw);
      return u.companyName || u.nickname || 'FlowSight 实时监控';
    }
  } catch (e) {}
  return 'FlowSight 实时监控';
});

const summary = ref<DashboardSummaryVO>({
  pointId: 0,
  pointName: '',
  clientName: '',
  appVersion: 'v1.2.0',
  lastHeartbeat: '',
  streamStatus: 'ONLINE',
  currentOccupancy: 0,
  todayEntry: 0,
  todayExit: 0,
  todayTotal: 0,
  snapshotUrl: '',
  demographics: {
    maleCount: 0,
    femaleCount: 0,
    maleRatio: 0.5,
    femaleRatio: 0.5,
    topAgeGroup: '青年(18-35)'
  },
  hourlyChart: []
});

const showSnapshotModal = ref(false);
const isSurged = computed(() => (summary.value.currentOccupancy || 0) >= 25);

// 实时动态波形图表实例与定时器
const pulseChartRef = ref<HTMLDivElement | null>(null);
let pulseChart: echarts.ECharts | null = null;
let pollTimer: any = null;
const isPageActive = ref<boolean>(true);

// 滑动时间序列 (与 Pinia dashboardStore 同步，跨导航与组件销毁持久保留)
const waveTimeline = ref<string[]>([]);
const waveEntryDelta = ref<number[]>([]);
const waveExitDelta = ref<number[]>([]);

const pointOptions = computed(() => {
  return pointStore.points.map((p) => ({
    text: p.pointName,
    value: p.id
  }));
});

// 从全局持久 Store 同步当前点位历史数据
const syncFromHistory = (pointId: number) => {
  if (!pointId) return;
  const hist = dashboardStore.getPointHistory(pointId);
  waveTimeline.value = [...hist.timeline];
  waveEntryDelta.value = [...hist.entryDelta];
  waveExitDelta.value = [...hist.exitDelta];
  if (hist.lastSummary) {
    summary.value = hist.lastSummary;
  }
};

const loadPoints = async () => {
  try {
    const list: any = await h5Api.getMyPoints();
    pointStore.setPoints(list);
    if (list.length > 0 && !activePointId.value) {
      activePointId.value = list[0].id;
    }
  } catch (e) {}
};

// 轮询：拉取当前通道最新人数与变化，推入全局持久时序
const pollTick = async () => {
  if (!activePointId.value) return;
  try {
    const data: any = await h5Api.getDashboardSummary(activePointId.value);
    summary.value = data;

    const now = new Date();
    const timeStr = [
      String(now.getHours()).padStart(2, '0'),
      String(now.getMinutes()).padStart(2, '0'),
      String(now.getSeconds()).padStart(2, '0')
    ].join(':');

    const hist = dashboardStore.getPointHistory(activePointId.value);
    let inDelta = 0;
    let outDelta = 0;

    if (hist.lastEntryCount >= 0) {
      inDelta = Math.max(0, data.todayEntry - hist.lastEntryCount);
    }
    if (hist.lastExitCount >= 0) {
      outDelta = Math.max(0, data.todayExit - hist.lastExitCount);
    }

    // 记录到全局 Store (自动管理滑动历史窗口，并写入 sessionStorage)
    dashboardStore.recordTick(activePointId.value, timeStr, inDelta, outDelta, data);

    // 同步到视图展示
    waveTimeline.value = [...hist.timeline];
    waveEntryDelta.value = [...hist.entryDelta];
    waveExitDelta.value = [...hist.exitDelta];

    // 仅在页面处于激活状态下刷新图表，避免后台渲染异常
    if (isPageActive.value) {
      renderPulseChart();
    }
  } catch (e) {}
};

const renderPulseChart = () => {
  if (!pulseChartRef.value) return;
  if (!pulseChart) {
    pulseChart = echarts.init(pulseChartRef.value);
  }

  const isLight = themeStore.currentTheme === 'light';
  const entryColor = isSurged.value ? '#FF5722' : (isLight ? '#0284C7' : '#00D2FF');
  const exitColor = isLight ? '#E11D48' : '#FF3D71';

  const option: echarts.EChartsOption = {
    tooltip: {
      trigger: 'axis',
      backgroundColor: isLight ? '#FFFFFF' : '#182030',
      borderColor: isLight ? '#CBD5E1' : '#2A364F',
      borderWidth: 1,
      padding: [6, 10],
      textStyle: { color: isLight ? '#0F172A' : '#FFFFFF', fontSize: 11, fontWeight: 700 },
      axisPointer: { type: 'line', lineStyle: { color: entryColor, width: 1, type: 'dashed' } }
    },
    grid: {
      left: '3%',
      right: '6%',
      top: '16%',
      bottom: '14%',
      containLabel: true
    },
    xAxis: {
      type: 'category',
      data: waveTimeline.value,
      name: '时间',
      nameLocation: 'end',
      nameTextStyle: { color: isLight ? '#475569' : '#94A3B8', fontSize: 10, padding: [0, 0, 0, 2] },
      axisLine: {
        show: true,
        symbol: ['none', 'arrow'],
        symbolSize: [6, 8],
        symbolOffset: [0, 6],
        lineStyle: { color: isLight ? '#94A3B8' : '#475569', width: 1.5 }
      },
      axisTick: {
        show: true,
        alignWithLabel: true,
        lineStyle: { color: isLight ? '#CBD5E1' : '#475569' }
      },
      splitLine: {
        show: true,
        lineStyle: { color: isLight ? '#E2E8F0' : '#1A2230', type: 'dashed' }
      },
      axisLabel: { color: isLight ? '#475569' : '#94A3B8', fontSize: 10 }
    },
    yAxis: {
      type: 'value',
      name: `流速(人/${pollRateSec.value}s)`,
      nameLocation: 'end',
      nameTextStyle: { color: isLight ? '#475569' : '#94A3B8', fontSize: 10, padding: [0, 0, 6, 0] },
      minInterval: 1,
      axisLine: {
        show: true,
        symbol: ['none', 'arrow'],
        symbolSize: [6, 8],
        symbolOffset: [0, 6],
        lineStyle: { color: isLight ? '#94A3B8' : '#475569', width: 1.5 }
      },
      axisTick: {
        show: true,
        lineStyle: { color: isLight ? '#CBD5E1' : '#475569' }
      },
      splitLine: {
        show: true,
        lineStyle: { color: isLight ? '#E2E8F0' : '#1F293D', type: 'dashed' }
      },
      axisLabel: { color: isLight ? '#475569' : '#94A3B8', fontSize: 10 }
    },
    series: [
      {
        name: isSurged.value ? '进入流速 (超限警戒)' : '进入流速',
        type: 'line',
        smooth: 0.35,
        symbol: 'circle',
        symbolSize: isSurged.value ? 6 : 5,
        itemStyle: { color: entryColor },
        lineStyle: { width: isSurged.value ? 2.8 : 2.5, color: entryColor },
        areaStyle: {
          color: isSurged.value
            ? new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                { offset: 0, color: 'rgba(255, 87, 34, 0.45)' },
                { offset: 1, color: 'rgba(255, 87, 34, 0.0)' }
              ])
            : new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                { offset: 0, color: isLight ? 'rgba(2, 132, 199, 0.22)' : 'rgba(0, 210, 255, 0.3)' },
                { offset: 1, color: isLight ? 'rgba(2, 132, 199, 0.0)' : 'rgba(0, 210, 255, 0.0)' }
              ])
        },
        data: waveEntryDelta.value
      },
      {
        name: '离开流速',
        type: 'line',
        smooth: 0.35,
        symbol: 'circle',
        symbolSize: 5,
        itemStyle: { color: exitColor },
        lineStyle: { width: 2.5, color: exitColor },
        areaStyle: {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: isLight ? 'rgba(225, 29, 72, 0.2)' : 'rgba(255, 61, 113, 0.3)' },
            { offset: 1, color: isLight ? 'rgba(225, 29, 72, 0.0)' : 'rgba(255, 61, 113, 0.0)' }
          ])
        },
        data: waveExitDelta.value
      }
    ]
  };

  pulseChart.setOption(option);
};

// 监听主题切换，座舱折线图实时重绘
watch(() => themeStore.currentTheme, () => {
  if (isPageActive.value) {
    renderPulseChart();
  }
});

const restartPollTimer = (sec: number) => {
  if (pollTimer) {
    clearInterval(pollTimer);
    pollTimer = null;
  }
  const intervalMs = Math.max(1, sec) * 1000;
  pollTimer = setInterval(pollTick, intervalMs);
};

// 监听系统设置频率更改，座舱秒级响应
watch(() => dashboardStore.pollRateSec, (newRate) => {
  restartPollTimer(newRate);
  if (isPageActive.value) {
    renderPulseChart();
  }
});

const onPointChange = () => {
  if (activePointId.value) {
    syncFromHistory(activePointId.value);
    renderPulseChart();
    pollTick();
  }
};

const handleResize = () => {
  if (pulseChart) pulseChart.resize();
};

onMounted(async () => {
  dashboardStore.syncFromStorage();
  await loadPoints();
  if (activePointId.value) {
    syncFromHistory(activePointId.value);
  }
  nextTick(() => {
    renderPulseChart();
  });
  await pollTick();
  restartPollTimer(pollRateSec.value);
  window.addEventListener('resize', handleResize);
  window.addEventListener('storage', onStorageChange);
});

// keep-alive 页面切回唤醒钩子：立即恢复高保真波形历史，自适应重绘，更新时序
onActivated(() => {
  isPageActive.value = true;
  dashboardStore.syncFromStorage();
  restartPollTimer(pollRateSec.value);
  if (activePointId.value) {
    syncFromHistory(activePointId.value);
  }
  nextTick(() => {
    if (pulseChartRef.value) {
      if (!pulseChart) {
        pulseChart = echarts.init(pulseChartRef.value);
      }
      pulseChart.resize();
      renderPulseChart();
    }
  });
  pollTick();
});

// keep-alive 切出后台钩子：暂停 DOM 图表绘制，数据后台由全局 Store 自动收集
onDeactivated(() => {
  isPageActive.value = false;
});

onUnmounted(() => {
  if (pollTimer) clearInterval(pollTimer);
  window.removeEventListener('resize', handleResize);
  window.removeEventListener('storage', onStorageChange);
  if (pulseChart) {
    pulseChart.dispose();
    pulseChart = null;
  }
});
</script>

<style scoped>
/* 整个页面视口高度固定，严禁任何页面滚动 */
.dashboard-viewport {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  box-sizing: border-box;
  background-color: #0A0D14;
}

/* 顶部极简标题栏 */
.flat-header {
  flex-shrink: 0;
  height: 44px;
  background: #0E131D;
  border-bottom: 1px solid #1E2636;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 12px;
  box-sizing: border-box;
}

.header-left {
  display: flex;
  align-items: center;
  gap: 8px;
}

.header-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #00D2FF;
}

.company-name {
  font-size: 14px;
  font-weight: 700;
  color: #FFFFFF;
  letter-spacing: 0.5px;
  max-width: 240px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.live-status-pill {
  font-size: 10px;
  font-weight: 700;
  padding: 2px 8px;
  border-radius: 4px;
}
.live-status-pill.online {
  background: rgba(0, 230, 118, 0.15);
  color: #00E676;
}
.live-status-pill.offline {
  background: rgba(255, 61, 113, 0.15);
  color: #FF3D71;
}

/* 主体内容区：纯平单屏填充 */
.flat-main {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  padding: 8px 10px;
  box-sizing: border-box;
  overflow: hidden;
  gap: 6px;
}

/* 行 1: 点位选择条 */
.flat-point-bar {
  flex-shrink: 0;
  height: 34px;
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 0 10px;
  display: flex;
  align-items: center;
  gap: 8px;
}

.point-label {
  font-size: 11px;
  color: #94A3B8;
  font-weight: 600;
  flex-shrink: 0;
}

.point-select-wrap {
  flex: 1;
}

.flat-select {
  width: 100%;
  height: 26px;
  background-color: #161D2A;
  color: #00D2FF;
  border: 1px solid #233048;
  border-radius: 4px;
  padding: 0 20px 0 8px;
  font-size: 12px;
  font-weight: 700;
  outline: none;
  color-scheme: dark;
  cursor: pointer;
  appearance: none;
  -webkit-appearance: none;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%2300D2FF' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='m6 9 6 6 6-6'/%3E%3C/svg%3E");
  background-repeat: no-repeat;
  background-position: right 6px center;
  transition: all 0.2s ease;
}

.flat-select:focus {
  border-color: #00D2FF;
  box-shadow: 0 0 0 1px rgba(0, 210, 255, 0.3);
}

.flat-select option {
  background-color: #121824 !important;
  color: #FFFFFF !important;
  font-size: 13px;
  font-weight: 600;
  padding: 8px 12px;
}

.flat-select option:checked {
  background-color: #1E293B !important;
  color: #00D2FF !important;
}

.poll-badge {
  font-size: 10px;
  color: #00D2FF;
  background: rgba(0, 210, 255, 0.12);
  border: 1px solid rgba(0, 210, 255, 0.35);
  padding: 2px 7px;
  border-radius: 4px;
  font-weight: 700;
  flex-shrink: 0;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  gap: 3px;
  transition: all 0.15s ease;
  user-select: none;
}

.poll-badge:hover,
.poll-badge:active {
  background: rgba(0, 210, 255, 0.25);
  border-color: #00D2FF;
  transform: scale(0.97);
}

/* 行 2: 四大指标矩阵 (2x2，等高自适应) */
.kpi-matrix {
  flex: 1;
  min-height: 0;
  max-height: 180px;
  display: grid;
  grid-template-columns: 1fr 1fr;
  grid-template-rows: 1fr 1fr;
  gap: 6px;
}

.kpi-block {
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 8px 10px;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  box-sizing: border-box;
}

.border-cyan { border-left: 3px solid #00D2FF; }
.border-green { border-left: 3px solid #00E676; }
.border-orange { border-left: 3px solid #FF9100; }
.border-purple { border-left: 3px solid #9D4EDD; }

.kpi-top {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.kpi-title {
  font-size: 11px;
  font-weight: 700;
  color: #CBD5E1;
}

.kpi-badge {
  font-size: 9px;
  font-weight: 700;
  padding: 1px 4px;
  border-radius: 2px;
}
.badge-cyan { background: rgba(0, 210, 255, 0.12); color: #00D2FF; }
.badge-green { background: rgba(0, 230, 118, 0.12); color: #00E676; }
.badge-orange { background: rgba(255, 145, 0, 0.12); color: #FF9100; }
.badge-purple { background: rgba(157, 78, 221, 0.12); color: #9D4EDD; }

.kpi-num {
  font-size: 26px;
  font-weight: 800;
  line-height: 1;
  margin: 2px 0;
}

.text-cyan { color: #00D2FF; }
.text-green { color: #00E676; }
.text-orange { color: #FF9100; }
.text-purple { color: #9D4EDD; }

.kpi-unit {
  font-size: 10px;
  font-weight: 600;
  color: #64748B;
  margin-left: 2px;
}

.kpi-footer {
  font-size: 9px;
  color: #64748B;
}

/* 行 3: 安全状态条 */
.flat-status-bar {
  flex-shrink: 0;
  height: 26px;
  border-radius: 4px;
  padding: 0 8px;
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 11px;
  font-weight: 600;
}
.status-ok {
  background: rgba(0, 230, 118, 0.1);
  color: #00E676;
  border: 1px solid rgba(0, 230, 118, 0.2);
}
.status-warn {
  background: rgba(255, 145, 0, 0.1);
  color: #FF9100;
  border: 1px solid rgba(255, 145, 0, 0.2);
}
.status-surged {
  background: rgba(255, 87, 34, 0.18);
  color: #FF5722;
  border: 1px solid #FF5722;
  animation: pulse-border 1.5s infinite;
}
@keyframes pulse-border {
  0% { box-shadow: 0 0 0 0 rgba(255, 87, 34, 0.45); }
  70% { box-shadow: 0 0 0 6px rgba(255, 87, 34, 0); }
  100% { box-shadow: 0 0 0 0 rgba(255, 87, 34, 0); }
}

.status-icon {
  font-weight: 800;
}

/* 行 3.5: 现场实时快照条 */
.flat-snapshot-strip {
  flex-shrink: 0;
  height: 38px;
  background: #111827;
  border: 1px solid #1F2937;
  border-radius: 4px;
  padding: 3px 8px;
  display: flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  transition: all 0.2s;
}
.flat-snapshot-strip:hover {
  border-color: #00D2FF;
}

.snap-thumb-box {
  position: relative;
  width: 52px;
  height: 32px;
  border-radius: 3px;
  overflow: hidden;
  background: #000;
  flex-shrink: 0;
}
.snap-thumb-img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}
.snap-live-tag {
  position: absolute;
  bottom: 1px;
  right: 2px;
  background: rgba(255, 0, 0, 0.85);
  color: #fff;
  font-size: 8px;
  font-weight: 800;
  padding: 0 3px;
  border-radius: 2px;
}
.snap-info {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  justify-content: center;
}
.snap-info-top {
  display: flex;
  align-items: center;
  gap: 6px;
}
.snap-info-title {
  font-size: 11px;
  font-weight: 700;
  color: #E2E8F0;
}
.snap-roi-pill {
  font-size: 9px;
  font-weight: 600;
  color: #00D2FF;
  background: rgba(0, 210, 255, 0.12);
  padding: 0 4px;
  border-radius: 2px;
}
.snap-info-sub {
  font-size: 9.5px;
  color: #64748B;
}
.snap-zoom-icon {
  font-size: 13px;
  color: #94A3B8;
}

/* 现场快照大图弹窗 */
.snapshot-modal-overlay {
  position: fixed;
  top: 0;
  left: 0;
  width: 100vw;
  height: 100vh;
  background: rgba(0, 0, 0, 0.88);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  padding: 16px;
  box-sizing: border-box;
}
.snapshot-modal-card {
  background: #111827;
  border: 1px solid #2A364F;
  border-radius: 8px;
  width: 100%;
  max-width: 500px;
  overflow: hidden;
  display: flex;
  flex-direction: column;
}
.smc-header {
  height: 38px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 12px;
  background: #182030;
  border-bottom: 1px solid #2A364F;
}
.smc-title-box {
  display: flex;
  align-items: center;
  gap: 6px;
}
.live-dot-red {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: #FF3D71;
}
.smc-title {
  font-size: 12px;
  font-weight: 700;
  color: #F8FAFC;
}
.smc-close {
  background: transparent;
  border: none;
  color: #94A3B8;
  font-size: 14px;
  cursor: pointer;
}
.smc-body {
  background: #000;
  display: flex;
  align-items: center;
  justify-content: center;
}
.modal-large-img {
  width: 100%;
  height: auto;
  display: block;
}
.smc-footer {
  padding: 6px 12px;
  background: #182030;
  font-size: 10px;
  color: #94A3B8;
}

.wave-dot-flame {
  background: #FF5722 !important;
  box-shadow: 0 0 6px #FF5722;
}
.w-dot.dot-flame {
  background: #FF5722;
  box-shadow: 0 0 6px #FF5722;
}
.flame-text {
  color: #FF5722 !important;
  font-weight: 700;
}

/* 行 4: 实时动态波形卡片 (填满下方剩余全部高度) */
.realtime-wave-card {
  flex: 1;
  min-height: 0;
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 6px 8px 2px 8px;
  display: flex;
  flex-direction: column;
  box-sizing: border-box;
}

.wave-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 2px;
}

.wave-title-box {
  display: flex;
  align-items: center;
  gap: 6px;
}

.wave-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: #00D2FF;
}

.wave-title {
  font-size: 11px;
  font-weight: 700;
  color: #CBD5E1;
}

.wave-legend {
  display: flex;
  align-items: center;
  gap: 8px;
}

.w-leg-item {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 10px;
  font-weight: 600;
  color: #94A3B8;
}

.w-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
}
.w-dot.cyan { background: #00D2FF; }
.w-dot.pink { background: #FF3D71; }

.pulse-chart-container {
  flex: 1;
  min-height: 0;
  width: 100%;
}

/* 刷新频率快捷弹窗样式 */
.rate-modal-overlay {
  position: fixed;
  top: 0;
  left: 0;
  width: 100vw;
  height: 100vh;
  background: rgba(0, 0, 0, 0.75);
  backdrop-filter: blur(4px);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 2000;
  padding: 16px;
  box-sizing: border-box;
}

.rate-modal-card {
  background: #111827;
  border: 1px solid #2A364F;
  border-radius: 10px;
  width: 100%;
  max-width: 360px;
  overflow: hidden;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.8);
}

.rm-header {
  height: 42px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 14px;
  background: #182030;
  border-bottom: 1px solid #2A364F;
}

.rm-title-box {
  display: flex;
  align-items: center;
  gap: 6px;
}

.rm-icon {
  font-size: 14px;
}

.rm-title {
  font-size: 13px;
  font-weight: 700;
  color: #FFFFFF;
}

.rm-close {
  background: transparent;
  border: none;
  color: #94A3B8;
  font-size: 15px;
  cursor: pointer;
  padding: 4px;
}

.rm-body {
  padding: 14px;
}

.rm-desc {
  font-size: 11px;
  color: #94A3B8;
  margin: 0 0 12px 0;
  line-height: 150%;
}

.rate-select-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.rate-card {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: #0B0E14;
  border: 1px solid #1F293D;
  border-radius: 6px;
  padding: 10px 12px;
  cursor: pointer;
  transition: all 0.15s;
}

.rate-card:active {
  background: #182030;
}

.rate-card-active {
  border-color: #00D2FF;
  background: rgba(0, 210, 255, 0.08);
}

.rc-left {
  display: flex;
  align-items: center;
  gap: 10px;
}

.rc-radio {
  color: #00D2FF;
  font-size: 14px;
  font-weight: 700;
}

.rc-text {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.rc-title {
  font-size: 12px;
  font-weight: 700;
  color: #FFFFFF;
}

.rc-desc {
  font-size: 10px;
  color: #64748B;
}

.rc-tag {
  font-size: 10px;
  color: #00D2FF;
  background: rgba(0, 210, 255, 0.15);
  border: 1px solid rgba(0, 210, 255, 0.4);
  padding: 1px 6px;
  border-radius: 3px;
  font-weight: 700;
}

/* ==================== 明亮浅色模式适配 (Light Theme) ==================== */
:global(.theme-light) .dashboard-viewport {
  background-color: var(--fs-chassis-bg);
}
:global(.theme-light) .flat-header {
  background: var(--fs-header-bg);
  border-bottom: 1px solid var(--fs-header-border);
}
:global(.theme-light) .company-name {
  color: var(--fs-text-main);
}
:global(.theme-light) .flat-point-bar {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.04);
}
:global(.theme-light) .point-label {
  color: var(--fs-text-sub);
}
:global(.theme-light) .flat-select {
  background-color: var(--fs-select-bg);
  border-color: var(--fs-select-border);
  color: var(--fs-select-text);
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%230284C7' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='m6 9 6 6 6-6'/%3E%3C/svg%3E");
}
:global(.theme-light) .poll-badge {
  background: rgba(2, 132, 199, 0.1);
  border-color: rgba(2, 132, 199, 0.4);
  color: #0284C7;
}
:global(.theme-light) .kpi-block {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.04);
}
:global(.theme-light) .border-cyan { border-left-color: #0284C7; }
:global(.theme-light) .border-green { border-left-color: #16A34A; }
:global(.theme-light) .border-orange { border-left-color: #EA580C; }
:global(.theme-light) .border-purple { border-left-color: #7C3AED; }
:global(.theme-light) .badge-cyan { background: rgba(2, 132, 199, 0.12); color: #0284C7; }
:global(.theme-light) .badge-green { background: rgba(22, 163, 74, 0.12); color: #16A34A; }
:global(.theme-light) .badge-orange { background: rgba(234, 88, 12, 0.12); color: #EA580C; }
:global(.theme-light) .badge-purple { background: rgba(124, 58, 237, 0.12); color: #7C3AED; }
:global(.theme-light) .text-cyan { color: #0284C7; }
:global(.theme-light) .text-green { color: #16A34A; }
:global(.theme-light) .text-orange { color: #EA580C; }
:global(.theme-light) .text-purple { color: #7C3AED; }
:global(.theme-light) .kpi-unit { color: var(--fs-text-dim); }
:global(.theme-light) .kpi-title {
  color: var(--fs-text-sub);
}
:global(.theme-light) .kpi-footer {
  color: var(--fs-text-dim);
}
:global(.theme-light) .flat-status-bar {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
  color: var(--fs-text-main);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.04);
}
:global(.theme-light) .flat-status-bar.status-ok {
  background: rgba(22, 163, 74, 0.08);
  border-color: rgba(22, 163, 74, 0.3);
  color: #15803D;
}
:global(.theme-light) .flat-snapshot-strip {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.04);
}
:global(.theme-light) .snap-info-title {
  color: var(--fs-text-main);
}
:global(.theme-light) .snap-info-sub {
  color: var(--fs-text-dim);
}
:global(.theme-light) .realtime-wave-card {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.04);
}
:global(.theme-light) .wave-title {
  color: var(--fs-text-main);
}
:global(.theme-light) .w-leg-item {
  color: var(--fs-text-sub);
}
:global(.theme-light) .rate-modal-card {
  background: #FFFFFF;
  border-color: #CBD5E1;
  box-shadow: 0 10px 30px rgba(15, 23, 42, 0.15);
}
:global(.theme-light) .rm-header {
  background: #F8FAFC;
  border-bottom-color: #E2E8F0;
}
:global(.theme-light) .rm-title {
  color: var(--fs-text-main);
}
:global(.theme-light) .rm-desc {
  color: var(--fs-text-sub);
}
:global(.theme-light) .rate-modal-card .rate-card {
  background: #F8FAFC;
  border-color: #E2E8F0;
}
:global(.theme-light) .rate-modal-card .rate-card-active {
  background: rgba(2, 132, 199, 0.08);
  border-color: #0284C7;
}
:global(.theme-light) .rate-modal-card .rc-title {
  color: var(--fs-text-main);
}
:global(.theme-light) .rate-modal-card .rc-desc {
  color: var(--fs-text-dim);
}
:global(.theme-light) .snapshot-modal-card {
  background: #FFFFFF;
  border-color: #CBD5E1;
}
:global(.theme-light) .smc-header,
:global(.theme-light) .smc-footer {
  background: #F8FAFC;
  border-color: #E2E8F0;
}
:global(.theme-light) .smc-title {
  color: var(--fs-text-main);
}
</style>