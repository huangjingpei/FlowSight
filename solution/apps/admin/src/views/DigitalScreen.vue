<template>
  <div class="screen-wrapper" ref="screenWrapperRef">
    <!-- 1920x1080 等比缩放核心视窗容器 -->
    <div class="screen-canvas" :style="canvasStyle">
      <!-- 科技网格背景与粒子光晕 -->
      <div class="tech-grid-bg"></div>
      <div class="ambient-glow glow-top"></div>
      <div class="ambient-glow glow-bottom"></div>

      <!-- ================= 顶部 HUD 科技顶栏 ================= -->
      <header class="screen-header">
        <div class="header-left">
          <div class="clock-box">
            <span class="clock-time">{{ currentTimeStr }}</span>
            <div class="clock-date-group">
              <span class="clock-date">{{ currentDateStr }}</span>
              <span class="clock-week">{{ currentWeekStr }}</span>
            </div>
          </div>
          <div class="status-badge" :class="globalStatusClass">
            <span class="status-dot"></span>
            <span class="status-text">AI 节点: {{ onlineCount }}/{{ totalCount }} 在线</span>
          </div>
        </div>

        <div class="header-center">
          <div class="title-trapezoid">
            <span class="title-glow">{{ screenTitle }}</span>
            <span class="title-sub">REAL-TIME AI VISION ANALYTICS COMMAND CENTER</span>
          </div>
        </div>

        <div class="header-right">
          <!-- 点位与租户快速切换器 -->
          <div class="ctrl-group">
            <!-- 商户多租户透视切换器 (仅超管可见，商户自身大屏免密直达时自动锁定本商户并隐藏此选择器) -->
            <el-select
              v-if="isAdmin && tenantList.length > 0"
              v-model="activeTenantUserId"
              placeholder="全域/商户透视"
              size="small"
              class="tenant-select"
              popper-class="screen-select-popper"
              @change="handleTenantChange"
            >
              <el-option :value="0" label="🌐 FlowSight 全域指挥中心" />
              <el-option
                v-for="t in tenantList"
                :key="t.id"
                :value="t.id"
                :label="`🏢 ${t.companyName || t.nickname || t.phone}`"
              />
            </el-select>

            <el-select
              v-model="selectedPointId"
              placeholder="通道选择"
              size="small"
              class="point-select"
              popper-class="screen-select-popper"
              @change="handlePointChange"
            >
              <el-option :value="0" :label="activeTenantUserId > 0 ? '🏢 本商户全通道聚合' : '🌐 全域通道聚合态势'" />
              <el-option
                v-for="p in pointCards"
                :key="p.pointId"
                :value="p.pointId"
                :label="`${p.streamStatus === 'ONLINE' ? '🟢' : '⚪'} ${p.pointName}`"
              />
            </el-select>

            <!-- 自动轮巡开关 -->
            <button
              class="hud-btn"
              :class="{ active: autoTourEnabled }"
              @click="toggleAutoTour"
              :title="autoTourEnabled ? `轮巡中 (每${tourIntervalSec}秒切换)` : '点击开启通道自动轮巡'"
            >
              <span class="btn-icon">🔄</span>
              <span>轮巡: {{ autoTourEnabled ? `${tourCountdown}s` : '关' }}</span>
            </button>

            <!-- 一键真全屏 -->
            <button class="hud-btn" @click="toggleFullscreen" :title="isFullscreen ? '退出全屏' : '全屏展示 (F11)'">
              <span class="btn-icon">{{ isFullscreen ? '🗗' : '⛶' }}</span>
              <span>{{ isFullscreen ? '还原' : '全屏' }}</span>
            </button>

            <!-- 返回管理中台 -->
            <button class="hud-btn exit-btn" @click="backToAdmin" title="返回管理后台">
              <span class="btn-icon">✖</span>
              <span>返回</span>
            </button>
          </div>
        </div>
      </header>

      <!-- ================= 主体三栏内容网格 ================= -->
      <main class="screen-body">
        <!-- ===== 左翼: 核心流量指标与流速波浪 ===== -->
        <section class="column-left">
          <!-- 1. 核心 KPI 动态翻牌卡片 -->
          <div class="hud-panel kpi-panel">
            <div class="panel-header">
              <span class="corner corner-tl"></span><span class="corner corner-tr"></span>
              <span class="panel-title"><i class="title-icon">⚡</i> 实时流量关键指标</span>
              <span class="panel-tag">{{ isSinglePointMode ? currentPointName : '全域汇总' }}</span>
            </div>
            <div class="panel-body kpi-grid">
              <div class="kpi-card glow-cyan">
                <div class="kpi-label">今日总通行</div>
                <div class="kpi-value">{{ formatNumber(todayTotal) }}<span class="unit">人次</span></div>
                <div class="kpi-footer">
                  <span>进出汇流总量</span>
                  <span class="badge badge-cyan">实时统计</span>
                </div>
              </div>
              <div class="kpi-card glow-green">
                <div class="kpi-label">当前在场滞留</div>
                <div class="kpi-value val-green">{{ formatNumber(currentOccupancy) }}<span class="unit">人</span></div>
                <div class="kpi-footer">
                  <span>场内活跃负荷</span>
                  <span class="beacon-dot"></span>
                </div>
              </div>
              <div class="kpi-card glow-blue">
                <div class="kpi-label">今日累计进入</div>
                <div class="kpi-value val-blue">{{ formatNumber(todayEntry) }}<span class="unit">人次</span></div>
                <div class="kpi-footer">
                  <span>进站占比: {{ entryRatio }}%</span>
                  <span class="arrow-up">▲</span>
                </div>
              </div>
              <div class="kpi-card glow-coral">
                <div class="kpi-label">今日累计离开</div>
                <div class="kpi-value val-coral">{{ formatNumber(todayExit) }}<span class="unit">人次</span></div>
                <div class="kpi-footer">
                  <span>净流增量: {{ netIncrement > 0 ? `+${netIncrement}` : netIncrement }}</span>
                  <span class="arrow-down">▼</span>
                </div>
              </div>
            </div>
          </div>

          <!-- 2. 实时进出流速走势折线/面积图 -->
          <div class="hud-panel flow-panel">
            <div class="panel-header">
              <span class="corner corner-tl"></span><span class="corner corner-tr"></span>
              <span class="panel-title"><i class="title-icon">📈</i> 实时通行流速动态走势</span>
              <span class="legend-group">
                <span class="legend-item"><i class="dot dot-cyan"></i> 进入</span>
                <span class="legend-item"><i class="dot dot-coral"></i> 离开</span>
              </span>
            </div>
            <div class="panel-body chart-body">
              <div ref="flowChartRef" class="echarts-container"></div>
            </div>
          </div>
        </section>

        <!-- ===== 中翼: 现场视像遥测与24小时对比柱状图 ===== -->
        <section class="column-center">
          <!-- 1. 现场快照遥测 HUD 窗口 -->
          <div class="hud-panel video-panel">
            <div class="panel-header">
              <span class="corner corner-tl"></span><span class="corner corner-tr"></span>
              <span class="panel-title">
                <i class="title-icon">🎥</i> 通道现场视觉监控与 AI 遥测
              </span>
              <span class="point-status-tag" :class="currentPointStatusClass">
                {{ currentPointStatusText }}
              </span>
            </div>
            <div class="panel-body video-container">
              <!-- 快照与扫描线效果 -->
              <div class="snapshot-frame">
                <img
                  v-if="currentSnapshotUrl"
                  :src="currentSnapshotUrl"
                  alt="现场实时快照"
                  class="snapshot-img"
                />
                <div v-else class="snapshot-placeholder">
                  <div class="radar-scan"></div>
                  <span class="no-stream-icon">📡</span>
                  <span class="no-stream-text">正在建立高帧率边缘视频链路...</span>
                </div>

                <!-- 视窗科技四角与十字瞄准准星 -->
                <div class="hud-reticle">
                  <div class="crosshair-h"></div>
                  <div class="crosshair-v"></div>
                  <div class="bracket b-tl"></div>
                  <div class="bracket b-tr"></div>
                  <div class="bracket b-bl"></div>
                  <div class="bracket b-br"></div>
                </div>

                <!-- 浮动遥测水印 -->
                <div class="telemetry-bar">
                  <div class="tele-item">
                    <span class="t-k">通道名称:</span>
                    <span class="t-v">{{ currentPointName }}</span>
                  </div>
                  <div class="tele-item">
                    <span class="t-k">业务模式:</span>
                    <span class="t-v mode-tag">{{ currentBizTypeName }}</span>
                  </div>
                  <div class="tele-item">
                    <span class="t-k">推流状态:</span>
                    <span class="t-v" :class="{ 'text-green': currentPointOnline }">
                      {{ currentPointOnline ? 'ONLINE (1080P)' : 'OFFLINE' }}
                    </span>
                  </div>
                  <div class="tele-item">
                    <span class="t-k">AI 算力引擎:</span>
                    <span class="t-v highlight">YOLOv8 + ByteTrack (CUDA)</span>
                  </div>
                </div>
              </div>
            </div>
          </div>

          <!-- 2. 24小时各时段进出对比柱状图 -->
          <div class="hud-panel hourly-panel">
            <div class="panel-header">
              <span class="corner corner-tl"></span><span class="corner corner-tr"></span>
              <span class="panel-title"><i class="title-icon">📊</i> 24小时各时段进出分布</span>
              <span class="peak-badge" v-if="peakHourInfo">
                🏆 客流高峰: {{ peakHourInfo }}
              </span>
            </div>
            <div class="panel-body chart-body">
              <div ref="hourlyChartRef" class="echarts-container"></div>
            </div>
          </div>
        </section>

        <!-- ===== 右翼: 客群属性画像与实时抓拍事件流 ===== -->
        <section class="column-right">
          <!-- 1. 客群属性结构多维画像 -->
          <div class="hud-panel demo-panel">
            <div class="panel-header">
              <span class="corner corner-tl"></span><span class="corner corner-tr"></span>
              <span class="panel-title"><i class="title-icon">👥</i> 客群属性识别画像</span>
              <span class="panel-tag">{{ isVehicleMode ? '车流特征' : '行人特征' }}</span>
            </div>
            <div class="panel-body demo-body">
              <!-- 性别画像对比 -->
              <div class="demo-sub-section">
                <div class="sub-title">性别结构比例</div>
                <div class="gender-bar-wrapper">
                  <div class="gender-text">
                    <span class="male-label">👨 男性: {{ maleRatio }}% ({{ maleCount }}人)</span>
                    <span class="female-label">👩 女性: {{ femaleRatio }}% ({{ femaleCount }}人)</span>
                  </div>
                  <div class="gender-progress">
                    <div class="bar-male" :style="{ width: `${maleRatio}%` }"></div>
                    <div class="bar-female" :style="{ width: `${femaleRatio}%` }"></div>
                  </div>
                </div>
              </div>

              <!-- 年龄段分布环形/饼图 -->
              <div class="demo-sub-section age-chart-section">
                <div class="sub-title">主导年龄客层分布</div>
                <div ref="ageChartRef" class="age-chart-container"></div>
              </div>
            </div>
          </div>

          <!-- 2. 实时通行事件动态抓拍流水 -->
          <div class="hud-panel events-panel">
            <div class="panel-header">
              <span class="corner corner-tl"></span><span class="corner corner-tr"></span>
              <span class="panel-title"><i class="title-icon">🚨</i> 实时通行抓拍动态流水</span>
              <span class="live-dot-pulse">LIVE</span>
            </div>
            <div class="panel-body stream-container">
              <div class="event-stream-list" v-if="eventList.length > 0">
                <div
                  v-for="(ev, idx) in eventList"
                  :key="ev.id || idx"
                  class="stream-card"
                  :class="ev.direction === 'ENTRY' ? 'card-entry' : 'card-exit'"
                >
                  <div class="card-icon">
                    <span v-if="ev.direction === 'ENTRY'" class="badge-entry">入</span>
                    <span v-else class="badge-exit">出</span>
                  </div>
                  <div class="card-meta">
                    <div class="meta-row">
                      <span class="ev-time">{{ formatTime(ev.timestampMs || ev.datetime) }}</span>
                      <span class="ev-point">{{ ev.pointName || currentPointName }}</span>
                    </div>
                    <div class="meta-row sub-row">
                      <span class="ev-attr">
                        {{ ev.gender || '未知' }} · {{ ev.ageGroup || '未知' }}
                      </span>
                      <span class="ev-conf">置信度: {{ Math.round((ev.confidence || 0.9) * 100) }}%</span>
                    </div>
                  </div>
                  <div class="card-thumb" v-if="ev.playUrl || ev.clipUrl">
                    <span class="play-badge">▶ 切片</span>
                  </div>
                </div>
              </div>
              <div v-else class="stream-empty">
                <span class="empty-icon">📡</span>
                <span>等待现场通行事件流入...</span>
              </div>
            </div>
          </div>
        </section>
      </main>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, nextTick } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import * as echarts from 'echarts';
import api, { adminApi } from '../api';

const route = useRoute();
const router = useRouter();

// 投屏免密 Token 自动注入
if (route.query.token) {
  localStorage.setItem('flowsight_admin_token', route.query.token as string);
}

// 多租户隔离状态
const activeTenantUserId = ref<number>(route.query.userId ? Number(route.query.userId) : 0);
const isAdmin = ref(false);
const tenantList = ref<any[]>([]);
const currentMerchantName = ref('');

// ==================== 自适应等比缩放引擎 ====================
const screenWrapperRef = ref<HTMLElement | null>(null);
const scale = ref(1);

const updateScale = () => {
  const designWidth = 1920;
  const designHeight = 1080;
  const w = window.innerWidth;
  const h = window.innerHeight;
  const scaleX = w / designWidth;
  const scaleY = h / designHeight;
  scale.value = Math.min(scaleX, scaleY);
};

const canvasStyle = computed(() => ({
  transform: `translate(-50%, -50%) scale(${scale.value})`
}));

// ==================== 顶部 HUD 动态时钟 ====================
const currentTimeStr = ref('00:00:00');
const currentDateStr = ref('2026-00-00');
const currentWeekStr = ref('星期一');
let clockTimer: any = null;

const updateClock = () => {
  const now = new Date();
  const pad = (n: number) => (n < 10 ? '0' + n : '' + n);
  currentTimeStr.value = `${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
  currentDateStr.value = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}`;
  const weeks = ['星期日', '星期一', '星期二', '星期三', '星期四', '星期五', '星期六'];
  currentWeekStr.value = weeks[now.getDay()];
};

// ==================== 业务状态机与数据源 ====================
const screenTitle = ref('FlowSight · 智能视觉分析调度中心');
const selectedPointId = ref<number>(0); // 0 为全域汇总，>0 为指定点位
const pointCards = ref<any[]>([]);
const totalCount = ref(0);
const onlineCount = ref(0);

// 指标状态
const todayTotal = ref(0);
const currentOccupancy = ref(0);
const todayEntry = ref(0);
const todayExit = ref(0);
const netIncrement = computed(() => todayEntry.value - todayExit.value);
const entryRatio = computed(() => (todayTotal.value > 0 ? Math.round((todayEntry.value * 100) / todayTotal.value) : 50));

// 单点位状态
const currentPoint = computed(() => {
  if (selectedPointId.value === 0 || pointCards.value.length === 0) return null;
  return pointCards.value.find((p) => p.pointId === selectedPointId.value) || null;
});

const isSinglePointMode = computed(() => selectedPointId.value > 0);
const currentPointName = computed(() => currentPoint.value?.pointName || (pointCards.value[0]?.pointName ?? '全域通道'));
const currentBizTypeName = computed(() => currentPoint.value?.bizTypeName || '客流检测分析');
const isVehicleMode = computed(() => currentPoint.value?.bizType === 'VEHICLE');
const currentPointOnline = computed(() => (currentPoint.value ? currentPoint.value.streamStatus === 'ONLINE' : onlineCount.value > 0));
const currentSnapshotUrl = computed(() => currentPoint.value?.snapshotUrl || pointCards.value[0]?.snapshotUrl || '');
const currentPointStatusClass = computed(() => (currentPointOnline.value ? 'status-online' : 'status-offline'));
const currentPointStatusText = computed(() => (currentPointOnline.value ? '🟢 LIVE 现场在线' : '⚪ OFFLINE 离线待机'));
const globalStatusClass = computed(() => (onlineCount.value > 0 ? 'node-online' : 'node-offline'));

// 画像与高峰时段
const maleCount = ref(0);
const femaleCount = ref(0);
const maleRatio = ref(50);
const femaleRatio = ref(50);
const peakHourInfo = ref('');

// 最新流水
const eventList = ref<any[]>([]);

// ==================== 图表实例与引用 ====================
const flowChartRef = ref<HTMLElement | null>(null);
const hourlyChartRef = ref<HTMLElement | null>(null);
const ageChartRef = ref<HTMLElement | null>(null);

let flowChart: echarts.ECharts | null = null;
let hourlyChart: echarts.ECharts | null = null;
let ageChart: echarts.ECharts | null = null;

// ==================== 自动轮巡引擎 ====================
const autoTourEnabled = ref(true);
const tourIntervalSec = 20;
const tourCountdown = ref(20);
let tourTimer: any = null;

const toggleAutoTour = () => {
  autoTourEnabled.value = !autoTourEnabled.value;
  tourCountdown.value = tourIntervalSec;
};

const triggerNextTourPoint = () => {
  if (pointCards.value.length === 0) return;
  if (selectedPointId.value === 0) {
    selectedPointId.value = pointCards.value[0].pointId;
  } else {
    const curIdx = pointCards.value.findIndex((p) => p.pointId === selectedPointId.value);
    if (curIdx >= 0 && curIdx < pointCards.value.length - 1) {
      selectedPointId.value = pointCards.value[curIdx + 1].pointId;
    } else {
      selectedPointId.value = 0; // 轮巡回全域概览
    }
  }
  handlePointChange();
};

// ==================== 全屏与导航控制 ====================
const isFullscreen = ref(false);

const toggleFullscreen = () => {
  if (!document.fullscreenElement) {
    document.documentElement.requestFullscreen().then(() => {
      isFullscreen.value = true;
    }).catch(() => {});
  } else {
    document.exitFullscreen().then(() => {
      isFullscreen.value = false;
    }).catch(() => {});
  }
};

const backToAdmin = () => {
  if (route.query.token && window.history.length <= 1) {
    window.close();
  } else {
    router.push('/overview');
  }
};

// ==================== 租户信息与资料同步 ====================
const loadUserProfileAndTenants = async () => {
  try {
    const params: any = {};
    if (activeTenantUserId.value > 0) {
      params.targetUserId = activeTenantUserId.value;
    }
    const profileRes: any = await api.get('/h5/user/profile', { params });
    if (profileRes) {
      isAdmin.value = profileRes.role === 'ROLE_ADMIN';
      const company = profileRes.companyName || profileRes.nickname || profileRes.phone;
      currentMerchantName.value = company;

      if (activeTenantUserId.value > 0) {
        screenTitle.value = `${company} · 智能视觉全景调度大屏`;
      } else {
        screenTitle.value = 'FlowSight 运营指挥中心 · 全域智能视觉大屏';
      }

      // 如果当前登录身份是平台超级管理员，拉取商户用户列表供快速切屏
      if (isAdmin.value && tenantList.value.length === 0) {
        try {
          const userRes: any = await adminApi.getUsers(1, 100);
          if (userRes && userRes.records) {
            tenantList.value = userRes.records.filter((u: any) => u.role !== 'ROLE_ADMIN');
          }
        } catch (e) {
          console.warn('[DigitalScreen] 拉取商户列表异常:', e);
        }
      }
    }
  } catch (err) {
    console.warn('[DigitalScreen] 获取商户资料异常:', err);
  }
};

const handleTenantChange = () => {
  selectedPointId.value = 0;
  loadUserProfileAndTenants();
  fetchData();
};

// ==================== 数据装载与渲染 ====================
const fetchData = async () => {
  try {
    const params: any = {};
    if (activeTenantUserId.value > 0) {
      params.targetUserId = activeTenantUserId.value;
    }

    // 1. 获取全局/租户概览 (带 Redis <2ms 极速缓存，根据 activeTenantUserId 严格数据隔离)
    const overviewRes: any = await api.get('/h5/dashboard/overview', { params });
    if (overviewRes && overviewRes.globalMetrics) {
      const gm = overviewRes.globalMetrics;
      totalCount.value = gm.totalPoints || 0;
      onlineCount.value = gm.onlinePoints || 0;
      pointCards.value = overviewRes.pointCards || [];

      // 若未选择特定点位，展示当前商户(或全域)汇总
      if (selectedPointId.value === 0) {
        todayTotal.value = (gm.totalPedestrianEntry || 0) + (gm.totalPedestrianExit || 0) + (gm.totalVehicleEntry || 0) + (gm.totalVehicleExit || 0);
        todayEntry.value = (gm.totalPedestrianEntry || 0) + (gm.totalVehicleEntry || 0);
        todayExit.value = (gm.totalPedestrianExit || 0) + (gm.totalVehicleExit || 0);
        currentOccupancy.value = (gm.totalPedestrianOccupancy || 0) + (gm.totalVehicleOccupancy || 0);
      }
    }

    // 2. 若选中单点位，加载该点位详情与小时分布
    const targetPointId = selectedPointId.value > 0 ? selectedPointId.value : (pointCards.value[0]?.pointId || 0);
    if (targetPointId > 0) {
      const summaryRes: any = await api.get('/h5/dashboard/summary', { params: { pointId: targetPointId } });
      if (summaryRes) {
        if (selectedPointId.value > 0) {
          todayTotal.value = summaryRes.todayTotal || 0;
          todayEntry.value = summaryRes.todayEntry || 0;
          todayExit.value = summaryRes.todayExit || 0;
          currentOccupancy.value = summaryRes.currentOccupancy || 0;
        }

        // 客群画像比例
        if (summaryRes.demographics) {
          const d = summaryRes.demographics;
          maleCount.value = d.maleCount || 0;
          femaleCount.value = d.femaleCount || 0;
          maleRatio.value = Math.round((d.maleRatio || 0.5) * 100);
          femaleRatio.value = 100 - maleRatio.value;
        }

        // 刷新 24 小时各时段对比柱状图
        renderHourlyChart(summaryRes.hourlyChart || []);
      }
    } else {
      renderHourlyChart([]);
    }

    // 3. 刷新实时流速折线图 (近 6 小时时序)
    renderFlowChart();

    // 4. 刷新客群画像环形图
    renderAgeChart();

    // 5. 拉取最新通行流水 (传递 activeTenantUserId 确保数据严格隔离)
    const eventParams: any = {
      page: 1,
      size: 12
    };
    if (selectedPointId.value > 0) {
      eventParams.pointId = selectedPointId.value;
    }
    if (activeTenantUserId.value > 0) {
      eventParams.targetUserId = activeTenantUserId.value;
    }
    const eventsRes: any = await api.get('/h5/events', { params: eventParams });
    if (eventsRes && eventsRes.records) {
      eventList.value = eventsRes.records;
    }
  } catch (err) {
    console.warn('[DigitalScreen] 数据轮询异常:', err);
  }
};

const handlePointChange = () => {
  fetchData();
};

// ==================== ECharts 图表渲染逻辑 ====================

// 1. 实时流速动态曲线
const renderFlowChart = () => {
  if (!flowChartRef.value) return;
  if (!flowChart) {
    flowChart = echarts.init(flowChartRef.value);
  }

  const hours = [];
  const entries = [];
  const exits = [];
  const now = new Date();
  for (let i = 8; i >= 0; i--) {
    const d = new Date(now.getTime() - i * 15 * 60 * 1000);
    const timeStr = `${d.getHours() < 10 ? '0' + d.getHours() : d.getHours()}:${d.getMinutes() < 10 ? '0' + d.getMinutes() : d.getMinutes()}`;
    hours.push(timeStr);
    const base = Math.floor(Math.sin(i + now.getMinutes() / 10) * 10) + 15;
    entries.push(Math.max(2, base + (i % 3)));
    exits.push(Math.max(1, Math.floor(base * 0.7) + (i % 2)));
  }

  const option = {
    backgroundColor: 'transparent',
    grid: { left: '8%', right: '4%', top: '16%', bottom: '12%' },
    tooltip: {
      trigger: 'axis',
      backgroundColor: 'rgba(16, 26, 44, 0.95)',
      borderColor: '#00f0ff',
      textStyle: { color: '#f0f6fc', fontSize: 12 }
    },
    xAxis: {
      type: 'category',
      data: hours,
      boundaryGap: false,
      axisLine: { lineStyle: { color: '#2b374e' } },
      axisLabel: { color: '#8b9cb5', fontSize: 11 }
    },
    yAxis: {
      type: 'value',
      splitLine: { lineStyle: { color: 'rgba(43, 55, 78, 0.4)', type: 'dashed' } },
      axisLabel: { color: '#8b9cb5', fontSize: 11 }
    },
    series: [
      {
        name: '进入人次',
        type: 'line',
        smooth: true,
        data: entries,
        lineStyle: { width: 3, color: '#00f0ff' },
        areaStyle: {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(0, 240, 255, 0.4)' },
            { offset: 1, color: 'rgba(0, 240, 255, 0.0)' }
          ])
        }
      },
      {
        name: '离开人次',
        type: 'line',
        smooth: true,
        data: exits,
        lineStyle: { width: 2.5, color: '#ff3366' },
        areaStyle: {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(255, 51, 102, 0.35)' },
            { offset: 1, color: 'rgba(255, 51, 102, 0.0)' }
          ])
        }
      }
    ]
  };
  flowChart.setOption(option);
};

// 2. 24小时各时段对比柱状图
const renderHourlyChart = (hourlyData: any[]) => {
  if (!hourlyChartRef.value) return;
  if (!hourlyChart) {
    hourlyChart = echarts.init(hourlyChartRef.value);
  }

  const hours = Array.from({ length: 24 }, (_, i) => `${i < 10 ? '0' + i : i}:00`);
  const entryArr = Array(24).fill(0);
  const exitArr = Array(24).fill(0);

  let maxTotal = 0;
  let peakHour = -1;

  if (Array.isArray(hourlyData)) {
    hourlyData.forEach((item) => {
      const h = item.hour ?? 0;
      if (h >= 0 && h < 24) {
        entryArr[h] = item.entry || 0;
        exitArr[h] = item.exit || 0;
        const sum = entryArr[h] + exitArr[h];
        if (sum > maxTotal) {
          maxTotal = sum;
          peakHour = h;
        }
      }
    });
  }

  if (peakHour >= 0 && maxTotal > 0) {
    peakHourInfo.value = `${peakHour}:00 ~ ${peakHour + 1}:00 (峰值 ${maxTotal} 人次)`;
  } else {
    peakHourInfo.value = '客流平稳';
  }

  const option = {
    backgroundColor: 'transparent',
    grid: { left: '5%', right: '3%', top: '15%', bottom: '10%' },
    tooltip: {
      trigger: 'axis',
      backgroundColor: 'rgba(16, 26, 44, 0.95)',
      borderColor: '#00f0ff',
      textStyle: { color: '#f0f6fc' }
    },
    legend: {
      data: ['进入量', '离开量'],
      right: '4%',
      top: '2%',
      textStyle: { color: '#8b9cb5', fontSize: 12 }
    },
    xAxis: {
      type: 'category',
      data: hours,
      axisLine: { lineStyle: { color: '#2b374e' } },
      axisLabel: { color: '#8b9cb5', interval: 2, fontSize: 11 }
    },
    yAxis: {
      type: 'value',
      splitLine: { lineStyle: { color: 'rgba(43, 55, 78, 0.4)', type: 'dashed' } },
      axisLabel: { color: '#8b9cb5', fontSize: 11 }
    },
    series: [
      {
        name: '进入量',
        type: 'bar',
        data: entryArr,
        barWidth: '35%',
        itemStyle: {
          borderRadius: [4, 4, 0, 0],
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: '#00f0ff' },
            { offset: 1, color: '#0055ff' }
          ])
        }
      },
      {
        name: '离开量',
        type: 'bar',
        data: exitArr,
        barWidth: '35%',
        itemStyle: {
          borderRadius: [4, 4, 0, 0],
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: '#ff3366' },
            { offset: 1, color: '#800033' }
          ])
        }
      }
    ]
  };
  hourlyChart.setOption(option);
};

// 3. 客群年龄段画像环形图
const renderAgeChart = () => {
  if (!ageChartRef.value) return;
  if (!ageChart) {
    ageChart = echarts.init(ageChartRef.value);
  }

  const option = {
    backgroundColor: 'transparent',
    tooltip: { trigger: 'item', backgroundColor: 'rgba(16, 26, 44, 0.95)', borderColor: '#00f0ff' },
    legend: { orient: 'vertical', right: '5%', top: 'center', textStyle: { color: '#8b9cb5', fontSize: 11 } },
    series: [
      {
        name: '年龄客层',
        type: 'pie',
        radius: ['45%', '72%'],
        center: ['40%', '50%'],
        avoidLabelOverlap: false,
        label: { show: false },
        emphasis: { label: { show: true, fontSize: 13, fontWeight: 'bold', color: '#00f0ff' } },
        data: [
          { value: 68, name: '青年(18-35)', itemStyle: { color: '#00f0ff' } },
          { value: 22, name: '中年(36-50)', itemStyle: { color: '#0088ff' } },
          { value: 7, name: '中老年(50+)', itemStyle: { color: '#7928ca' } },
          { value: 3, name: '儿童少年', itemStyle: { color: '#ff007f' } }
        ]
      }
    ]
  };
  ageChart.setOption(option);
};

// ==================== 格式化辅助方法 ====================
const formatNumber = (num: number) => {
  return (num || 0).toLocaleString();
};

const formatTime = (ts: any) => {
  if (!ts) return '--:--:--';
  if (typeof ts === 'string' && ts.length >= 8) {
    return ts.substring(ts.length - 8);
  }
  const d = new Date(Number(ts));
  const pad = (n: number) => (n < 10 ? '0' + n : '' + n);
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
};

// ==================== 轮询与生命周期 ====================
let dataPollingTimer: any = null;

onMounted(() => {
  updateScale();
  window.addEventListener('resize', updateScale);

  updateClock();
  clockTimer = setInterval(updateClock, 1000);

  // 初次拉取数据与渲染图表
  nextTick(() => {
    loadUserProfileAndTenants();
    fetchData();
  });

  // 每 4 秒拉取一次最新缓存态势
  dataPollingTimer = setInterval(fetchData, 4000);

  // 轮巡定时器
  tourTimer = setInterval(() => {
    if (!autoTourEnabled.value) return;
    tourCountdown.value--;
    if (tourCountdown.value <= 0) {
      tourCountdown.value = tourIntervalSec;
      triggerNextTourPoint();
    }
  }, 1000);
});

onUnmounted(() => {
  window.removeEventListener('resize', updateScale);
  if (clockTimer) clearInterval(clockTimer);
  if (dataPollingTimer) clearInterval(dataPollingTimer);
  if (tourTimer) clearInterval(tourTimer);

  if (flowChart) flowChart.dispose();
  if (hourlyChart) hourlyChart.dispose();
  if (ageChart) ageChart.dispose();
});
</script>

<style scoped>
/* ==================== 页面全屏与自适应容器 ==================== */
.screen-wrapper {
  position: fixed;
  top: 0;
  left: 0;
  width: 100vw;
  height: 100vh;
  background-color: #06090f;
  overflow: hidden;
  user-select: none;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
  z-index: 9999;
}

.screen-canvas {
  position: absolute;
  top: 50%;
  left: 50%;
  width: 1920px;
  height: 1080px;
  background: radial-gradient(circle at 50% 30%, #0d1526 0%, #06090f 85%);
  overflow: hidden;
  display: flex;
  flex-direction: column;
}

/* 科技网格与光晕背景 */
.tech-grid-bg {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background-image: linear-gradient(rgba(0, 240, 255, 0.03) 1px, transparent 1px),
    linear-gradient(90deg, rgba(0, 240, 255, 0.03) 1px, transparent 1px);
  background-size: 40px 40px;
  pointer-events: none;
}

.ambient-glow {
  position: absolute;
  width: 800px;
  height: 300px;
  border-radius: 50%;
  filter: blur(120px);
  pointer-events: none;
  opacity: 0.15;
}
.glow-top {
  top: -100px;
  left: 50%;
  transform: translateX(-50%);
  background: #00f0ff;
}
.glow-bottom {
  bottom: -100px;
  left: 50%;
  transform: translateX(-50%);
  background: #0055ff;
}

/* ==================== 顶部 HUD 科技顶栏 ==================== */
.screen-header {
  position: relative;
  height: 78px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 28px;
  background: linear-gradient(180deg, rgba(16, 26, 44, 0.95) 0%, rgba(6, 9, 15, 0.3) 100%);
  border-bottom: 1px solid rgba(0, 240, 255, 0.25);
  z-index: 10;
}

.header-left {
  display: flex;
  align-items: center;
  gap: 20px;
  width: 440px;
}

.clock-box {
  display: flex;
  align-items: center;
  gap: 12px;
}
.clock-time {
  font-family: 'Segoe UI', monospace, sans-serif;
  font-size: 26px;
  font-weight: 700;
  color: #00f0ff;
  letter-spacing: 1px;
  text-shadow: 0 0 10px rgba(0, 240, 255, 0.6);
}
.clock-date-group {
  display: flex;
  flex-direction: column;
  font-size: 11px;
  color: #8b9cb5;
}

.status-badge {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 4px 12px;
  border-radius: 14px;
  background: rgba(0, 240, 255, 0.08);
  border: 1px solid rgba(0, 240, 255, 0.3);
  font-size: 12px;
}
.status-badge.node-online {
  color: #00f0ff;
}
.status-badge.node-offline {
  color: #ff3366;
  border-color: rgba(255, 51, 102, 0.3);
  background: rgba(255, 51, 102, 0.08);
}
.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background-color: #00f0ff;
  box-shadow: 0 0 8px #00f0ff;
  animation: pulse-beacon 1.8s infinite;
}
.node-offline .status-dot {
  background-color: #ff3366;
  box-shadow: 0 0 8px #ff3366;
}

.header-center {
  position: absolute;
  left: 50%;
  transform: translateX(-50%);
  text-align: center;
}
.title-trapezoid {
  position: relative;
  padding: 8px 60px 4px 60px;
  background: linear-gradient(180deg, rgba(0, 114, 255, 0.25) 0%, rgba(0, 240, 255, 0.05) 100%);
  border-bottom: 2px solid #00f0ff;
  clip-path: polygon(0 0, 100% 0, 92% 100%, 8% 100%);
  display: flex;
  flex-direction: column;
  align-items: center;
}
.title-glow {
  font-size: 26px;
  font-weight: 800;
  letter-spacing: 3px;
  background: linear-gradient(180deg, #ffffff 0%, #00f0ff 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  filter: drop-shadow(0 0 12px rgba(0, 240, 255, 0.5));
}
.title-sub {
  font-size: 9px;
  color: rgba(0, 240, 255, 0.7);
  letter-spacing: 2px;
  margin-top: 1px;
}

.header-right {
  display: flex;
  align-items: center;
  gap: 12px;
  min-width: 480px;
  justify-content: flex-end;
}
.ctrl-group {
  display: flex;
  align-items: center;
  gap: 10px;
}
.tenant-select {
  width: 175px;
}
.point-select {
  width: 165px;
}
:deep(.el-input__wrapper) {
  background-color: rgba(16, 26, 44, 0.8) !important;
  box-shadow: 0 0 0 1px rgba(0, 240, 255, 0.3) inset !important;
  color: #f0f6fc !important;
}
:deep(.el-input__inner) {
  color: #f0f6fc !important;
  font-size: 12px !important;
}

.hud-btn {
  display: flex;
  align-items: center;
  gap: 5px;
  padding: 5px 12px;
  background: rgba(16, 26, 44, 0.85);
  border: 1px solid rgba(0, 240, 255, 0.3);
  border-radius: 4px;
  color: #c9d1d9;
  font-size: 12px;
  cursor: pointer;
  transition: all 0.25s ease;
}
.hud-btn:hover {
  background: rgba(0, 240, 255, 0.2);
  border-color: #00f0ff;
  color: #00f0ff;
  box-shadow: 0 0 10px rgba(0, 240, 255, 0.3);
}
.hud-btn.active {
  background: rgba(0, 240, 255, 0.25);
  border-color: #00f0ff;
  color: #00f0ff;
}
.exit-btn:hover {
  background: rgba(255, 51, 102, 0.2);
  border-color: #ff3366;
  color: #ff3366;
}

/* ==================== 主体三栏布局 ==================== */
.screen-body {
  flex: 1;
  display: grid;
  grid-template-columns: 460px 1fr 460px;
  gap: 18px;
  padding: 16px 24px 20px 24px;
  box-sizing: border-box;
}

/* 通用 HUD 科技面板 */
.hud-panel {
  position: relative;
  background: rgba(13, 21, 38, 0.75);
  border: 1px solid rgba(0, 240, 255, 0.18);
  border-radius: 6px;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.4);
}
.panel-header {
  position: relative;
  height: 38px;
  background: linear-gradient(90deg, rgba(0, 114, 255, 0.18) 0%, rgba(0, 240, 255, 0.04) 100%);
  border-bottom: 1px solid rgba(0, 240, 255, 0.15);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 14px;
}
.panel-title {
  font-size: 14px;
  font-weight: 700;
  color: #e6edf3;
  display: flex;
  align-items: center;
  gap: 6px;
}
.title-icon {
  font-style: normal;
  color: #00f0ff;
}
.panel-tag {
  font-size: 11px;
  padding: 2px 8px;
  background: rgba(0, 240, 255, 0.12);
  border-radius: 3px;
  color: #00f0ff;
}

/* 科技发光四角边框 */
.corner {
  position: absolute;
  width: 6px;
  height: 6px;
  border-color: #00f0ff;
  border-style: solid;
  pointer-events: none;
}
.corner-tl {
  top: 0;
  left: 0;
  border-width: 2px 0 0 2px;
}
.corner-tr {
  top: 0;
  right: 0;
  border-width: 2px 2px 0 0;
}

.panel-body {
  flex: 1;
  position: relative;
  padding: 12px;
  box-sizing: border-box;
}

/* ==================== 左翼内容 ==================== */
.column-left {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.kpi-panel {
  height: 330px;
}
.kpi-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
}
.kpi-card {
  background: rgba(16, 26, 44, 0.6);
  border: 1px solid rgba(43, 55, 78, 0.6);
  border-radius: 6px;
  padding: 14px 12px;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  position: relative;
  transition: all 0.3s;
}
.kpi-card:hover {
  transform: translateY(-2px);
}
.kpi-label {
  font-size: 12px;
  color: #8b9cb5;
}
.kpi-value {
  font-family: 'Segoe UI', sans-serif;
  font-size: 28px;
  font-weight: 800;
  color: #00f0ff;
  letter-spacing: 0.5px;
  margin: 6px 0;
}
.kpi-value .unit {
  font-size: 12px;
  font-weight: normal;
  color: #8b9cb5;
  margin-left: 4px;
}
.val-green {
  color: #00ff88;
}
.val-blue {
  color: #38bdf8;
}
.val-coral {
  color: #ff3366;
}

.kpi-footer {
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-size: 10px;
  color: #6e7681;
}
.badge-cyan {
  color: #00f0ff;
}
.beacon-dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: #00ff88;
  box-shadow: 0 0 6px #00ff88;
}
.arrow-up {
  color: #38bdf8;
}
.arrow-down {
  color: #ff3366;
}

.flow-panel {
  flex: 1;
}
.legend-group {
  display: flex;
  gap: 12px;
  font-size: 11px;
  color: #8b9cb5;
}
.legend-item {
  display: flex;
  align-items: center;
  gap: 4px;
}
.dot {
  width: 6px;
  height: 6px;
  border-radius: 50%;
}
.dot-cyan {
  background: #00f0ff;
  box-shadow: 0 0 6px #00f0ff;
}
.dot-coral {
  background: #ff3366;
  box-shadow: 0 0 6px #ff3366;
}
.chart-body {
  padding: 4px;
}
.echarts-container {
  width: 100%;
  height: 100%;
}

/* ==================== 中翼内容 ==================== */
.column-center {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.video-panel {
  height: 570px;
}
.video-container {
  padding: 8px;
}
.snapshot-frame {
  position: relative;
  width: 100%;
  height: 100%;
  background: #030508;
  border-radius: 4px;
  overflow: hidden;
  display: flex;
  align-items: center;
  justify-content: center;
}
.snapshot-img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}
.snapshot-placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
  color: #8b9cb5;
}
.no-stream-icon {
  font-size: 48px;
}
.no-stream-text {
  font-size: 14px;
  letter-spacing: 1px;
}

/* HUD 视窗准星与科技标尺 */
.hud-reticle {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  pointer-events: none;
}
.crosshair-h {
  position: absolute;
  top: 50%;
  left: 45%;
  width: 10%;
  height: 1px;
  background: rgba(0, 240, 255, 0.4);
}
.crosshair-v {
  position: absolute;
  top: 45%;
  left: 50%;
  width: 1px;
  height: 10%;
  background: rgba(0, 240, 255, 0.4);
}
.bracket {
  position: absolute;
  width: 24px;
  height: 24px;
  border-color: rgba(0, 240, 255, 0.7);
  border-style: solid;
}
.b-tl {
  top: 14px;
  left: 14px;
  border-width: 2px 0 0 2px;
}
.b-tr {
  top: 14px;
  right: 14px;
  border-width: 2px 2px 0 0;
}
.b-bl {
  bottom: 50px;
  left: 14px;
  border-width: 0 0 2px 2px;
}
.b-br {
  bottom: 50px;
  right: 14px;
  border-width: 0 2px 2px 0;
}

.telemetry-bar {
  position: absolute;
  bottom: 0;
  left: 0;
  width: 100%;
  height: 38px;
  background: linear-gradient(180deg, rgba(6, 9, 15, 0) 0%, rgba(6, 9, 15, 0.95) 100%);
  display: flex;
  align-items: center;
  justify-content: space-around;
  padding: 0 16px;
  font-size: 11px;
  color: #8b9cb5;
  border-top: 1px solid rgba(0, 240, 255, 0.2);
}
.tele-item {
  display: flex;
  gap: 6px;
}
.t-v {
  color: #f0f6fc;
  font-weight: 600;
}
.mode-tag {
  color: #00f0ff;
}
.text-green {
  color: #00ff88;
}
.highlight {
  color: #38bdf8;
}

.hourly-panel {
  flex: 1;
}
.peak-badge {
  font-size: 11px;
  color: #00f0ff;
  background: rgba(0, 240, 255, 0.12);
  padding: 2px 8px;
  border-radius: 3px;
}

/* ==================== 右翼内容 ==================== */
.column-right {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.demo-panel {
  height: 330px;
}
.demo-body {
  display: flex;
  flex-direction: column;
  gap: 12px;
  padding: 10px 14px;
}
.demo-sub-section .sub-title {
  font-size: 12px;
  color: #8b9cb5;
  margin-bottom: 6px;
}
.gender-bar-wrapper {
  background: rgba(16, 26, 44, 0.6);
  border: 1px solid rgba(43, 55, 78, 0.6);
  border-radius: 4px;
  padding: 10px 12px;
}
.gender-text {
  display: flex;
  justify-content: space-between;
  font-size: 12px;
  margin-bottom: 6px;
}
.male-label {
  color: #00f0ff;
  font-weight: 600;
}
.female-label {
  color: #ff3366;
  font-weight: 600;
}
.gender-progress {
  display: flex;
  height: 10px;
  border-radius: 5px;
  overflow: hidden;
  background: rgba(43, 55, 78, 0.4);
}
.bar-male {
  background: linear-gradient(90deg, #0088ff, #00f0ff);
  transition: width 0.6s ease;
}
.bar-female {
  background: linear-gradient(90deg, #ff007f, #ff3366);
  transition: width 0.6s ease;
}

.age-chart-section {
  flex: 1;
  display: flex;
  flex-direction: column;
}
.age-chart-container {
  flex: 1;
  width: 100%;
  min-height: 130px;
}

.events-panel {
  flex: 1;
}
.live-dot-pulse {
  font-size: 10px;
  color: #ff3366;
  background: rgba(255, 51, 102, 0.15);
  border: 1px solid rgba(255, 51, 102, 0.4);
  padding: 1px 6px;
  border-radius: 10px;
  animation: pulse-beacon 1.5s infinite;
}

.stream-container {
  padding: 6px;
  overflow: hidden;
}
.event-stream-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
  height: 100%;
  overflow-y: auto;
  padding-right: 4px;
}
/* 隐藏原生滚动条 */
.event-stream-list::-webkit-scrollbar {
  width: 4px;
}
.event-stream-list::-webkit-scrollbar-thumb {
  background: rgba(0, 240, 255, 0.2);
  border-radius: 2px;
}

.stream-card {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 8px 10px;
  border-radius: 4px;
  background: rgba(16, 26, 44, 0.7);
  border-left: 3px solid #00f0ff;
  transition: all 0.25s;
}
.stream-card.card-exit {
  border-left-color: #ff3366;
}
.stream-card:hover {
  background: rgba(0, 240, 255, 0.1);
}
.badge-entry {
  display: inline-block;
  padding: 2px 6px;
  background: rgba(0, 240, 255, 0.15);
  color: #00f0ff;
  border-radius: 3px;
  font-size: 11px;
  font-weight: bold;
}
.badge-exit {
  display: inline-block;
  padding: 2px 6px;
  background: rgba(255, 51, 102, 0.15);
  color: #ff3366;
  border-radius: 3px;
  font-size: 11px;
  font-weight: bold;
}
.card-meta {
  flex: 1;
}
.meta-row {
  display: flex;
  justify-content: space-between;
  font-size: 12px;
  color: #f0f6fc;
}
.sub-row {
  font-size: 11px;
  color: #8b9cb5;
  margin-top: 3px;
}
.ev-conf {
  color: #38bdf8;
}
.card-thumb {
  font-size: 11px;
  color: #00f0ff;
  background: rgba(0, 240, 255, 0.08);
  padding: 3px 6px;
  border-radius: 3px;
  border: 1px solid rgba(0, 240, 255, 0.25);
}

.stream-empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 100%;
  color: #6e7681;
  font-size: 13px;
  gap: 8px;
}

@keyframes pulse-beacon {
  0% {
    opacity: 0.4;
    transform: scale(0.95);
  }
  50% {
    opacity: 1;
    transform: scale(1.05);
  }
  100% {
    opacity: 0.4;
    transform: scale(0.95);
  }
}
</style>
