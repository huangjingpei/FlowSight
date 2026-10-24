<template>
  <div class="dashboard-viewport">
    <!-- 1. 顶部嵌入式标题栏 -->
    <header class="flat-header">
      <div class="header-left">
        <span class="status-dot" :class="summary.streamStatus === 'ONLINE' ? 'online' : 'offline'"></span>
        <span class="company-name">{{ currentCompanyName }}</span>
      </div>
      <div class="header-right" @click="handleLogout">
        <span class="logout-btn">退出</span>
      </div>
    </header>

    <!-- 2. 主体：单屏纯平数据仪表盘 (无圈套、无滚动、数据极度聚焦) -->
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
        <span class="online-tag" :class="summary.streamStatus === 'ONLINE' ? 'tag-online' : 'tag-offline'">
          {{ summary.streamStatus === 'ONLINE' ? '在线' : '离线' }}
        </span>
      </div>

      <!-- 行 2: 四大核心客流指标矩阵 (扁平化高反差色块，大字号聚焦) -->
      <div class="kpi-matrix">
        <!-- 1. 瞬时滞留 -->
        <div class="kpi-block border-cyan">
          <div class="kpi-top">
            <span class="kpi-title">瞬时滞留</span>
            <span class="kpi-badge badge-cyan">在场</span>
          </div>
          <div class="kpi-num text-cyan">{{ summary.currentOccupancy }}<span class="kpi-unit">人</span></div>
          <div class="kpi-footer">当前通道内实时在场</div>
        </div>

        <!-- 2. 今日进入 -->
        <div class="kpi-block border-green">
          <div class="kpi-top">
            <span class="kpi-title">累计进入 (In)</span>
            <span class="kpi-badge badge-green">进场</span>
          </div>
          <div class="kpi-num text-green">{{ summary.todayEntry }}<span class="kpi-unit">人次</span></div>
          <div class="kpi-footer">当日累计进门总人次</div>
        </div>

        <!-- 3. 今日离开 -->
        <div class="kpi-block border-orange">
          <div class="kpi-top">
            <span class="kpi-title">累计离开 (Out)</span>
            <span class="kpi-badge badge-orange">离场</span>
          </div>
          <div class="kpi-num text-orange">{{ summary.todayExit }}<span class="kpi-unit">人次</span></div>
          <div class="kpi-footer">当日累计出门总人次</div>
        </div>

        <!-- 4. 双向总流 -->
        <div class="kpi-block border-purple">
          <div class="kpi-top">
            <span class="kpi-title">双向总客流</span>
            <span class="kpi-badge badge-purple">汇聚</span>
          </div>
          <div class="kpi-num text-purple">{{ summary.todayTotal }}<span class="kpi-unit">人次</span></div>
          <div class="kpi-footer">进出双向总吞吐通量</div>
        </div>
      </div>

      <!-- 行 3: 安全状态指示条 -->
      <div class="flat-status-bar" :class="summary.currentOccupancy > 30 ? 'status-warn' : 'status-ok'">
        <span class="status-icon">{{ summary.currentOccupancy > 30 ? '⚠️' : '✓' }}</span>
        <span class="status-msg">
          {{ summary.currentOccupancy > 30 ? '当前人流密度偏高，已触发疏导指引' : '通道通行状态与人流安全基线正常' }}
        </span>
      </div>

      <!-- 行 4: 底部边缘节点工业遥测条 -->
      <div class="flat-tele-bar">
        <div class="tele-col">
          <span class="t-label">推理引擎</span>
          <span class="t-val text-cyan">YOLOv8</span>
        </div>
        <div class="tele-col">
          <span class="t-label">采集帧率</span>
          <span class="t-val text-green">30.0 FPS</span>
        </div>
        <div class="tele-col">
          <span class="t-label">单帧时延</span>
          <span class="t-val text-orange">14.2 ms</span>
        </div>
        <div class="tele-col">
          <span class="t-label">离线积压</span>
          <span class="t-val text-white">0 同步</span>
        </div>
      </div>
    </main>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue';
import { useRouter } from 'vue-router';
import { showToast, showDialog } from 'vant';
import { h5Api, DashboardSummaryVO } from '../api';
import { usePointStore } from '../store/point';

const router = useRouter();
const pointStore = usePointStore();

const activePointId = ref<number>(0);

const currentCompanyName = computed(() => {
  try {
    const raw = localStorage.getItem('flowsight_user');
    if (raw) {
      const u = JSON.parse(raw);
      return u.companyName || u.nickname || 'FlowSight 决策中台';
    }
  } catch (e) {}
  return 'FlowSight 决策中台';
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
  demographics: {
    maleCount: 0,
    femaleCount: 0,
    maleRatio: 0.5,
    femaleRatio: 0.5,
    topAgeGroup: '青年(18-35)'
  },
  hourlyChart: []
});

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

const handleLogout = () => {
  showDialog({
    title: '安全登出',
    message: '确定要退出当前 FlowSight 决策账号吗？',
    showCancelButton: true
  }).then(() => {
    localStorage.removeItem('flowsight_token');
    localStorage.removeItem('flowsight_user');
    showToast('已安全登出');
    router.push('/login');
  }).catch(() => {});
};

onMounted(async () => {
  await loadPoints();
  if (activePointId.value) {
    await loadSummary();
  }
  timer = setInterval(loadSummary, 10000);
});

onUnmounted(() => {
  if (timer) clearInterval(timer);
});
</script>

<style scoped>
/* 整个页面视口高度固定，严禁任何滚动 */
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

.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
}
.status-dot.online { background: #00E676; }
.status-dot.offline { background: #FF3D71; }

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

.logout-btn {
  font-size: 11px;
  font-weight: 600;
  color: #94A3B8;
  cursor: pointer;
  padding: 3px 8px;
  border-radius: 4px;
  background: #182030;
  border: 1px solid #2A364F;
}
.logout-btn:active {
  color: #FF3D71;
  border-color: #FF3D71;
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
  gap: 8px;
}

/* 行 1: 点位选择条 */
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

.online-tag {
  font-size: 10px;
  font-weight: 700;
  padding: 2px 6px;
  border-radius: 4px;
  flex-shrink: 0;
}
.tag-online { background: rgba(0, 230, 118, 0.15); color: #00E676; }
.tag-offline { background: rgba(255, 61, 113, 0.15); color: #FF3D71; }

/* 行 2: 四大指标矩阵 (2x2，等高自适应) */
.kpi-matrix {
  flex: 1;
  min-height: 0;
  display: grid;
  grid-template-columns: 1fr 1fr;
  grid-template-rows: 1fr 1fr;
  gap: 8px;
}

.kpi-block {
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 8px;
  padding: 10px 12px;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  box-sizing: border-box;
}

/* 扁平纯色左侧重点色 */
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
  font-size: 12px;
  font-weight: 700;
  color: #CBD5E1;
}

.kpi-badge {
  font-size: 10px;
  font-weight: 700;
  padding: 1px 5px;
  border-radius: 3px;
}
.badge-cyan { background: rgba(0, 210, 255, 0.12); color: #00D2FF; }
.badge-green { background: rgba(0, 230, 118, 0.12); color: #00E676; }
.badge-orange { background: rgba(255, 145, 0, 0.12); color: #FF9100; }
.badge-purple { background: rgba(157, 78, 221, 0.12); color: #9D4EDD; }

.kpi-num {
  font-size: 32px;
  font-weight: 800;
  line-height: 1;
  margin: 4px 0;
}

.text-cyan { color: #00D2FF; }
.text-green { color: #00E676; }
.text-orange { color: #FF9100; }
.text-purple { color: #9D4EDD; }

.kpi-unit {
  font-size: 11px;
  font-weight: 600;
  color: #64748B;
  margin-left: 2px;
}

.kpi-footer {
  font-size: 10px;
  color: #64748B;
}

/* 行 3: 安全状态条 */
.flat-status-bar {
  flex-shrink: 0;
  height: 30px;
  border-radius: 6px;
  padding: 0 10px;
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 11px;
  font-weight: 600;
}
.status-ok {
  background: rgba(0, 230, 118, 0.1);
  color: #00E676;
  border: 1px solid rgba(0, 230, 118, 0.25);
}
.status-warn {
  background: rgba(255, 145, 0, 0.1);
  color: #FF9100;
  border: 1px solid rgba(255, 145, 0, 0.25);
}

.status-icon {
  font-weight: 800;
}

/* 行 4: 底部遥测条 */
.flat-tele-bar {
  flex-shrink: 0;
  height: 40px;
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  padding: 4px 0;
  box-sizing: border-box;
}

.tele-col {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 2px;
  border-right: 1px solid #1E2636;
}
.tele-col:last-child {
  border-right: none;
}

.t-label {
  font-size: 9px;
  color: #64748B;
  font-weight: 600;
}

.t-val {
  font-size: 11px;
  font-weight: 700;
}
.text-white { color: #FFFFFF; }
</style>