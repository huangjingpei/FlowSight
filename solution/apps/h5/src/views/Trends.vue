<template>
  <div class="trends-viewport">
    <!-- 1. 顶部嵌入式标题栏 -->
    <header class="flat-header">
      <div class="header-left">
        <span class="header-dot"></span>
        <span class="header-title">时序走势分析</span>
      </div>
      <div class="realtime-badge">
        <span>实时时序流</span>
      </div>
    </header>

    <!-- 2. 主体：单屏纯平走势大屏 (无多余圈套，图表自适应填充) -->
    <main class="flat-main">
      <!-- 行 1: 通道选择条 -->
      <div class="flat-point-bar">
        <span class="point-label">监控通道:</span>
        <div class="point-select-wrap">
          <select v-model="activePointId" class="flat-select" @change="onPointChange">
            <option v-for="p in pointOptions" :key="p.value" :value="p.value">
              {{ p.text }}
            </option>
          </select>
        </div>
      </div>

      <!-- 行 2: 5 维扁平统计时间粒度切换器 -->
      <div class="flat-segmented">
        <div
          v-for="dim in dimensionOptions"
          :key="dim.key"
          :class="['seg-btn', currentDimension === dim.key ? 'active' : '']"
          @click="switchDimension(dim.key, dim.days)"
        >
          {{ dim.label }}
        </div>
      </div>

      <!-- 行 3: 3 项核心指标平铺卡片 -->
      <div class="flat-kpi-row">
        <div class="kpi-cell">
          <span class="cell-label">周期总流</span>
          <span class="cell-val text-white">{{ trendData.totalPeriodCount }}<span class="cell-unit">人次</span></span>
        </div>
        <div class="kpi-cell">
          <span class="cell-label">峰值最高</span>
          <span class="cell-val text-cyan">{{ trendData.peakTime }} ({{ trendData.peakValue }}人)</span>
        </div>
        <div class="kpi-cell">
          <span class="cell-label">时序均值</span>
          <span class="cell-val text-green">{{ trendData.averageCount || '0.0' }}<span class="cell-unit">人/点</span></span>
        </div>
      </div>

      <!-- 行 4: 扁平高对比度 ECharts 图表容器 (填满剩余高度) -->
      <div class="chart-box">
        <div class="chart-legend-row">
          <span class="legend-title">通行时序曲线 (采集时间动态流)</span>
          <div class="legend-items">
            <span class="leg-item"><i class="leg-dot cyan"></i>进入</span>
            <span class="leg-item"><i class="leg-dot pink"></i>离开</span>
          </div>
        </div>
        <div ref="chartRef" class="echarts-container"></div>
      </div>
    </main>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue';
import * as echarts from 'echarts';
import { h5Api, TrendReportVO } from '../api';
import { usePointStore } from '../store/point';

const pointStore = usePointStore();
const activePointId = ref<number>(0);

const dimensionOptions = [
  { key: 'realtime', label: '实时时序', days: 1 },
  { key: 'hour', label: '今日逐时', days: 1 },
  { key: 'day7', label: '近7天', days: 7 },
  { key: 'day30', label: '近30天', days: 30 },
  { key: 'quarter', label: '季度走势', days: 90 }
];
const currentDimension = ref<string>('realtime');
const currentDays = ref<number>(1);

const trendData = ref<TrendReportVO>({
  dimension: 'realtime',
  title: '今日实时通行时序曲线',
  categories: [],
  entryList: [],
  exitList: [],
  totalList: [],
  peakValue: 0,
  peakTime: '-',
  totalPeriodCount: 0,
  averageCount: 0.0
});

const chartRef = ref<HTMLDivElement | null>(null);
let myChart: echarts.ECharts | null = null;
let timer: any = null;

const pointOptions = computed(() => {
  return pointStore.points.map((p) => ({
    text: p.pointName,
    value: p.id
  }));
});

const loadPoints = async () => {
  try {
    const list: any = await h5Api.getMyPoints();
    pointStore.setPoints(list);
    if (list.length > 0 && !activePointId.value) {
      activePointId.value = list[0].id;
    }
  } catch (e) {}
};

const loadTrends = async () => {
  if (!activePointId.value) return;
  try {
    let dim = currentDimension.value;
    if (dim.startsWith('day')) dim = 'day';
    const data: any = await h5Api.getTrends(activePointId.value, dim, currentDays.value);
    trendData.value = data;
    renderECharts();
  } catch (e) {}
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
      backgroundColor: '#182030',
      borderColor: '#2A364F',
      borderWidth: 1,
      padding: [8, 12],
      textStyle: { color: '#FFFFFF', fontSize: 12, fontWeight: 600 },
      axisPointer: { type: isBar ? 'shadow' : 'line', lineStyle: { color: '#00D2FF', width: 1.5, type: 'dashed' } }
    },
    grid: {
      left: '4%',
      right: '8%',
      top: '16%',
      bottom: '12%',
      containLabel: true
    },
    xAxis: {
      type: 'category',
      name: '时间',
      nameLocation: 'end',
      nameTextStyle: { color: '#94A3B8', fontSize: 11, padding: [0, 0, 0, 4] },
      data: categories,
      boundaryGap: isBar,
      axisLine: {
        show: true,
        symbol: ['none', 'arrow'],
        symbolSize: [6, 10],
        symbolOffset: [0, 8],
        lineStyle: { color: '#475569', width: 1.5 }
      },
      axisTick: {
        show: true,
        alignWithLabel: true,
        lineStyle: { color: '#475569' }
      },
      splitLine: {
        show: true,
        lineStyle: { color: '#1A2230', type: 'dashed' }
      },
      axisLabel: { color: '#94A3B8', fontSize: 10, interval: categories.length > 14 ? 1 : 0 }
    },
    yAxis: {
      type: 'value',
      name: '人次 (人)',
      nameLocation: 'end',
      nameTextStyle: { color: '#94A3B8', fontSize: 11, padding: [0, 0, 6, 0] },
      minInterval: 1,
      axisLine: {
        show: true,
        symbol: ['none', 'arrow'],
        symbolSize: [6, 10],
        symbolOffset: [0, 8],
        lineStyle: { color: '#475569', width: 1.5 }
      },
      axisTick: {
        show: true,
        lineStyle: { color: '#475569' }
      },
      splitLine: {
        show: true,
        lineStyle: { color: '#1F293D', type: 'dashed' }
      },
      axisLabel: { color: '#94A3B8', fontSize: 10 }
    },
    series: [
      {
        name: '进入',
        type: isBar ? 'bar' : 'line',
        smooth: 0.3,
        showSymbol: categories.length <= 15,
        symbolSize: 5,
        itemStyle: { color: '#00D2FF' },
        lineStyle: { width: 2.5 },
        areaStyle: isBar ? undefined : {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(0, 210, 255, 0.25)' },
            { offset: 1, color: 'rgba(0, 210, 255, 0.00)' }
          ])
        },
        data: entryData
      },
      {
        name: '离开',
        type: isBar ? 'bar' : 'line',
        smooth: 0.3,
        showSymbol: categories.length <= 15,
        symbolSize: 5,
        itemStyle: { color: '#FF3D71' },
        lineStyle: { width: 2.5 },
        areaStyle: isBar ? undefined : {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(255, 61, 113, 0.25)' },
            { offset: 1, color: 'rgba(255, 61, 113, 0.00)' }
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

const onPointChange = () => {
  loadTrends();
};

const handleResize = () => {
  if (myChart) myChart.resize();
};

onMounted(async () => {
  await loadPoints();
  if (activePointId.value) {
    await loadTrends();
  }
  window.addEventListener('resize', handleResize);
  timer = setInterval(loadTrends, 10000);
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
/* 视图严格铺满单屏，无滚动条 */
.trends-viewport {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  box-sizing: border-box;
  background-color: #0A0D14;
}

/* 顶部标题栏 */
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

.header-title {
  font-size: 14px;
  font-weight: 700;
  color: #FFFFFF;
}

.realtime-badge {
  font-size: 10px;
  font-weight: 700;
  color: #00D2FF;
  background: rgba(0, 210, 255, 0.12);
  padding: 2px 8px;
  border-radius: 4px;
}

/* 主体单屏 */
.flat-main {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  padding: 8px 10px;
  box-sizing: border-box;
  overflow: hidden;
  gap: 8px;
}

/* 点位栏 */
.flat-point-bar {
  flex-shrink: 0;
  height: 36px;
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 0 10px;
  display: flex;
  align-items: center;
  gap: 8px;
}

.point-label {
  font-size: 12px;
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
  background: transparent;
  color: #FFFFFF;
  border: none;
  font-size: 12px;
  font-weight: 700;
  outline: none;
}

/* 5 维分段选择 */
.flat-segmented {
  flex-shrink: 0;
  height: 32px;
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  display: grid;
  grid-template-columns: repeat(5, 1fr);
  padding: 2px;
  gap: 2px;
  box-sizing: border-box;
}

.seg-btn {
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 11px;
  font-weight: 700;
  color: #94A3B8;
  border-radius: 4px;
  cursor: pointer;
  transition: all 0.15s ease;
}

.seg-btn.active {
  background: #00D2FF;
  color: #0A0D14;
}

/* 3 项核心摘要 */
.flat-kpi-row {
  flex-shrink: 0;
  height: 50px;
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 6px;
}

.kpi-cell {
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 6px 8px;
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 2px;
  text-align: center;
}

.cell-label {
  font-size: 10px;
  color: #64748B;
  font-weight: 600;
}

.cell-val {
  font-size: 12px;
  font-weight: 800;
}

.cell-unit {
  font-size: 10px;
  font-weight: normal;
  color: #64748B;
}

.text-white { color: #FFFFFF; }
.text-cyan { color: #00D2FF; }
.text-green { color: #00E676; }

/* 图表容器 */
.chart-box {
  flex: 1;
  min-height: 0;
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 8px 8px 4px 8px;
  display: flex;
  flex-direction: column;
}

.chart-legend-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 4px;
}

.legend-title {
  font-size: 11px;
  font-weight: 700;
  color: #CBD5E1;
}

.legend-items {
  display: flex;
  align-items: center;
  gap: 8px;
}

.leg-item {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 10px;
  font-weight: 600;
  color: #94A3B8;
}

.leg-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
}
.leg-dot.cyan { background: #00D2FF; }
.leg-dot.pink { background: #FF3D71; }

.echarts-container {
  flex: 1;
  min-height: 0;
  width: 100%;
}
</style>