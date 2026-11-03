<template>
  <div class="trends-viewport">
    <!-- 1. 顶部嵌入式标题栏 -->
    <header class="flat-header">
      <div class="header-left">
        <span class="header-dot"></span>
        <span class="header-title">时序走势分析</span>
      </div>
      <div class="header-right">
        <div class="realtime-badge">
          <span>实时时序流</span>
        </div>
        <button
          class="flat-export-btn"
          :disabled="isExporting"
          @click="handleExportExcel"
          :title="isVehicleMode ? '导出车流时序Excel报表并可分享到微信' : '导出客流时序Excel报表并可分享到微信'"
        >
          <span v-if="isExporting">导出中...</span>
          <span v-else>📥 导出报表</span>
        </button>
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
          <span class="cell-val text-white">{{ trendData.totalPeriodCount }}<span class="cell-unit">{{ isVehicleMode ? '车次' : '人次' }}</span></span>
        </div>
        <div class="kpi-cell">
          <span class="cell-label">峰值最高</span>
          <span class="cell-val text-cyan">{{ trendData.peakTime }} ({{ trendData.peakValue }} {{ isVehicleMode ? '辆' : '人' }})</span>
        </div>
        <div class="kpi-cell">
          <span class="cell-label">时序均值</span>
          <span class="cell-val text-green">{{ trendData.averageCount || '0.0' }}<span class="cell-unit">{{ isVehicleMode ? '车/点' : '人/点' }}</span></span>
        </div>
      </div>

      <!-- 行 4: 扁平高对比度 ECharts 图表容器 (填满剩余高度) -->
      <div class="chart-box">
        <div class="chart-legend-row">
          <span class="legend-title">{{ trendData.title || (isVehicleMode ? '通行时序曲线 (车流动态流)' : '通行时序曲线 (客流动态流)') }}</span>
          <div class="legend-items">
            <span class="leg-item"><i class="leg-dot cyan"></i>{{ isVehicleMode ? '进车' : '进入' }}</span>
            <span class="leg-item"><i class="leg-dot pink"></i>{{ isVehicleMode ? '出车' : '离开' }}</span>
          </div>
        </div>
        <div ref="chartRef" class="echarts-container"></div>
      </div>
    </main>

    <!-- 移动端导出完成与微信转发弹窗 -->
    <div v-if="showExportModal" class="export-modal-mask" @click.self="showExportModal = false">
      <div class="export-modal-card">
        <div class="modal-header">
          <div class="modal-title-row">
            <span class="modal-icon">📊</span>
            <span class="modal-title">报表已生成并保存</span>
          </div>
          <button class="modal-close-btn" @click="showExportModal = false">✕</button>
        </div>

        <div class="modal-body">
          <!-- 文件信息卡片 -->
          <div class="file-info-box">
            <div class="file-name-row">
              <span class="file-type-badge">{{ isVehicleMode ? '🚗 车流' : '🚶 客流' }}</span>
              <span class="file-name-text" :title="exportFileName">{{ exportFileName }}</span>
            </div>
            <div class="file-meta-row">
              <span>通道: {{ currentPointName }}</span>
              <span>格式: Excel (.xlsx)</span>
            </div>
          </div>

          <!-- 微信机制说明与发送操作步骤 -->
          <div class="wechat-guide-card">
            <div class="guide-title">
              <span class="guide-badge">微信发送指南</span>
              <span class="guide-sub">微信安全策略要求在聊天窗口中选择本地文件发送</span>
            </div>
            <div class="guide-steps">
              <div class="step-item">
                <span class="step-num">1</span>
                <span class="step-text">报表已自动下载至手机【下载 / 文件】目录</span>
              </div>
              <div class="step-item">
                <span class="step-num">2</span>
                <span class="step-text">进入微信任意好友、群聊或【文件传输助手】</span>
              </div>
              <div class="step-item">
                <span class="step-num">3</span>
                <span class="step-text">点击聊天框右下角 <b>【+】➜【文件】</b>，选择刚生成的表格发送</span>
              </div>
            </div>
          </div>

          <!-- 操作按钮矩阵 -->
          <div class="modal-actions-grid">
            <button class="action-btn wechat-btn" @click="handleOpenWeChat">
              <span class="btn-icon">💬</span>
              <span>快捷打开微信发送</span>
            </button>
            <button class="action-btn copy-btn" @click="handleCopySummary">
              <span class="btn-icon">📋</span>
              <span>复制数据快报文字</span>
            </button>
            <button class="action-btn download-btn" @click="handleReDownload">
              <span class="btn-icon">📥</span>
              <span>重新下载 Excel 文件</span>
            </button>
          </div>
        </div>

        <div class="modal-footer">
          <button class="confirm-btn" @click="showExportModal = false">我知道了</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, onActivated, nextTick, watch } from 'vue';
import * as echarts from 'echarts';
import * as XLSX from 'xlsx';
import { showToast, showDialog } from 'vant';
import { h5Api, TrendReportVO } from '../api';
import { usePointStore } from '../store/point';
import { useThemeStore } from '../store/theme';

const pointStore = usePointStore();
const themeStore = useThemeStore();
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

const isVehicleMode = computed(() => {
  if (trendData.value?.bizType) {
    return trendData.value.bizType === 'VEHICLE';
  }
  const pt = pointStore.points.find((p) => p.id === activePointId.value);
  return pt?.bizType === 'VEHICLE';
});

const pointOptions = computed(() => {
  return pointStore.points.map((p) => ({
    text: `[${p.id}] ${p.pointName} (${p.bizType === 'VEHICLE' ? '车流' : '客流'})`,
    value: p.id
  }));
});

const loadPoints = async () => {
  try {
    const list: any = await h5Api.getMyPoints();
    pointStore.setPoints(list);
    if (list.length > 0) {
      if (!activePointId.value || !list.some((p: any) => p.id === activePointId.value)) {
        activePointId.value = pointStore.activePointId || list[0].id;
      }
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

  const isLight = themeStore.currentTheme === 'light';
  const isBar = currentDimension.value === 'quarter';
  const categories = trendData.value.categories || [];
  const entryData = trendData.value.entryList || [];
  const exitData = trendData.value.exitList || [];

  const entryColor = isLight ? '#0284C7' : '#00D2FF';
  const exitColor = isLight ? '#E11D48' : '#FF3D71';

  const option: echarts.EChartsOption = {
    tooltip: {
      trigger: 'axis',
      backgroundColor: isLight ? '#FFFFFF' : '#182030',
      borderColor: isLight ? '#CBD5E1' : '#2A364F',
      borderWidth: 1,
      padding: [8, 12],
      textStyle: { color: isLight ? '#0F172A' : '#FFFFFF', fontSize: 12, fontWeight: 600 },
      axisPointer: { type: isBar ? 'shadow' : 'line', lineStyle: { color: entryColor, width: 1.5, type: 'dashed' } }
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
      nameTextStyle: { color: isLight ? '#64748B' : '#94A3B8', fontSize: 11, padding: [0, 0, 0, 4] },
      data: categories,
      boundaryGap: isBar,
      axisLine: {
        show: true,
        symbol: ['none', 'arrow'],
        symbolSize: [6, 10],
        symbolOffset: [0, 8],
        lineStyle: { color: isLight ? '#94A3B8' : '#475569', width: 1.5 }
      },
      axisTick: {
        show: true,
        alignWithLabel: true,
        lineStyle: { color: isLight ? '#94A3B8' : '#475569' }
      },
      splitLine: {
        show: true,
        lineStyle: { color: isLight ? '#F1F5F9' : '#1A2230', type: 'dashed' }
      },
      axisLabel: { color: isLight ? '#475569' : '#94A3B8', fontSize: 10, interval: categories.length > 14 ? 1 : 0 }
    },
    yAxis: {
      type: 'value',
      name: isVehicleMode.value ? '车次 (辆)' : '人次 (人)',
      nameLocation: 'end',
      nameTextStyle: { color: isLight ? '#64748B' : '#94A3B8', fontSize: 11, padding: [0, 0, 6, 0] },
      minInterval: 1,
      axisLine: {
        show: true,
        symbol: ['none', 'arrow'],
        symbolSize: [6, 10],
        symbolOffset: [0, 8],
        lineStyle: { color: isLight ? '#94A3B8' : '#475569', width: 1.5 }
      },
      axisTick: {
        show: true,
        lineStyle: { color: isLight ? '#94A3B8' : '#475569' }
      },
      splitLine: {
        show: true,
        lineStyle: { color: isLight ? '#F1F5F9' : '#1F293D', type: 'dashed' }
      },
      axisLabel: { color: isLight ? '#475569' : '#94A3B8', fontSize: 10 }
    },
    series: [
      {
        name: isVehicleMode.value ? '进车' : '进入',
        type: isBar ? 'bar' : 'line',
        smooth: 0.3,
        showSymbol: categories.length <= 15,
        symbolSize: 5,
        itemStyle: { color: entryColor },
        lineStyle: { width: 2.5 },
        areaStyle: isBar ? undefined : {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: isLight ? 'rgba(2, 132, 199, 0.28)' : 'rgba(0, 210, 255, 0.25)' },
            { offset: 1, color: isLight ? 'rgba(2, 132, 199, 0.00)' : 'rgba(0, 210, 255, 0.00)' }
          ])
        },
        data: entryData
      },
      {
        name: isVehicleMode.value ? '出车' : '离开',
        type: isBar ? 'bar' : 'line',
        smooth: 0.3,
        showSymbol: categories.length <= 15,
        symbolSize: 5,
        itemStyle: { color: exitColor },
        lineStyle: { width: 2.5 },
        areaStyle: isBar ? undefined : {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: isLight ? 'rgba(225, 29, 72, 0.22)' : 'rgba(255, 61, 113, 0.25)' },
            { offset: 1, color: isLight ? 'rgba(225, 29, 72, 0.00)' : 'rgba(255, 61, 113, 0.00)' }
          ])
        },
        data: exitData
      }
    ]
  };

  myChart.setOption(option, true);
};

watch(() => themeStore.currentTheme, () => {
  renderECharts();
});

const switchDimension = (key: string, days: number) => {
  currentDimension.value = key;
  currentDays.value = days;
  loadTrends();
};

const onPointChange = () => {
  loadTrends();
};

const isExporting = ref(false);
const showExportModal = ref(false);
const exportFileName = ref('');
const lastExportBlob = ref<Blob | null>(null);

const currentPointName = computed(() => {
  const curPoint = pointStore.points.find((p) => p.id === activePointId.value);
  return curPoint ? `[${curPoint.id}] ${curPoint.pointName}` : `通道#${activePointId.value}`;
});

const handleOpenWeChat = () => {
  showToast({ message: '正在调起微信...', duration: 1500 });
  setTimeout(() => {
    window.location.href = 'weixin://';
  }, 300);
};

const handleCopySummary = async () => {
  const isVeh = isVehicleMode.value;
  const dimObj = dimensionOptions.find((d) => d.key === currentDimension.value);
  const dimLabel = dimObj ? dimObj.label : currentDimension.value;
  const text = `【FlowSight ${isVeh ? '车流' : '客流'}时序走势快报】
通道/卡口: ${currentPointName.value}
统计维度: ${dimLabel}
周期总量: ${trendData.value.totalPeriodCount || 0} ${isVeh ? '车次' : '人次'}
峰值最高: ${trendData.value.peakTime || '-'} (${trendData.value.peakValue || 0} ${isVeh ? '辆' : '人'})
时序均值: ${trendData.value.averageCount || '0.0'} ${isVeh ? '车/点' : '人/点'}
（完整 Excel 数据表格已保存至手机本地文件，在微信聊天窗口点击【+】->【文件】即可发送）`;

  try {
    if (navigator.clipboard && navigator.clipboard.writeText) {
      await navigator.clipboard.writeText(text);
    } else {
      const textarea = document.createElement('textarea');
      textarea.value = text;
      document.body.appendChild(textarea);
      textarea.select();
      document.execCommand('copy');
      document.body.removeChild(textarea);
    }
    showToast({ type: 'success', message: '数据快报已复制，可直接粘贴到微信聊天' });
  } catch (e) {
    showToast({ message: '已准备快报内容' });
  }
};

const handleReDownload = () => {
  if (lastExportBlob.value && exportFileName.value) {
    const url = URL.createObjectURL(lastExportBlob.value);
    const a = document.createElement('a');
    a.href = url;
    a.download = exportFileName.value;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(() => URL.revokeObjectURL(url), 1000);
    showToast({ type: 'success', message: '已重新触发下载' });
  } else {
    handleExportExcel();
  }
};

const handleExportExcel = async () => {
  if (isExporting.value) return;
  isExporting.value = true;
  showToast({ type: 'loading', message: '正在生成报表...', duration: 0 });

  try {
    const curPoint = pointStore.points.find((p) => p.id === activePointId.value);
    const pointName = curPoint ? curPoint.pointName : `通道#${activePointId.value}`;
    const pointCode = curPoint?.pointCode || `P-${activePointId.value}`;
    const isVeh = isVehicleMode.value;
    const dimObj = dimensionOptions.find((d) => d.key === currentDimension.value);
    const dimLabel = dimObj ? dimObj.label : currentDimension.value;
    const now = new Date();
    const pad = (n: number) => (n < 10 ? '0' + n : '' + n);
    const nowStr = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())} ${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
    const fileTimestamp = `${now.getFullYear()}${pad(now.getMonth() + 1)}${pad(now.getDate())}_${pad(now.getHours())}${pad(now.getMinutes())}`;

    // 1. 组装 Sheet 1: 时序趋势汇总报表
    const sheet1Rows: any[][] = [];
    sheet1Rows.push([`FlowSight - ${isVeh ? '车流' : '客流'}时序走势分析报表`]);
    sheet1Rows.push([
      `监测${isVeh ? '卡口' : '通道'}: [${activePointId.value}] ${pointName}`,
      `编码: ${pointCode}`,
      `业务模式: ${isVeh ? '车流计数与感知' : '客流统计与感知'}`,
      `统计维度: ${dimLabel}`,
      `导出时间: ${nowStr}`
    ]);
    sheet1Rows.push([]); // 空行

    // KPI 汇总卡片行
    sheet1Rows.push([
      isVeh ? '周期总车流' : '周期总客流',
      isVeh ? '车流高峰时段' : '客流高峰时段',
      isVeh ? '峰值最高' : '峰值最高',
      isVeh ? '时序平均车流' : '时序平均客流'
    ]);
    sheet1Rows.push([
      `${trendData.value.totalPeriodCount || 0} ${isVeh ? '车次' : '人次'}`,
      trendData.value.peakTime || '-',
      `${trendData.value.peakValue || 0} ${isVeh ? '辆' : '人'}`,
      `${trendData.value.averageCount || '0.0'} ${isVeh ? '车/点' : '人/点'}`
    ]);
    sheet1Rows.push([]); // 空行

    // 时序明细数据表头
    if (isVeh) {
      sheet1Rows.push(['序号', '统计时段/分类', '进车车次 (车次)', '出车车次 (车次)', '双向总车流 (车次)', '净增车流 (车次)']);
    } else {
      sheet1Rows.push(['序号', '统计时段/分类', '进入人次 (人次)', '离开人次 (人次)', '双向总客流 (人次)', '净增客流 (人次)']);
    }

    const categories = trendData.value.categories || [];
    const entryList = trendData.value.entryList || [];
    const exitList = trendData.value.exitList || [];
    const totalList = trendData.value.totalList || [];

    categories.forEach((cat, idx) => {
      const entry = entryList[idx] || 0;
      const exit = exitList[idx] || 0;
      const total = totalList[idx] || (entry + exit);
      const net = entry - exit;
      sheet1Rows.push([idx + 1, cat, entry, exit, total, net]);
    });

    const ws1 = XLSX.utils.aoa_to_sheet(sheet1Rows);
    ws1['!cols'] = [
      { wch: 8 },  // 序号
      { wch: 22 }, // 时段/分类
      { wch: 18 }, // 进
      { wch: 18 }, // 出
      { wch: 20 }, // 总
      { wch: 16 }  // 净增
    ];

    // 2. 尝试抓取当前卡口/通道的最近采集记录 (Sheet 2)
    const sheet2Rows: any[][] = [];
    try {
      const eventsRes: any = await h5Api.getEvents({
        pointId: activePointId.value,
        page: 1,
        size: 500
      });
      const records = eventsRes?.records || [];
      if (records.length > 0) {
        if (isVeh) {
          sheet2Rows.push(['序号', '通行时间', '监测卡口编号', '卡口名称', '车辆跟踪ID', '通行方向', '置信度', '车型分类', '车身颜色', '车牌号码', '号牌颜色']);
          records.forEach((ev: any, idx: number) => {
            const dirStr = ev.direction === 'ENTRY' || ev.direction === '进' || ev.direction === '1' ? '进车 (IN)' : '出车 (OUT)';
            sheet2Rows.push([
              idx + 1,
              ev.datetime || ev.createTime || '-',
              ev.pointId || activePointId.value,
              ev.pointName || pointName,
              ev.trackId,
              dirStr,
              typeof ev.confidence === 'number' ? ev.confidence.toFixed(2) : (ev.confidence || '0.92'),
              ev.vehicleType || '轿车',
              ev.vehicleColor || '白色',
              ev.plateNumber || '未识别',
              ev.plateColor || '蓝牌'
            ]);
          });
        } else {
          sheet2Rows.push(['序号', '通行时间', '检测通道编号', '通道名称', '行人跟踪ID', '通行方向', '置信度', '性别画像', '年龄段画像']);
          records.forEach((ev: any, idx: number) => {
            const dirStr = ev.direction === 'ENTRY' || ev.direction === '进' || ev.direction === '1' ? '进入 (IN)' : '离开 (OUT)';
            sheet2Rows.push([
              idx + 1,
              ev.datetime || ev.createTime || '-',
              ev.pointId || activePointId.value,
              ev.pointName || pointName,
              ev.trackId,
              dirStr,
              typeof ev.confidence === 'number' ? ev.confidence.toFixed(2) : (ev.confidence || '0.90'),
              ev.gender || '未知',
              ev.ageGroup || '未知'
            ]);
          });
        }
      }
    } catch (e) {
      console.warn('获取明细失败，仅导出时序汇总表', e);
    }

    // 3. 构建工作簿
    const wb = XLSX.utils.book_new();
    XLSX.utils.book_append_sheet(wb, ws1, isVeh ? '车流时序汇总' : '客流时序汇总');

    if (sheet2Rows.length > 0) {
      const ws2 = XLSX.utils.aoa_to_sheet(sheet2Rows);
      if (isVeh) {
        ws2['!cols'] = [
          { wch: 8 }, { wch: 20 }, { wch: 14 }, { wch: 18 }, { wch: 12 },
          { wch: 14 }, { wch: 10 }, { wch: 14 }, { wch: 14 }, { wch: 16 }, { wch: 12 }
        ];
      } else {
        ws2['!cols'] = [
          { wch: 8 }, { wch: 20 }, { wch: 14 }, { wch: 18 }, { wch: 12 },
          { wch: 14 }, { wch: 10 }, { wch: 12 }, { wch: 16 }
        ];
      }
      XLSX.utils.book_append_sheet(wb, ws2, isVeh ? '车辆采集明细' : '客流采集明细');
    }

    const cleanPointName = pointName.replace(/[^\w\u4e00-\u9fa5]/g, '_');
    const fileName = `${isVeh ? '车流时序走势报表' : '客流时序走势报表'}_${cleanPointName}_${dimLabel}_${fileTimestamp}.xlsx`;

    const wbout = XLSX.write(wb, { bookType: 'xlsx', type: 'array' });
    const blob = new Blob([wbout], {
      type: 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet'
    });

    // 4. 环境判定与分发（电脑端直接下载，移动端保存并弹窗引导）
    const ua = navigator.userAgent;
    const isMobile = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(ua) || window.innerWidth <= 768;

    exportFileName.value = fileName;
    lastExportBlob.value = blob;

    // 无论任何环境，第一时间触发本地极速下载保存，确保文件稳妥落地到手机/电脑
    const downloadUrl = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = downloadUrl;
    a.download = fileName;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    setTimeout(() => URL.revokeObjectURL(downloadUrl), 1000);

    if (isMobile) {
      // 移动端：直接弹出专业微信转发与打开指引浮层，杜绝系统分享产生的静默丢弃与找不到文件
      showExportModal.value = true;
      showToast({ type: 'success', message: '报表已生成并保存' });
    } else {
      // 电脑端：直接触发极速下载保存至本地
      showToast({ type: 'success', message: '报表已成功下载至本地电脑' });
    }
  } catch (error: any) {
    showToast({ type: 'fail', message: `导出失败: ${error?.message || '未知异常'}` });
  } finally {
    isExporting.value = false;
  }
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

onActivated(() => {
  nextTick(() => {
    if (myChart) myChart.resize();
  });
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
  background-color: var(--fs-bg, #0A0D14);
}

/* 顶部标题栏 */
.flat-header {
  flex-shrink: 0;
  height: 44px;
  background: var(--fs-header-bg, #0E131D);
  border-bottom: 1px solid var(--fs-header-border, #1E2636);
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
  background: var(--fs-primary, #00D2FF);
}

.header-title {
  font-size: 14px;
  font-weight: 700;
  color: var(--fs-text-main, #FFFFFF);
}

.header-right {
  display: flex;
  align-items: center;
  gap: 8px;
}

.realtime-badge {
  font-size: 10px;
  font-weight: 700;
  color: #00D2FF;
  background: rgba(0, 210, 255, 0.12);
  padding: 2px 8px;
  border-radius: 4px;
  white-space: nowrap;
}

.flat-export-btn {
  height: 26px;
  background: linear-gradient(135deg, rgba(0, 210, 255, 0.18), rgba(14, 165, 233, 0.28));
  border: 1px solid rgba(0, 210, 255, 0.45);
  color: #00D2FF;
  font-size: 11px;
  font-weight: 700;
  border-radius: 4px;
  padding: 0 9px;
  display: flex;
  align-items: center;
  gap: 4px;
  cursor: pointer;
  transition: all 0.2s ease;
  white-space: nowrap;
  outline: none;
}

.flat-export-btn:active {
  background: rgba(0, 210, 255, 0.35);
  transform: scale(0.96);
}

.flat-export-btn:disabled {
  opacity: 0.6;
  cursor: not-allowed;
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
  background: var(--fs-card, #121722);
  border: 1px solid var(--fs-card-border, #1E2636);
  border-radius: 6px;
  padding: 0 10px;
  display: flex;
  align-items: center;
  gap: 8px;
}

.point-label {
  font-size: 12px;
  color: var(--fs-text-sub, #94A3B8);
  font-weight: 600;
  flex-shrink: 0;
}

.point-select-wrap {
  flex: 1;
}

.flat-select {
  width: 100%;
  height: 26px;
  background-color: var(--fs-select-bg, #161D2A);
  color: var(--fs-select-text, #00D2FF);
  border: 1px solid var(--fs-select-border, #233048);
  border-radius: 4px;
  padding: 0 20px 0 8px;
  font-size: 12px;
  font-weight: 700;
  outline: none;
  cursor: pointer;
  appearance: none;
  -webkit-appearance: none;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%2300D2FF' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='m6 9 6 6 6-6'/%3E%3C/svg%3E");
  background-repeat: no-repeat;
  background-position: right 6px center;
  transition: all 0.2s ease;
}

.flat-select:focus {
  border-color: var(--fs-primary, #00D2FF);
  box-shadow: 0 0 0 1px rgba(0, 210, 255, 0.3);
}

.flat-select option {
  background-color: var(--fs-select-option-bg, #121824);
  color: var(--fs-select-option-text, #FFFFFF);
  font-size: 13px;
  font-weight: 600;
  padding: 8px 12px;
}

.flat-select option:checked {
  background-color: #1E293B;
  color: var(--fs-primary, #00D2FF);
}

/* 5 维分段选择 */
.flat-segmented {
  flex-shrink: 0;
  height: 32px;
  background: var(--fs-card, #121722);
  border: 1px solid var(--fs-card-border, #1E2636);
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
  color: var(--fs-text-sub, #94A3B8);
  border-radius: 4px;
  cursor: pointer;
  transition: all 0.15s ease;
}

.seg-btn.active {
  background: var(--fs-primary, #00D2FF);
  color: #FFFFFF;
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
  background: var(--fs-card, #121722);
  border: 1px solid var(--fs-card-border, #1E2636);
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
  color: var(--fs-text-dim, #64748B);
  font-weight: 600;
}

.cell-val {
  font-size: 12px;
  font-weight: 800;
}

.cell-unit {
  font-size: 10px;
  font-weight: normal;
  color: var(--fs-text-dim, #64748B);
}

.text-white { color: var(--fs-text-main, #FFFFFF); }
.text-cyan { color: var(--fs-primary, #00D2FF); }
.text-green { color: var(--fs-success, #00E676); }

/* 图表容器 */
.chart-box {
  flex: 1;
  min-height: 0;
  background: var(--fs-card, #121722);
  border: 1px solid var(--fs-card-border, #1E2636);
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
  color: var(--fs-text-main, #CBD5E1);
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

/* ================= 浅色高对比度主题适配 ================= */
:global(.theme-light) .trends-viewport {
  background-color: var(--fs-bg);
}

:global(.theme-light) .flat-header {
  background: var(--fs-header-bg);
  border-bottom-color: var(--fs-border);
}

:global(.theme-light) .header-title {
  color: var(--fs-text-main);
}

:global(.theme-light) .header-dot {
  background: var(--fs-primary);
}

:global(.theme-light) .realtime-badge {
  color: var(--fs-primary);
  background: rgba(2, 132, 199, 0.12);
}

:global(.theme-light) .flat-point-bar {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
}

:global(.theme-light) .point-label {
  color: var(--fs-text-sub);
}

:global(.theme-light) .flat-select {
  background-color: #F1F5F9;
  color: #0F172A;
  border-color: #CBD5E1;
  color-scheme: light;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%230284C7' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='m6 9 6 6 6-6'/%3E%3C/svg%3E");
}

:global(.theme-light) .flat-select option {
  background-color: #FFFFFF !important;
  color: #0F172A !important;
}

:global(.theme-light) .flat-select option:checked {
  background-color: #E2E8F0 !important;
  color: #0284C7 !important;
}

:global(.theme-light) .flat-segmented {
  background: #F1F5F9;
  border-color: var(--fs-card-border);
}

:global(.theme-light) .seg-btn {
  color: var(--fs-text-sub);
}

:global(.theme-light) .seg-btn.active {
  background: var(--fs-primary);
  color: #FFFFFF;
}

:global(.theme-light) .kpi-cell {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
}

:global(.theme-light) .cell-label {
  color: var(--fs-text-dim);
}

:global(.theme-light) .cell-unit {
  color: var(--fs-text-dim);
}

:global(.theme-light) .text-white {
  color: var(--fs-text-main);
}

:global(.theme-light) .text-cyan {
  color: var(--fs-primary);
}

:global(.theme-light) .text-green {
  color: var(--fs-success);
}

:global(.theme-light) .chart-box {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
}

:global(.theme-light) .legend-title {
  color: var(--fs-text-main);
}

:global(.theme-light) .leg-item {
  color: var(--fs-text-sub);
}

:global(.theme-light) .leg-dot.cyan {
  background: var(--fs-primary);
}

:global(.theme-light) .leg-dot.pink {
  background: #E11D48;
}

/* 移动端导出完成与微信转发弹窗 */
.export-modal-mask {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.72);
  backdrop-filter: blur(6px);
  z-index: 9999;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 16px;
  box-sizing: border-box;
}

.export-modal-card {
  width: 100%;
  max-width: 360px;
  background: var(--fs-card, #121722);
  border: 1px solid var(--fs-primary, #00D2FF);
  box-shadow: 0 12px 32px rgba(0, 0, 0, 0.6), 0 0 20px rgba(0, 210, 255, 0.2);
  border-radius: 12px;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  animation: modalScaleIn 0.25s cubic-bezier(0.16, 1, 0.3, 1);
}

@keyframes modalScaleIn {
  from {
    opacity: 0;
    transform: scale(0.92);
  }
  to {
    opacity: 1;
    transform: scale(1);
  }
}

.modal-header {
  height: 46px;
  background: linear-gradient(90deg, rgba(0, 210, 255, 0.15), rgba(14, 165, 233, 0.05));
  border-bottom: 1px solid var(--fs-card-border, #1E2636);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 14px;
  box-sizing: border-box;
}

.modal-title-row {
  display: flex;
  align-items: center;
  gap: 8px;
}

.modal-icon {
  font-size: 16px;
}

.modal-title {
  font-size: 14px;
  font-weight: 700;
  color: #FFFFFF;
}

.modal-close-btn {
  background: transparent;
  border: none;
  color: var(--fs-text-sub, #94A3B8);
  font-size: 16px;
  cursor: pointer;
  padding: 4px;
  line-height: 1;
}

.modal-body {
  padding: 14px;
  display: flex;
  flex-direction: column;
  gap: 12px;
  box-sizing: border-box;
}

.file-info-box {
  background: rgba(0, 210, 255, 0.06);
  border: 1px dashed rgba(0, 210, 255, 0.35);
  border-radius: 8px;
  padding: 10px 12px;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.file-name-row {
  display: flex;
  align-items: center;
  gap: 8px;
}

.file-type-badge {
  font-size: 10px;
  font-weight: 700;
  padding: 2px 6px;
  border-radius: 4px;
  background: rgba(0, 210, 255, 0.2);
  color: #00D2FF;
  flex-shrink: 0;
}

.file-name-text {
  font-size: 12px;
  font-weight: 600;
  color: #F8FAFC;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.file-meta-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-size: 11px;
  color: var(--fs-text-sub, #94A3B8);
}

.wechat-guide-card {
  background: #161D2B;
  border: 1px solid #233048;
  border-radius: 8px;
  padding: 10px 12px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.guide-title {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.guide-badge {
  font-size: 12px;
  font-weight: 700;
  color: #10B981;
}

.guide-sub {
  font-size: 10px;
  color: #64748B;
}

.guide-steps {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.step-item {
  display: flex;
  align-items: flex-start;
  gap: 8px;
  font-size: 11px;
  color: #CBD5E1;
  line-height: 1.4;
}

.step-num {
  width: 16px;
  height: 16px;
  border-radius: 50%;
  background: rgba(16, 185, 129, 0.2);
  color: #10B981;
  font-size: 10px;
  font-weight: 700;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  margin-top: 1px;
}

.step-text b {
  color: #00D2FF;
}

.modal-actions-grid {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 2px;
}

.action-btn {
  height: 38px;
  border-radius: 6px;
  font-size: 12px;
  font-weight: 700;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  cursor: pointer;
  transition: all 0.2s ease;
  border: none;
}

.action-btn:active {
  transform: scale(0.98);
}

.wechat-btn {
  background: linear-gradient(135deg, #07C160, #059648);
  color: #FFFFFF;
  box-shadow: 0 4px 12px rgba(7, 193, 96, 0.3);
}

.copy-btn {
  background: rgba(0, 210, 255, 0.12);
  border: 1px solid rgba(0, 210, 255, 0.4);
  color: #00D2FF;
}

.download-btn {
  background: rgba(148, 163, 184, 0.1);
  border: 1px solid rgba(148, 163, 184, 0.25);
  color: #94A3B8;
}

.modal-footer {
  padding: 4px 14px 14px;
}

.confirm-btn {
  width: 100%;
  height: 34px;
  background: #1E293B;
  border: 1px solid #334155;
  color: #CBD5E1;
  border-radius: 6px;
  font-size: 12px;
  font-weight: 600;
  cursor: pointer;
}

/* 亮色模式适配 */
:global(.theme-light) .export-modal-card {
  background: #FFFFFF;
  border-color: #0284C7;
  box-shadow: 0 12px 32px rgba(0, 0, 0, 0.15);
}

:global(.theme-light) .modal-header {
  background: #F0F9FF;
  border-bottom-color: #E2E8F0;
}

:global(.theme-light) .modal-title {
  color: #0F172A;
}

:global(.theme-light) .file-info-box {
  background: #F8FAFC;
  border-color: #BAE6FD;
}

:global(.theme-light) .file-name-text {
  color: #0F172A;
}

:global(.theme-light) .wechat-guide-card {
  background: #F1F5F9;
  border-color: #E2E8F0;
}

:global(.theme-light) .step-item {
  color: #334155;
}

:global(.theme-light) .step-text b {
  color: #0284C7;
}

:global(.theme-light) .confirm-btn {
  background: #F1F5F9;
  border-color: #CBD5E1;
  color: #475569;
}
</style>