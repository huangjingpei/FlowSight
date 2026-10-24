<template>
  <div class="dashboard-page">
    <div class="cockpit-shell">
      <!-- 顶部智能座舱 HUD 导航栏 -->
      <van-nav-bar fixed placeholder class="cockpit-nav-bar">
        <template #left>
          <div class="nav-brand">
            <span class="brand-laser-dot"></span>
            <span class="brand-title">{{ currentCompanyName }}</span>
          </div>
        </template>
        <template #right>
          <div class="nav-logout-btn" @click="handleLogout">
            <van-icon name="revoke" size="16" />
            <span class="logout-text">退出</span>
          </div>
        </template>
      </van-nav-bar>

      <!-- 若无开通点位，显示友好引导状态 -->
      <div v-if="!pointsLoading && pointStore.points.length === 0" class="no-points-container">
        <van-empty image="network">
          <template #description>
            <div class="empty-desc-bold">当前商户账号尚未配置视频点位</div>
            <div class="empty-desc-sub">请联系系统总管理员分配监控点位与边端工控机授权</div>
          </template>
          <van-button round type="primary" size="small" class="refresh-btn" @click="handleRefreshPoints">
            刷新点位状态
          </van-button>
        </van-empty>
      </div>

      <!-- 正常已配置点位的智能座舱大屏内容 -->
      <template v-else>
        <!-- 顶部点位选择器: 胶囊舱式悬浮卡片 -->
        <div class="point-switcher-capsule">
          <van-dropdown-menu :disabled="pointStore.isMediaPlaying">
            <van-dropdown-item
              v-model="activePointId"
              :options="pointOptions"
              :disabled="pointStore.isMediaPlaying"
              @change="onPointChange"
            />
          </van-dropdown-menu>
          <div v-if="pointStore.isMediaPlaying" class="locked-tip">
            <span>🔒 视频回放中已锁定点位切换</span>
          </div>
        </div>

        <!-- 客户端节点身份与健康状态铭牌 (HUD 仪表条) -->
        <div class="client-meta-hud" v-if="summary.pointId">
          <div class="client-info">
            <span class="client-icon">💻</span>
            <span class="client-name">{{ summary.clientName || ('FlowSight-Node-' + summary.pointId) }}</span>
            <span class="client-version">Ver: {{ summary.appVersion || 'v1.2.0' }}</span>
          </div>
          <div class="client-status">
            <span :class="['status-pulse-dot', summary.streamStatus === 'ONLINE' ? 'online' : 'offline']"></span>
            <span class="status-text">{{ summary.streamStatus === 'ONLINE' ? '实时在线' : '离线' }}</span>
            <span class="last-hb" v-if="summary.lastHeartbeat">· {{ summary.lastHeartbeat.split(' ')[1] || summary.lastHeartbeat }}</span>
          </div>
        </div>

        <!-- 承载安全与超载防聚集监测条 -->
        <div :class="['safety-banner', summary.currentOccupancy > 30 ? 'safety-warning' : 'safety-normal']">
          <span class="safety-icon">{{ summary.currentOccupancy > 30 ? '⚠️' : '🛡️' }}</span>
          <span class="safety-text">
            {{ summary.currentOccupancy > 30 ? '当前区域人员聚集密度较高，已触发人流疏导指引' : '当前通道人流承载与通行状态正常' }}
          </span>
        </div>

        <van-pull-refresh v-model="refreshing" @refresh="onRefresh">
          <!-- 4 大核心客流数显座舱仪表矩阵 -->
          <div class="kpi-grid">
            <div class="kpi-card card-cyan">
              <div class="kpi-header">
                <span class="kpi-label">当前区域滞留</span>
                <span class="kpi-badge"><span class="badge-dot cyan-dot"></span>实时在场</span>
              </div>
              <div class="kpi-value">{{ summary.currentOccupancy }} <span class="unit">人</span></div>
              <div class="kpi-sub">瞬时在场感知与安全基线</div>
            </div>

            <div class="kpi-card card-emerald">
              <div class="kpi-header">
                <span class="kpi-label">今日累计进入 (In)</span>
                <span class="kpi-badge"><span class="badge-dot emerald-dot"></span>进场通行</span>
              </div>
              <div class="kpi-value">{{ summary.todayEntry }} <span class="unit">人次</span></div>
              <div class="kpi-sub">主入口进门累计总人次</div>
            </div>

            <div class="kpi-card card-amber">
              <div class="kpi-header">
                <span class="kpi-label">今日累计离开 (Out)</span>
                <span class="kpi-badge"><span class="badge-dot amber-dot"></span>离场离闸</span>
              </div>
              <div class="kpi-value">{{ summary.todayExit }} <span class="unit">人次</span></div>
              <div class="kpi-sub">出口闸道离开累计总人次</div>
            </div>

            <div class="kpi-card card-violet">
              <div class="kpi-header">
                <span class="kpi-label">全天通行总人流</span>
                <span class="kpi-badge"><span class="badge-dot violet-dot"></span>双向汇聚</span>
              </div>
              <div class="kpi-value">{{ summary.todayTotal }} <span class="unit">人次</span></div>
              <div class="kpi-sub">全天双向吞吐客流总量</div>
            </div>
          </div>

          <!-- 核心多维时序走势大屏模块 (支持按真实采集时间时序绘制) -->
          <div class="section-card trend-section">
            <div class="section-header">
              <div class="section-title">
                <span class="glow-icon">📈</span> 客流时序走势与高阶全景曲线
              </div>
              <div class="live-tag">
                <span class="radar-dot"></span>
                <span>动态采集流</span>
              </div>
            </div>

            <!-- 统计粒度切换标签 (默认实时采集时序流水) -->
            <div class="dimension-segmented">
              <div
                v-for="dim in dimensionOptions"
                :key="dim.key"
                :class="['seg-item', currentDimension === dim.key ? 'active' : '']"
                @click="switchDimension(dim.key, dim.days)"
              >
                {{ dim.label }}
              </div>
            </div>

            <!-- 统计核心指标摘要药丸 HUD -->
            <div class="trend-summary-pills">
              <div class="pill-item">
                <span class="pill-lbl">周期总客流</span>
                <span class="pill-val">{{ trendData.totalPeriodCount || summary.todayTotal }} 人</span>
              </div>
              <div class="pill-item">
                <span class="pill-lbl">单点最高峰值</span>
                <span class="pill-val peak-highlight">{{ trendData.peakTime }} ({{ trendData.peakValue }}人)</span>
              </div>
              <div class="pill-item">
                <span class="pill-lbl">采样均值</span>
                <span class="pill-val">{{ trendData.averageCount || '-' }} 人/点</span>
              </div>
            </div>

            <!-- 高保真座舱全息光效曲线图容器 -->
            <div ref="chartRef" class="echarts-container"></div>
          </div>

          <!-- 客群属性画像模块: 性别比例与主要年龄段 -->
          <div class="section-card">
            <div class="section-header">
              <div class="section-title">
                <span class="glow-icon">👥</span> 客群画像结构与生物特征识别
              </div>
              <span class="sub-badge">AI 多目标感知</span>
            </div>
            <div class="demo-box" v-if="summary.demographics">
              <div class="gender-bar-wrapper">
                <div class="gender-header">
                  <span class="gender-male-text">
                    👨 男性顾客: {{ (summary.demographics.maleRatio * 100).toFixed(1) }}% ({{ summary.demographics.maleCount }}人)
                  </span>
                  <span class="gender-female-text">
                    👩 女性顾客: {{ (summary.demographics.femaleRatio * 100).toFixed(1) }}% ({{ summary.demographics.femaleCount }}人)
                  </span>
                </div>
                <div class="gender-bar">
                  <div class="gender-male" :style="{ width: (summary.demographics.maleRatio * 100) + '%' }"></div>
                  <div class="gender-female" :style="{ width: (summary.demographics.femaleRatio * 100) + '%' }"></div>
                </div>
              </div>
              <div class="age-distribution-row">
                <div class="age-tag-box">
                  <span class="age-label">主力到场年龄段:</span>
                  <van-tag type="primary" size="medium" class="age-tag">{{ summary.demographics.topAgeGroup || '青年(18-35)' }}</van-tag>
                </div>
                <div class="tag-hint">基于边缘 NPU 深度特征提取</div>
              </div>
            </div>
          </div>

          <!-- 证据切片回放中心快速入口 -->
          <div class="section-card playback-entry-card" @click="goToPlayback">
            <div class="entry-flex">
              <div class="entry-icon-box">🎬</div>
              <div class="entry-info">
                <div class="entry-title">通行切片证据中心与录像回放</div>
                <div class="entry-desc">支持多通道检索、高精时间筛选、首尾页翻页与 H.264 高清切片原画复核</div>
              </div>
              <div class="entry-arrow">
                <span class="btn-go-playback">进入回放 ➔</span>
              </div>
            </div>
          </div>

          <!-- 边缘设备与系统运行遥测 -->
          <div class="section-card telemetry-card">
            <div class="section-header">
              <div class="section-title">
                <span class="glow-icon">⚡</span> 边端硬件与算法遥测指标
              </div>
              <span class="sub-badge">边缘工业总线</span>
            </div>
            <div class="telemetry-grid">
              <div class="tele-item">
                <span class="tele-k">AI 推理引擎</span>
                <span class="tele-v text-cyan">YOLOv8n (ORT C++)</span>
              </div>
              <div class="tele-item">
                <span class="tele-k">实时处理帧率</span>
                <span class="tele-v text-emerald">30.0 FPS</span>
              </div>
              <div class="tele-item">
                <span class="tele-k">单帧推理耗时</span>
                <span class="tele-v text-amber">14.2 ms</span>
              </div>
              <div class="tele-item">
                <span class="tele-k">离线容灾积压</span>
                <span class="tele-v text-violet">0 条 (实时同步)</span>
              </div>
            </div>
          </div>
        </van-pull-refresh>
      </template>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, nextTick } from 'vue';
import { useRouter } from 'vue-router';
import { showToast, showDialog } from 'vant';
import * as echarts from 'echarts';
import { h5Api } from '../api';
import { usePointStore } from '../store/point';

const router = useRouter();
const pointStore = usePointStore();

const currentUser = JSON.parse(localStorage.getItem('flowsight_user') || '{}');
const currentCompanyName = ref(currentUser.companyName || currentUser.nickname || 'FlowSight 智能决策中心');

const goToPlayback = () => {
  router.push('/playback');
};

const activePointId = ref<number>(0);
const refreshing = ref(false);
const summary = ref<any>({
  currentOccupancy: 0,
  todayEntry: 0,
  todayExit: 0,
  todayTotal: 0,
  clientName: '',
  appVersion: '',
  lastHeartbeat: '',
  demographics: { maleRatio: 0.5, femaleRatio: 0.5, maleCount: 0, femaleCount: 0, topAgeGroup: '青年(18-35)' }
});

// 多粒度趋势维度选择: 默认启用「实时采集时序」
const dimensionOptions = [
  { key: 'realtime', label: '⚡ 实时采集时序', days: 1 },
  { key: 'hour', label: '⏱️ 今日逐时', days: 1 },
  { key: 'day_7', label: '📅 近7天走势', days: 7 },
  { key: 'day_30', label: '📊 近30天走势', days: 30 },
  { key: 'quarter', label: '🌐 季度宏观', days: 90 }
];
const currentDimension = ref('realtime');
const currentDays = ref(1);

const trendData = ref<any>({
  dimension: 'realtime',
  title: '今日实时通行时序曲线 (真实采集时间动态流)',
  categories: [],
  entryList: [],
  exitList: [],
  totalList: [],
  peakValue: 0,
  peakTime: '-',
  totalPeriodCount: 0,
  averageCount: 0.0
});

// ECharts 实例引用
const chartRef = ref<HTMLDivElement | null>(null);
let myChart: echarts.ECharts | null = null;
let timer: any = null;

const pointOptions = computed(() => {
  return pointStore.points.map((p) => ({
    text: `${p.pointName} (${p.streamStatus === 'ONLINE' ? '在线' : '离线'})`,
    value: p.id
  }));
});

const pointsLoading = ref(true);

const loadPoints = async () => {
  pointsLoading.value = true;
  try {
    const list: any = await h5Api.getMyPoints();
    pointStore.setPoints(list);
    if (list.length > 0 && !activePointId.value) {
      activePointId.value = list[0].id;
    }
  } catch (e) {
    //
  } finally {
    pointsLoading.value = false;
  }
};

const handleRefreshPoints = async () => {
  await loadPoints();
  if (activePointId.value) {
    await loadSummary();
    await loadTrends();
  }
};

const loadSummary = async () => {
  if (!activePointId.value) return;
  try {
    const data: any = await h5Api.getDashboardSummary(activePointId.value);
    summary.value = data;
  } catch (e) {
    //
  }
};

const loadTrends = async () => {
  if (!activePointId.value) return;
  try {
    let dim = currentDimension.value;
    if (dim.startsWith('day')) dim = 'day';
    const data: any = await h5Api.getTrends(activePointId.value, dim, currentDays.value);
    trendData.value = data;
    renderECharts();
  } catch (e) {
    //
  }
};

const renderECharts = () => {
  if (!chartRef.value) return;
  if (!myChart) {
    myChart = echarts.init(chartRef.value);
  }

  const isBar = currentDimension.value === 'quarter';
  const categories = trendData.value.categories || [];
  const entryData = trendData.value.entryList || [];
  const exitData = trendData.value.exitList || [];

  const option: echarts.EChartsOption = {
    tooltip: {
      trigger: 'axis',
      backgroundColor: 'rgba(7, 13, 26, 0.94)',
      borderColor: 'rgba(0, 242, 254, 0.5)',
      borderWidth: 1,
      padding: [10, 14],
      textStyle: { color: '#e2e8f0', fontSize: 12 },
      axisPointer: { type: isBar ? 'shadow' : 'line', lineStyle: { color: '#00f2fe', type: 'dashed' } }
    },
    legend: {
      data: ['进入人次 (In)', '离开人次 (Out)'],
      bottom: 2,
      icon: 'roundRect',
      textStyle: { color: '#94a3b8', fontSize: 11 }
    },
    grid: {
      left: '3%',
      right: '4%',
      top: '12%',
      bottom: '16%',
      containLabel: true
    },
    xAxis: {
      type: 'category',
      data: categories,
      boundaryGap: isBar,
      axisLine: { lineStyle: { color: 'rgba(255, 255, 255, 0.12)' } },
      axisLabel: { color: '#94a3b8', fontSize: 10, interval: categories.length > 14 ? 1 : 0 }
    },
    yAxis: {
      type: 'value',
      splitLine: { lineStyle: { color: 'rgba(255, 255, 255, 0.06)', type: 'dashed' } },
      axisLabel: { color: '#64748b', fontSize: 10 }
    },
    series: [
      {
        name: '进入人次 (In)',
        type: isBar ? 'bar' : 'line',
        smooth: 0.35,
        showSymbol: categories.length <= 20,
        symbolSize: 6,
        itemStyle: { color: '#00f2fe' },
        lineStyle: { width: 3, shadowColor: 'rgba(0, 242, 254, 0.4)', shadowBlur: 8 },
        areaStyle: isBar ? undefined : {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(0, 242, 254, 0.4)' },
            { offset: 1, color: 'rgba(0, 242, 254, 0.01)' }
          ])
        },
        data: entryData
      },
      {
        name: '离开人次 (Out)',
        type: isBar ? 'bar' : 'line',
        smooth: 0.35,
        showSymbol: categories.length <= 20,
        symbolSize: 6,
        itemStyle: { color: '#ff5e7e' },
        lineStyle: { width: 3, shadowColor: 'rgba(255, 94, 126, 0.4)', shadowBlur: 8 },
        areaStyle: isBar ? undefined : {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(255, 94, 126, 0.35)' },
            { offset: 1, color: 'rgba(255, 94, 126, 0.01)' }
          ])
        },
        data: exitData
      }
    ]
  };

  myChart.setOption(option, true);
};

const switchDimension = (key: string, days: number) => {
  currentDimension.value = key;
  currentDays.value = days;
  loadTrends();
};

const onPointChange = (val: number) => {
  activePointId.value = val;
  loadSummary();
  loadTrends();
};

const onRefresh = async () => {
  await Promise.all([loadPoints(), loadSummary(), loadTrends()]);
  refreshing.value = false;
  showToast({ message: '数据已同步刷新', position: 'top', duration: 1000 });
};

const handleLogout = () => {
  showDialog({
    title: '退出登录',
    message: '确定要退出当前 FlowSight 决策账号吗？',
    showCancelButton: true
  }).then(() => {
    localStorage.removeItem('flowsight_token');
    localStorage.removeItem('flowsight_user');
    showToast('已安全登出');
    router.push('/login');
  }).catch(() => {});
};

const handleResize = () => {
  if (myChart) {
    myChart.resize();
  }
};

onMounted(async () => {
  await loadPoints();
  if (activePointId.value) {
    await loadSummary();
    await loadTrends();
  }
  window.addEventListener('resize', handleResize);
  timer = setInterval(() => {
    loadSummary();
    loadTrends();
  }, 10000);
});

onUnmounted(() => {
  window.removeEventListener('resize', handleResize);
  if (timer) clearInterval(timer);
  if (myChart) {
    myChart.dispose();
    myChart = null;
  }
});
</script>

<style scoped>
.dashboard-page {
  min-height: 100vh;
  background-color: #060911;
  background-image: 
    radial-gradient(circle at 50% 0%, rgba(14, 116, 144, 0.22) 0%, transparent 60%),
    radial-gradient(circle at 100% 100%, rgba(99, 102, 241, 0.12) 0%, transparent 50%);
  display: flex;
  justify-content: center;
  align-items: flex-start;
  color: #f1f5f9;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
  overflow-x: hidden;
}

/* 智能座舱主体机壳：桌面端居中约束，移动端 100% 贴合 */
.cockpit-shell {
  width: 100%;
  max-width: 520px;
  min-height: 100vh;
  background: rgba(10, 15, 29, 0.94);
  border-left: 1px solid rgba(255, 255, 255, 0.06);
  border-right: 1px solid rgba(255, 255, 255, 0.06);
  box-shadow: 0 0 60px rgba(0, 0, 0, 0.9), 0 0 30px rgba(0, 242, 254, 0.06);
  display: flex;
  flex-direction: column;
  position: relative;
  box-sizing: border-box;
  padding-bottom: 70px;
}

/* 顶部智能座舱 HUD 导航栏 */
.cockpit-nav-bar {
  background: rgba(9, 14, 26, 0.88) !important;
  backdrop-filter: blur(20px) !important;
  -webkit-backdrop-filter: blur(20px) !important;
  border-bottom: 1px solid rgba(255, 255, 255, 0.08);
}

.nav-brand {
  display: flex;
  align-items: center;
  gap: 8px;
}

.brand-laser-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: #00f2fe;
  box-shadow: 0 0 10px #00f2fe;
}

.brand-title {
  font-size: 14px;
  font-weight: 700;
  color: #f8fafc;
  letter-spacing: 0.5px;
  max-width: 280px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.nav-logout-btn {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 12px;
  color: #94a3b8;
  cursor: pointer;
  padding: 4px 8px;
  border-radius: 6px;
  background: rgba(255, 255, 255, 0.04);
}

.nav-logout-btn:active {
  opacity: 0.7;
}

/* 顶部点位选择器: 胶囊舱式悬浮卡片 */
.point-switcher-capsule {
  margin: 12px 14px 4px 14px;
  border-radius: 24px;
  background: rgba(15, 23, 42, 0.8);
  border: 1px solid rgba(0, 242, 254, 0.28);
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.4), inset 0 0 12px rgba(0, 242, 254, 0.06);
  position: relative;
}

:deep(.van-dropdown-menu) {
  background: transparent !important;
}

:deep(.van-dropdown-menu__bar) {
  background: transparent !important;
  box-shadow: none !important;
  height: 42px;
}

:deep(.van-dropdown-menu__title) {
  color: #00f2fe !important;
  font-weight: 600;
  font-size: 13px;
}

:deep(.van-dropdown-menu__title:after) {
  border-color: transparent transparent #00f2fe #00f2fe !important;
}

:deep(.van-dropdown-item__content) {
  background: #0b1120 !important;
  border: 1px solid rgba(0, 242, 254, 0.25);
  border-radius: 0 0 14px 14px;
  box-shadow: 0 12px 30px rgba(0, 0, 0, 0.8);
}

:deep(.van-cell) {
  background: transparent !important;
  color: #cbd5e1 !important;
}

:deep(.van-cell--clickable:active) {
  background: rgba(0, 242, 254, 0.1) !important;
}

:deep(.van-dropdown-item__option--active) {
  color: #00f2fe !important;
  font-weight: 700;
}

.locked-tip {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(5, 8, 16, 0.85);
  border-radius: 24px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fbbf24;
  font-size: 12px;
  font-weight: 600;
  z-index: 10;
}

/* 客户端节点身份与健康状态铭牌 (HUD 仪表条) */
.client-meta-hud {
  margin: 8px 14px 4px 14px;
  padding: 8px 14px;
  background: rgba(13, 20, 36, 0.65);
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 10px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.client-info {
  display: flex;
  align-items: center;
  gap: 6px;
}

.client-name {
  font-size: 12px;
  font-weight: 600;
  color: #e2e8f0;
}

.client-version {
  font-size: 10px;
  color: #64748b;
  background: rgba(255, 255, 255, 0.06);
  padding: 1px 6px;
  border-radius: 4px;
}

.client-status {
  display: flex;
  align-items: center;
  gap: 6px;
}

.status-pulse-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  position: relative;
}

.status-pulse-dot.online {
  background: #10b981;
  box-shadow: 0 0 10px #10b981;
  animation: pulse-online 2s infinite;
}

.status-pulse-dot.offline {
  background: #ef4444;
  box-shadow: 0 0 8px #ef4444;
}

@keyframes pulse-online {
  0% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7); }
  70% { box-shadow: 0 0 0 6px rgba(16, 185, 129, 0); }
  100% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0); }
}

.status-text {
  font-size: 11px;
  font-weight: 600;
  color: #10b981;
}

.last-hb {
  font-size: 10px;
  color: #64748b;
}

/* 承载安全监测条 */
.safety-banner {
  margin: 6px 14px 8px 14px;
  padding: 8px 12px;
  border-radius: 8px;
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 11px;
}

.safety-normal {
  background: rgba(16, 185, 129, 0.1);
  border: 1px solid rgba(16, 185, 129, 0.25);
  color: #34d399;
}

.safety-warning {
  background: rgba(245, 158, 11, 0.15);
  border: 1px solid rgba(245, 158, 11, 0.4);
  color: #fbbf24;
}

/* 4 大核心客流数显座舱仪表矩阵 */
.kpi-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 10px;
  padding: 4px 14px 6px 14px;
}

.kpi-card {
  position: relative;
  border-radius: 14px;
  padding: 12px 14px;
  background: rgba(15, 23, 42, 0.7);
  backdrop-filter: blur(16px);
  -webkit-backdrop-filter: blur(16px);
  border: 1px solid rgba(255, 255, 255, 0.08);
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  overflow: hidden;
  transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
}

.kpi-card::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 2px;
}

/* 1. 瞬时滞留 (Electric Cyan) */
.card-cyan {
  border-color: rgba(6, 182, 212, 0.35);
  background: linear-gradient(145deg, rgba(6, 182, 212, 0.12) 0%, rgba(15, 23, 42, 0.85) 100%);
  box-shadow: 0 8px 24px rgba(6, 182, 212, 0.12);
}
.card-cyan::before {
  background: linear-gradient(90deg, #00f2fe, #4facfe);
}
.card-cyan .kpi-value {
  color: #22d3ee;
  text-shadow: 0 0 16px rgba(34, 211, 238, 0.4);
}

/* 2. 累计进入 (Neon Emerald) */
.card-emerald {
  border-color: rgba(16, 185, 129, 0.35);
  background: linear-gradient(145deg, rgba(16, 185, 129, 0.12) 0%, rgba(15, 23, 42, 0.85) 100%);
  box-shadow: 0 8px 24px rgba(16, 185, 129, 0.12);
}
.card-emerald::before {
  background: linear-gradient(90deg, #10b981, #059669);
}
.card-emerald .kpi-value {
  color: #34d399;
  text-shadow: 0 0 16px rgba(52, 211, 153, 0.4);
}

/* 3. 累计离开 (Cyber Amber) */
.card-amber {
  border-color: rgba(245, 158, 11, 0.35);
  background: linear-gradient(145deg, rgba(245, 158, 11, 0.12) 0%, rgba(15, 23, 42, 0.85) 100%);
  box-shadow: 0 8px 24px rgba(245, 158, 11, 0.12);
}
.card-amber::before {
  background: linear-gradient(90deg, #f59e0b, #d97706);
}
.card-amber .kpi-value {
  color: #fbbf24;
  text-shadow: 0 0 16px rgba(251, 191, 36, 0.4);
}

/* 4. 双向总客流 (Ultraviolet) */
.card-violet {
  border-color: rgba(139, 92, 246, 0.35);
  background: linear-gradient(145deg, rgba(139, 92, 246, 0.12) 0%, rgba(15, 23, 42, 0.85) 100%);
  box-shadow: 0 8px 24px rgba(139, 92, 246, 0.12);
}
.card-violet::before {
  background: linear-gradient(90deg, #a78bfa, #7c3aed);
}
.card-violet .kpi-value {
  color: #c4b5fd;
  text-shadow: 0 0 16px rgba(196, 181, 253, 0.4);
}

.kpi-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 4px;
}

.kpi-label {
  font-size: 11px;
  color: #94a3b8;
  font-weight: 500;
}

.kpi-badge {
  font-size: 9px;
  padding: 2px 6px;
  border-radius: 10px;
  background: rgba(255, 255, 255, 0.08);
  color: #cbd5e1;
  display: flex;
  align-items: center;
  gap: 3px;
}

.badge-dot {
  width: 5px;
  height: 5px;
  border-radius: 50%;
}
.cyan-dot { background: #00f2fe; }
.emerald-dot { background: #10b981; }
.amber-dot { background: #f59e0b; }
.violet-dot { background: #a78bfa; }

.kpi-value {
  font-size: 26px;
  font-weight: 800;
  font-family: "DIN Alternate", "Orbitron", -apple-system, sans-serif;
  line-height: 1.1;
  margin: 3px 0;
}

.kpi-value .unit {
  font-size: 11px;
  font-weight: normal;
  color: #94a3b8;
  margin-left: 2px;
}

.kpi-sub {
  font-size: 10px;
  color: #64748b;
}

/* 核心趋势大屏卡片 */
.section-card {
  margin: 10px 14px;
  border-radius: 14px;
  background: rgba(15, 23, 42, 0.72);
  backdrop-filter: blur(16px);
  -webkit-backdrop-filter: blur(16px);
  border: 1px solid rgba(255, 255, 255, 0.08);
  box-shadow: 0 8px 30px rgba(0, 0, 0, 0.45);
  padding: 14px 12px;
}

.section-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 10px;
}

.section-title {
  font-size: 13px;
  font-weight: 700;
  color: #f8fafc;
  display: flex;
  align-items: center;
  gap: 6px;
}

.section-title .glow-icon {
  color: #00f2fe;
  text-shadow: 0 0 10px rgba(0, 242, 254, 0.6);
}

.live-tag {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 10px;
  color: #00f2fe;
  background: rgba(0, 242, 254, 0.1);
  padding: 2px 6px;
  border-radius: 10px;
  border: 1px solid rgba(0, 242, 254, 0.3);
}

.radar-dot {
  width: 5px;
  height: 5px;
  border-radius: 50%;
  background: #00f2fe;
  animation: pulse-online 1.5s infinite;
}

.sub-badge {
  font-size: 10px;
  color: #94a3b8;
  background: rgba(255, 255, 255, 0.06);
  padding: 2px 6px;
  border-radius: 4px;
}

/* 维度切换分段条 */
.dimension-segmented {
  display: flex;
  background: rgba(7, 11, 20, 0.85);
  border-radius: 10px;
  padding: 3px;
  gap: 3px;
  border: 1px solid rgba(255, 255, 255, 0.06);
  margin-bottom: 10px;
  overflow-x: auto;
}

.seg-item {
  flex: 1;
  text-align: center;
  padding: 6px 2px;
  font-size: 10px;
  color: #94a3b8;
  border-radius: 7px;
  cursor: pointer;
  white-space: nowrap;
  transition: all 0.25s ease;
}

.seg-item.active {
  background: linear-gradient(135deg, #00f2fe 0%, #0284c7 100%);
  color: #040914;
  font-weight: 700;
  box-shadow: 0 2px 10px rgba(0, 242, 254, 0.4);
}

/* 统计指标药丸 HUD */
.trend-summary-pills {
  display: flex;
  justify-content: space-around;
  background: rgba(10, 16, 30, 0.65);
  border: 1px solid rgba(255, 255, 255, 0.04);
  border-radius: 10px;
  padding: 8px 10px;
  margin-bottom: 10px;
}

.pill-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 2px;
}

.pill-lbl {
  font-size: 9px;
  color: #64748b;
}

.pill-val {
  font-size: 12px;
  font-weight: 700;
  color: #e2e8f0;
}

.pill-val.peak-highlight {
  color: #38bdf8;
  text-shadow: 0 0 8px rgba(56, 189, 248, 0.4);
}

.echarts-container {
  width: 100%;
  height: 270px;
}

/* 客群画像 */
.demo-box {
  margin-top: 8px;
}

.gender-bar-wrapper {
  background: rgba(7, 11, 20, 0.7);
  border-radius: 10px;
  padding: 10px 12px;
  border: 1px solid rgba(255, 255, 255, 0.05);
}

.gender-header {
  display: flex;
  justify-content: space-between;
  font-size: 11px;
  margin-bottom: 6px;
}

.gender-male-text {
  color: #22d3ee;
  font-weight: 600;
}

.gender-female-text {
  color: #f472b6;
  font-weight: 600;
}

.gender-bar {
  display: flex;
  height: 8px;
  border-radius: 4px;
  overflow: hidden;
  background: #1e293b;
}

.gender-male {
  background: linear-gradient(90deg, #06b6d4, #38bdf8);
  box-shadow: 0 0 8px rgba(6, 182, 212, 0.5);
  transition: width 0.5s ease;
}

.gender-female {
  background: linear-gradient(90deg, #f472b6, #ec4899);
  box-shadow: 0 0 8px rgba(236, 72, 153, 0.5);
  transition: width 0.5s ease;
}

.age-distribution-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-top: 8px;
  padding: 8px 12px;
  background: rgba(7, 11, 20, 0.7);
  border-radius: 10px;
  border: 1px solid rgba(255, 255, 255, 0.05);
}

.age-label {
  font-size: 11px;
  color: #94a3b8;
  margin-right: 6px;
}

.age-tag {
  background: rgba(6, 182, 212, 0.15) !important;
  color: #22d3ee !important;
  border: 1px solid rgba(6, 182, 212, 0.4) !important;
}

.tag-hint {
  font-size: 9px;
  color: #64748b;
}

/* 切片回放入口卡片 */
.playback-entry-card {
  cursor: pointer;
  background: linear-gradient(135deg, rgba(30, 58, 138, 0.25) 0%, rgba(15, 23, 42, 0.85) 100%);
  border: 1px solid rgba(56, 189, 248, 0.25);
  transition: all 0.25s ease;
}

.playback-entry-card:active {
  transform: scale(0.98);
}

.entry-flex {
  display: flex;
  align-items: center;
  gap: 12px;
}

.entry-icon-box {
  width: 40px;
  height: 40px;
  border-radius: 10px;
  background: rgba(0, 242, 254, 0.12);
  border: 1px solid rgba(0, 242, 254, 0.3);
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 20px;
  box-shadow: 0 0 14px rgba(0, 242, 254, 0.2);
}

.entry-info {
  flex: 1;
}

.entry-title {
  font-size: 13px;
  font-weight: 700;
  color: #f8fafc;
  margin-bottom: 2px;
}

.entry-desc {
  font-size: 10px;
  color: #94a3b8;
  line-height: 1.4;
}

.btn-go-playback {
  font-size: 10px;
  font-weight: 700;
  color: #00f2fe;
  background: rgba(0, 242, 254, 0.1);
  padding: 5px 10px;
  border-radius: 14px;
  border: 1px solid rgba(0, 242, 254, 0.3);
}

/* 遥测仪器 */
.telemetry-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 8px;
  margin-top: 8px;
}

.tele-item {
  background: rgba(7, 11, 20, 0.8);
  border: 1px solid rgba(255, 255, 255, 0.05);
  border-radius: 8px;
  padding: 8px 10px;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.tele-k {
  font-size: 9px;
  color: #64748b;
}

.tele-v {
  font-size: 12px;
  font-weight: 700;
  font-family: "DIN Alternate", -apple-system, sans-serif;
}

.text-cyan { color: #00f2fe; }
.text-emerald { color: #10b981; }
.text-amber { color: #f59e0b; }
.text-violet { color: #a78bfa; }

/* 0 点位空状态卡片 */
.no-points-container {
  padding: 60px 20px;
  display: flex;
  justify-content: center;
  align-items: center;
}

.empty-desc-bold {
  font-size: 15px;
  font-weight: 600;
  color: #e2e8f0;
  margin-bottom: 4px;
}

.empty-desc-sub {
  font-size: 12px;
  color: #64748b;
  line-height: 1.5;
  margin-bottom: 14px;
}

.refresh-btn {
  padding: 0 20px;
}
</style>
