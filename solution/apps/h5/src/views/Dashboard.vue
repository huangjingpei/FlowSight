<template>
  <div class="dashboard-page">
    <!-- 顶部政务物业决策大屏导航 -->
    <van-nav-bar fixed placeholder class="gov-nav-bar">
      <template #left>
        <div class="nav-brand">
          <span class="brand-logo">👁️</span>
          <span class="brand-title">{{ currentCompanyName }}</span>
        </div>
      </template>
      <template #right>
        <van-icon name="revoke" size="18" class="logout-icon" @click="handleLogout" />
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

    <!-- 正常已配置点位的大屏内容 -->
    <template v-else>
      <!-- 顶部点位选择器: 视频播放中硬锁定禁止切换 -->
      <div class="point-switcher-bar">
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

    <!-- 客户端节点身份与健康状态铭牌 -->
    <div class="client-meta-bar" v-if="summary.pointId">
      <div class="client-info">
        <span class="client-icon">💻</span>
        <span class="client-name">{{ summary.clientName || ('FlowSight-Node-' + summary.pointId) }}</span>
        <span class="client-version">Ver: {{ summary.appVersion || 'v1.2.0' }}</span>
      </div>
      <div class="client-status">
        <span :class="['status-dot', summary.streamStatus === 'ONLINE' ? 'online' : 'offline']"></span>
        <span class="status-text">{{ summary.streamStatus === 'ONLINE' ? '实时在线' : '离线' }}</span>
        <span class="last-hb" v-if="summary.lastHeartbeat">· {{ summary.lastHeartbeat.split(' ')[1] || summary.lastHeartbeat }}</span>
      </div>
    </div>

    <!-- 承载安全与超载防聚集监测条 -->
    <div :class="['safety-banner', summary.currentOccupancy > 30 ? 'safety-warning' : 'safety-normal']">
      <span class="safety-icon">{{ summary.currentOccupancy > 30 ? '⚠️' : '🛡️' }}</span>
      <span class="safety-text">
        {{ summary.currentOccupancy > 30 ? '当前区域人员聚集密度较高，请关注通道畅通度与安全疏导' : '当前区域人流承载与通行状态正常' }}
      </span>
    </div>

    <van-pull-refresh v-model="refreshing" @refresh="onRefresh">
      <!-- 4 大核心客流数显矩阵 -->
      <div class="kpi-grid">
        <div class="kpi-card card-blue">
          <div class="kpi-header">
            <span class="kpi-label">当前区域滞留</span>
            <span class="kpi-badge">● 实时在场</span>
          </div>
          <div class="kpi-value">{{ summary.currentOccupancy }} <span class="unit">人</span></div>
          <div class="kpi-sub">瞬时在场感知与安全基线</div>
        </div>
        <div class="kpi-card card-emerald">
          <div class="kpi-header">
            <span class="kpi-label">今日累计进入 (In)</span>
            <span class="kpi-badge">↑ 通行增量</span>
          </div>
          <div class="kpi-value">{{ summary.todayEntry }} <span class="unit">人次</span></div>
          <div class="kpi-sub">主入口进门累计总人次</div>
        </div>
        <div class="kpi-card card-amber">
          <div class="kpi-header">
            <span class="kpi-label">今日累计离开 (Out)</span>
            <span class="kpi-badge">↓ 出门离场</span>
          </div>
          <div class="kpi-value">{{ summary.todayExit }} <span class="unit">人次</span></div>
          <div class="kpi-sub">出口闸道离开累计总人次</div>
        </div>
        <div class="kpi-card card-indigo">
          <div class="kpi-header">
            <span class="kpi-label">全天通行总人流</span>
            <span class="kpi-badge">∑ 双向汇总</span>
          </div>
          <div class="kpi-value">{{ summary.todayTotal }} <span class="unit">人次</span></div>
          <div class="kpi-sub">全天双向吞吐客流总量</div>
        </div>
      </div>

      <!-- 核心多维统计报表模块 (按小时、按天、按月、按季度) -->
      <div class="section-card trend-section">
        <div class="section-header">
          <div class="section-title">
            <span>📈</span> 客流趋势全景分析与决策曲线
          </div>
        </div>

        <!-- 统计粒度切换标签 -->
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

        <!-- 统计核心指标摘要药丸 -->
        <div class="trend-summary-pills">
          <div class="pill-item">
            <span class="pill-lbl">周期总客流</span>
            <span class="pill-val">{{ trendData.totalPeriodCount || summary.todayTotal }}</span>
          </div>
          <div class="pill-item">
            <span class="pill-lbl">峰值时段</span>
            <span class="pill-val peak-highlight">{{ trendData.peakTime }} ({{ trendData.peakValue }}人)</span>
          </div>
          <div class="pill-item">
            <span class="pill-lbl">区间均值</span>
            <span class="pill-val">{{ trendData.averageCount || '-' }}</span>
          </div>
        </div>

        <!-- 高保真交互曲线图容器 -->
        <div ref="chartRef" class="echarts-container"></div>
      </div>

      <!-- 客群属性画像模块: 性别比例与主要年龄段 -->
      <div class="section-card">
        <div class="section-title"><span>👥</span> 客群画像与结构特征</div>
        <div class="demo-box" v-if="summary.demographics">
          <div class="gender-bar-wrapper">
            <div class="gender-header">
              <span class="gender-male-text">
                👨 男性: {{ (summary.demographics.maleRatio * 100).toFixed(1) }}% ({{ summary.demographics.maleCount }}人)
              </span>
              <span class="gender-female-text">
                👩 女性: {{ (summary.demographics.femaleRatio * 100).toFixed(1) }}% ({{ summary.demographics.femaleCount }}人)
              </span>
            </div>
            <div class="gender-bar">
              <div class="gender-male" :style="{ width: (summary.demographics.maleRatio * 100) + '%' }"></div>
              <div class="gender-female" :style="{ width: (summary.demographics.femaleRatio * 100) + '%' }"></div>
            </div>
          </div>
          <div class="age-distribution-row">
            <div class="age-tag-box">
              <span class="age-label">主要到场客群年龄:</span>
              <van-tag type="primary" size="medium" class="age-tag">{{ summary.demographics.topAgeGroup || '青年(18-35)' }}</van-tag>
            </div>
            <div class="tag-hint">基于边缘 NPU 属性多目标智能分析</div>
          </div>
        </div>
      </div>

      <!-- 证据切片回放中心快速入口 -->
      <div class="section-card playback-entry-card" @click="goToPlayback">
        <div class="entry-flex">
          <div class="entry-icon-box">🎬</div>
          <div class="entry-info">
            <div class="entry-title">通行切片证据中心与视频回放</div>
            <div class="entry-desc">支持多通道检索、日期范围筛选、多页翻页与高清切片视频复核</div>
          </div>
          <div class="entry-arrow">
            <span class="btn-go-playback">进入回放 ➔</span>
          </div>
        </div>
      </div>

      <!-- 边缘设备与系统运行遥测 -->
      <div class="section-card telemetry-card">
        <div class="section-title"><span>⚡</span> 边端硬件与算法遥测指标</div>
        <div class="telemetry-grid">
          <div class="tele-item">
            <span class="tele-k">AI 推理框架</span>
            <span class="tele-v text-blue">YOLOv8n (ORT C++)</span>
          </div>
          <div class="tele-item">
            <span class="tele-k">实时处理帧率</span>
            <span class="tele-v text-emerald">30.0 FPS</span>
          </div>
          <div class="tele-item">
            <span class="tele-k">单帧推理耗时</span>
            <span class="tele-v text-cyan">14.2 ms</span>
          </div>
          <div class="tele-item">
            <span class="tele-k">边端离线队列</span>
            <span class="tele-v text-amber">0 条 (实时同步)</span>
          </div>
        </div>
      </div>
    </van-pull-refresh>
    </template>
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
const currentCompanyName = ref(currentUser.companyName || currentUser.nickname || 'FlowSight 决策中心');

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

// 多粒度趋势维度选择
const dimensionOptions = [
  { key: 'hour', label: '24小时逐时', days: 1 },
  { key: 'day_7', label: '近7天走势', days: 7 },
  { key: 'day_30', label: '近30天走势', days: 30 },
  { key: 'quarter', label: '季度宏观走势', days: 90 }
];
const currentDimension = ref('hour');
const currentDays = ref(1);

const trendData = ref<any>({
  dimension: 'hour',
  title: '今日 24 小时逐时客流分布曲线',
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
    const dim = currentDimension.value === 'quarter' ? 'quarter' : (currentDimension.value.startsWith('day') ? 'day' : 'hour');
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
      backgroundColor: 'rgba(15, 23, 42, 0.9)',
      borderColor: '#38bdf8',
      textStyle: { color: '#f8fafc', fontSize: 12 },
      axisPointer: { type: isBar ? 'shadow' : 'line', lineStyle: { color: '#38bdf8', type: 'dashed' } }
    },
    legend: {
      data: ['进入人次 (In)', '离开人次 (Out)'],
      bottom: 0,
      icon: 'circle',
      textStyle: { color: '#64748b', fontSize: 11 }
    },
    grid: {
      left: '3%',
      right: '4%',
      top: '12%',
      bottom: '15%',
      containLabel: true
    },
    xAxis: {
      type: 'category',
      data: categories,
      axisLine: { lineStyle: { color: '#cbd5e1' } },
      axisLabel: { color: '#64748b', fontSize: 10, interval: categories.length > 15 ? 2 : 0 }
    },
    yAxis: {
      type: 'value',
      splitLine: { lineStyle: { color: '#f1f5f9', type: 'dashed' } },
      axisLabel: { color: '#94a3b8', fontSize: 10 }
    },
    series: [
      {
        name: '进入人次 (In)',
        type: isBar ? 'bar' : 'line',
        smooth: true,
        showSymbol: false,
        itemStyle: { color: '#2563eb' },
        areaStyle: isBar ? undefined : {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(37, 99, 235, 0.35)' },
            { offset: 1, color: 'rgba(37, 99, 235, 0.02)' }
          ])
        },
        data: entryData,
        markPoint: !isBar ? {
          data: [{ type: 'max', name: '最高峰值' }],
          symbolSize: 45,
          label: { fontSize: 9 }
        } : undefined
      },
      {
        name: '离开人次 (Out)',
        type: isBar ? 'bar' : 'line',
        smooth: true,
        showSymbol: false,
        itemStyle: { color: '#f59e0b' },
        areaStyle: isBar ? undefined : {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(245, 158, 11, 0.30)' },
            { offset: 1, color: 'rgba(245, 158, 11, 0.02)' }
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
  if (pointStore.isMediaPlaying) {
    showToast('视频回放中禁止切换点位！');
    return;
  }
  activePointId.value = val;
  loadSummary();
  loadTrends();
};

const onRefresh = async () => {
  await Promise.all([loadSummary(), loadTrends()]);
  refreshing.value = false;
  showToast('数据已刷新');
};

const handleLogout = () => {
  showDialog({
    title: '提示',
    message: '确认安全退出当前商户账号？',
    showCancelButton: true
  }).then(() => {
    localStorage.removeItem('flowsight_token');
    localStorage.removeItem('flowsight_user');
    router.replace('/login');
  });
};

const handleResize = () => {
  if (myChart) myChart.resize();
};

onMounted(async () => {
  await loadPoints();
  if (activePointId.value) {
    await Promise.all([loadSummary(), loadTrends()]);
  }

  nextTick(() => {
    window.addEventListener('resize', handleResize);
  });

  // 15 秒定时轮询最新数据
  timer = setInterval(() => {
    if (!pointStore.isMediaPlaying && activePointId.value) {
      loadSummary();
      loadTrends();
    }
  }, 15000);
});

onUnmounted(() => {
  if (timer) clearInterval(timer);
  window.removeEventListener('resize', handleResize);
  if (myChart) {
    myChart.dispose();
    myChart = null;
  }
});
</script>

<style scoped>
.dashboard-page {
  padding-bottom: 30px;
  background-color: #f8fafc;
  min-height: 100vh;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
}

/* 顶部政务导航 */
.gov-nav-bar {
  background: #0f172a !important;
  color: #fff !important;
}

:deep(.van-nav-bar__content) {
  background: #0f172a;
}

.nav-brand {
  display: flex;
  align-items: center;
  gap: 8px;
}

.brand-logo {
  font-size: 18px;
}

.brand-title {
  font-size: 15px;
  font-weight: 600;
  color: #f8fafc;
  letter-spacing: 0.5px;
}

.logout-icon {
  color: #94a3b8;
  cursor: pointer;
}

/* 顶部点位选择器 */
.point-switcher-bar {
  background: #fff;
  border-bottom: 1px solid #e2e8f0;
  position: relative;
}

.locked-tip {
  padding: 5px 16px;
  font-size: 11px;
  color: #d97706;
  background-color: #fffbeb;
  display: flex;
  align-items: center;
}

/* 客户端节点身份铭牌 */
.client-meta-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 8px 14px;
  background-color: #f1f5f9;
  border-bottom: 1px solid #e2e8f0;
  font-size: 12px;
}

.client-info {
  display: flex;
  align-items: center;
  gap: 6px;
  color: #1e293b;
}

.client-name {
  font-weight: 600;
  color: #2563eb;
}

.client-version {
  font-size: 10px;
  color: #64748b;
  background: #e2e8f0;
  padding: 1px 5px;
  border-radius: 4px;
}

.client-status {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 11px;
}

.status-dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  display: inline-block;
}

.status-dot.online {
  background-color: #10b981;
  box-shadow: 0 0 6px #10b981;
}

.status-dot.offline {
  background-color: #94a3b8;
}

.status-text {
  color: #475569;
}

.last-hb {
  color: #94a3b8;
  font-size: 10px;
}

/* 安全承载状态条 */
.safety-banner {
  display: flex;
  align-items: center;
  gap: 8px;
  margin: 10px 12px 2px 12px;
  padding: 8px 12px;
  border-radius: 8px;
  font-size: 12px;
}

.safety-normal {
  background-color: #ecfdf5;
  color: #065f46;
  border: 1px solid #a7f3d0;
}

.safety-warning {
  background-color: #fef2f2;
  color: #991b1b;
  border: 1px solid #fecaca;
  font-weight: 500;
}

/* KPI 4 格数显卡片矩阵 */
.kpi-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
  padding: 10px 12px;
}

.kpi-card {
  border-radius: 10px;
  padding: 12px 12px;
  color: #fff;
  box-shadow: 0 4px 12px rgba(15, 23, 42, 0.08);
  display: flex;
  flex-direction: column;
}

.card-blue {
  background: linear-gradient(135deg, #1e40af, #2563eb);
}
.card-emerald {
  background: linear-gradient(135deg, #065f46, #059669);
}
.card-amber {
  background: linear-gradient(135deg, #b45309, #d97706);
}
.card-indigo {
  background: linear-gradient(135deg, #3730a3, #4f46e5);
}

.kpi-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 6px;
}

.kpi-label {
  font-size: 11px;
  opacity: 0.9;
}

.kpi-badge {
  font-size: 9px;
  background: rgba(255, 255, 255, 0.2);
  padding: 1px 5px;
  border-radius: 4px;
}

.kpi-value {
  font-size: 24px;
  font-weight: 700;
  font-family: 'Consolas', monospace;
  line-height: 1.2;
}

.kpi-value .unit {
  font-size: 11px;
  font-weight: normal;
}

.kpi-sub {
  font-size: 10px;
  opacity: 0.8;
  margin-top: 4px;
}

/* 核心模块卡片通用规范 */
.section-card {
  background: #fff;
  border-radius: 12px;
  padding: 14px 14px;
  margin: 10px 12px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.04);
  border: 1px solid #e2e8f0;
}

.section-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 10px;
}

.section-title {
  font-size: 14px;
  font-weight: 600;
  color: #0f172a;
  display: flex;
  align-items: center;
  gap: 6px;
}

/* 多粒度分段切换组件 */
.dimension-segmented {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  background-color: #f1f5f9;
  padding: 3px;
  border-radius: 8px;
  margin-bottom: 12px;
}

.seg-item {
  text-align: center;
  padding: 6px 0;
  font-size: 11px;
  color: #64748b;
  border-radius: 6px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.seg-item.active {
  background-color: #fff;
  color: #1e40af;
  font-weight: 600;
  box-shadow: 0 1px 4px rgba(0, 0, 0, 0.08);
}

/* 趋势摘要药丸 */
.trend-summary-pills {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 6px;
  background-color: #f8fafc;
  padding: 8px 10px;
  border-radius: 8px;
  border: 1px solid #e2e8f0;
  margin-bottom: 10px;
}

.pill-item {
  display: flex;
  flex-direction: column;
}

.pill-lbl {
  font-size: 10px;
  color: #94a3b8;
}

.pill-val {
  font-size: 12px;
  font-weight: 600;
  color: #1e293b;
  margin-top: 2px;
}

.peak-highlight {
  color: #2563eb;
}

.echarts-container {
  width: 100%;
  height: 240px;
}

/* 客群画像 */
.gender-bar-wrapper {
  margin-top: 8px;
}

.gender-header {
  display: flex;
  justify-content: space-between;
  font-size: 11px;
  margin-bottom: 6px;
}

.gender-male-text {
  color: #2563eb;
  font-weight: 500;
}

.gender-female-text {
  color: #f43f5e;
  font-weight: 500;
}

.gender-bar {
  display: flex;
  height: 10px;
  border-radius: 5px;
  overflow: hidden;
  background-color: #e2e8f0;
}

.gender-male {
  background: linear-gradient(90deg, #3b82f6, #2563eb);
}

.gender-female {
  background: linear-gradient(90deg, #fb7185, #f43f5e);
}

.age-distribution-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-top: 10px;
  padding-top: 8px;
  border-top: 1px dashed #f1f5f9;
}

.age-tag-box {
  display: flex;
  align-items: center;
  gap: 8px;
}

.age-label {
  font-size: 12px;
  color: #475569;
}

.age-tag {
  font-size: 11px;
}

.tag-hint {
  font-size: 10px;
  color: #94a3b8;
}

/* 快捷切片入口横幅 */
.playback-entry-card {
  background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%);
  border: 1px solid #3b82f64d;
  cursor: pointer;
  transition: all 0.2s;
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
  font-size: 26px;
  background: #3b82f626;
  width: 44px;
  height: 44px;
  border-radius: 22px;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}

.entry-info {
  flex: 1;
}

.entry-title {
  color: #f8fafc;
  font-size: 14px;
  font-weight: 700;
  margin-bottom: 3px;
}

.entry-desc {
  color: #94a3b8;
  font-size: 11px;
  line-height: 1.4;
}

.btn-go-playback {
  color: #38bdf8;
  font-size: 12px;
  font-weight: bold;
  white-space: nowrap;
}

/* 遥测指标卡片 */
.telemetry-card {
  padding-bottom: 12px;
}

.telemetry-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 10px;
}

.tele-item {
  background: #f8fafc;
  border: 1px solid #e2e8f0;
  border-radius: 6px;
  padding: 8px 10px;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.tele-k {
  font-size: 11px;
  color: #64748b;
}

.tele-v {
  font-size: 13px;
  font-weight: bold;
}

.text-blue { color: #2563eb; }
.text-emerald { color: #059669; }
.text-cyan { color: #0284c7; }
.text-amber { color: #d97706; }

.no-points-container {
  padding: 60px 20px;
  display: flex;
  justify-content: center;
  align-items: center;
}

.empty-desc-bold {
  font-size: 16px;
  font-weight: 600;
  color: #1e293b;
  margin-bottom: 6px;
}

.empty-desc-sub {
  font-size: 13px;
  color: #64748b;
  line-height: 1.5;
  margin-bottom: 16px;
}

.refresh-btn {
  padding: 0 24px;
}
</style>
