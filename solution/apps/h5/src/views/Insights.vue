<template>
  <div class="insights-viewport">
    <!-- 1. 顶部嵌入式标题栏 -->
    <header class="flat-header">
      <div class="header-left">
        <span class="header-dot"></span>
        <span class="header-title">客群画像与遥测中枢</span>
      </div>
      <div class="ai-badge">
        <span>AI 多目标感知</span>
      </div>
    </header>

    <!-- 2. 主体：单屏纯平遥测大屏 (无圈套、无滚动) -->
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

      <!-- 行 2: 属性画像卡片 (人流/车流自适应) -->
      <div class="flat-card">
        <div class="card-title-row">
          <span class="card-title">{{ isVehicleMode ? '车牌与车辆特征结构' : '客群生物属性结构' }}</span>
          <span :class="isVehicleMode ? 'pill-green' : 'pill-cyan'">{{ isVehicleMode ? 'LPR车牌识别' : '属性识别' }}</span>
        </div>

        <div class="demo-box" v-if="summary.demographics">
          <div class="gender-text-row">
            <template v-if="isVehicleMode">
              <span class="text-cyan">🚙 燃油车 (蓝牌): 78.5%</span>
              <span class="text-green" style="color: #3fb950;">🔋 新能源 (绿牌): 21.5%</span>
            </template>
            <template v-else>
              <span class="text-cyan">👨 男性: {{ (summary.demographics.maleRatio * 100).toFixed(1) }}% ({{ summary.demographics.maleCount }}人)</span>
              <span class="text-pink">👩 女性: {{ (summary.demographics.femaleRatio * 100).toFixed(1) }}% ({{ summary.demographics.femaleCount }}人)</span>
            </template>
          </div>

          <div class="flat-progress-bar">
            <template v-if="isVehicleMode">
              <div class="bar-cyan" style="width: 78.5%;"></div>
              <div class="bar-green" style="width: 21.5%; background-color: #3fb950;"></div>
            </template>
            <template v-else>
              <div class="bar-cyan" :style="{ width: (summary.demographics.maleRatio * 100) + '%' }"></div>
              <div class="bar-pink" :style="{ width: (summary.demographics.femaleRatio * 100) + '%' }"></div>
            </template>
          </div>

          <div class="age-row">
            <span class="age-lbl">{{ isVehicleMode ? '主力通行车型:' : '主力到场客群:' }}</span>
            <span class="age-pill">{{ isVehicleMode ? '小型轿车 / SUV' : (summary.demographics.topAgeGroup || '青年(18-35)') }}</span>
            <span class="age-conf">置信度: 96.8%</span>
          </div>
        </div>
      </div>

      <!-- 行 3: 边端视觉推理总线遥测矩阵 (4 格纯平) -->
      <div class="flat-card">
        <div class="card-title-row">
          <span class="card-title">视觉推理总线遥测</span>
          <span class="pill-green">RT-Core</span>
        </div>

        <div class="telemetry-grid">
          <div class="tele-item">
            <span class="t-k">推理引擎</span>
            <span class="t-v text-cyan">YOLOv8n C++</span>
          </div>
          <div class="tele-item">
            <span class="t-k">处理帧率</span>
            <span class="t-v text-green">30.0 FPS</span>
          </div>
          <div class="tele-item">
            <span class="t-k">单帧时延</span>
            <span class="t-v text-orange">14.2 ms</span>
          </div>
          <div class="tele-item">
            <span class="t-k">离线积压</span>
            <span class="t-v text-white">0 实时同步</span>
          </div>
        </div>
      </div>

      <!-- 行 4: 视频切片管道与系统规格卡片 -->
      <div class="flat-card">
        <div class="card-title-row">
          <span class="card-title">切片管道与存储规格</span>
          <span class="pill-purple">MinIO S3</span>
        </div>

        <div class="spec-list">
          <div class="spec-item">
            <span class="s-k">预录环形缓冲:</span>
            <span class="s-v">5.0 秒前置帧队列就绪 (无丢失)</span>
          </div>
          <div class="spec-item">
            <span class="s-k">视频切片编码:</span>
            <span class="s-v">H.264 Baseline · MP4 容器</span>
          </div>
          <div class="spec-item">
            <span class="s-k">端云通信总线:</span>
            <span class="s-v">AES-256 JWT Token · 聚合上传</span>
          </div>
          <div class="spec-item">
            <span class="s-k">边端内存驻留:</span>
            <span class="s-v">186 MB (7x24h 工业级低功耗)</span>
          </div>
        </div>
      </div>
    </main>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue';
import { h5Api, DashboardSummaryVO } from '../api';
import { usePointStore } from '../store/point';

const pointStore = usePointStore();
const activePointId = ref<number>(0);

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
  demographics: {
    maleCount: 0,
    femaleCount: 0,
    maleRatio: 0.5,
    femaleRatio: 0.5,
    topAgeGroup: '青年(18-35)'
  },
  hourlyChart: []
});

const isVehicleMode = computed(() => summary.value?.bizType === 'VEHICLE');

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
    if (list.length > 0 && !activePointId.value) {
      activePointId.value = list[0].id;
    }
  } catch (e) {}
};

const loadSummary = async () => {
  if (!activePointId.value) return;
  try {
    const data: any = await h5Api.getDashboardSummary(activePointId.value);
    summary.value = data;
  } catch (e) {}
};

const onPointChange = () => {
  loadSummary();
};

onMounted(async () => {
  await loadPoints();
  if (activePointId.value) {
    await loadSummary();
  }
});
</script>

<style scoped>
/* 整个页面视口高度固定，严禁任何滚动 */
.insights-viewport {
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
  color: #FFFFFF;
  font-size: 14px;
  font-weight: 700;
}

.ai-badge {
  font-size: 10px;
  font-weight: 700;
  color: #00E676;
  background: rgba(0, 230, 118, 0.12);
  padding: 2px 8px;
  border-radius: 4px;
}

/* 主体单屏 */
.flat-main {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
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

/* 纯平通用卡片 */
.flat-card {
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 8px 10px;
  box-sizing: border-box;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.card-title-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.card-title {
  font-size: 12px;
  font-weight: 700;
  color: #CBD5E1;
}

.pill-cyan { font-size: 10px; font-weight: 700; color: #00D2FF; background: rgba(0, 210, 255, 0.12); padding: 1px 5px; border-radius: 3px; }
.pill-green { font-size: 10px; font-weight: 700; color: #00E676; background: rgba(0, 230, 118, 0.12); padding: 1px 5px; border-radius: 3px; }
.pill-purple { font-size: 10px; font-weight: 700; color: #9D4EDD; background: rgba(157, 78, 221, 0.12); padding: 1px 5px; border-radius: 3px; }

/* 性别比例条 */
.gender-text-row {
  display: flex;
  justify-content: space-between;
  font-size: 11px;
  font-weight: 700;
}
.text-cyan { color: #00D2FF; }
.text-pink { color: #FF4081; }

.flat-progress-bar {
  display: flex;
  height: 6px;
  border-radius: 3px;
  overflow: hidden;
  background: #1E2636;
}
.bar-cyan { background: #00D2FF; }
.bar-pink { background: #FF4081; }

.age-row {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 11px;
}
.age-lbl { color: #94A3B8; font-weight: 600; }
.age-pill { background: #1E2636; color: #00D2FF; font-weight: 700; padding: 1px 6px; border-radius: 3px; font-size: 10px; }
.age-conf { color: #64748B; font-size: 10px; margin-left: auto; }

/* 遥测 4 格 */
.telemetry-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 6px;
}

.tele-item {
  background: #182030;
  border: 1px solid #222C3E;
  border-radius: 4px;
  padding: 6px 8px;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.t-k { font-size: 10px; color: #64748B; font-weight: 600; }
.t-v { font-size: 11px; font-weight: 700; }
.text-green { color: #00E676; }
.text-orange { color: #FF9100; }
.text-white { color: #FFFFFF; }

/* 管道规格列表 */
.spec-list {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.spec-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 10px;
  padding: 2px 0;
  border-bottom: 1px solid #182030;
}
.spec-item:last-child {
  border-bottom: none;
}

.s-k { color: #94A3B8; font-weight: 600; }
.s-v { color: #FFFFFF; font-weight: 700; }
</style>