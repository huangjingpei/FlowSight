<template>
  <div class="dashboard-page">
    <!-- 顶部导航与多点位流切换组件 (核心约束: 视频播放时 Disable 锁定禁止切换) -->
    <van-nav-bar title="FlowSight 运营看板" fixed placeholder>
      <template #right>
        <van-icon name="revoke" size="18" @click="handleLogout" />
      </template>
    </van-nav-bar>

    <!-- 顶部点位选择器: 视频播放中禁止切换 -->
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

    <van-pull-refresh v-model="refreshing" @refresh="onRefresh">
      <!-- 4 大核心客流数显矩阵 -->
      <div class="kpi-grid">
        <div class="kpi-card card-blue">
          <div class="kpi-label">当前区域滞留</div>
          <div class="kpi-value">{{ summary.currentOccupancy }} <span class="unit">人</span></div>
          <div class="kpi-sub">● 瞬时在场感知</div>
        </div>
        <div class="kpi-card card-green">
          <div class="kpi-label">今日累计进入 (In)</div>
          <div class="kpi-value">{{ summary.todayEntry }} <span class="unit">人次</span></div>
          <div class="kpi-sub">↑ 进门通行累计</div>
        </div>
        <div class="kpi-card card-red">
          <div class="kpi-label">今日累计离开 (Out)</div>
          <div class="kpi-value">{{ summary.todayExit }} <span class="unit">人次</span></div>
          <div class="kpi-sub">↓ 出门通行累计</div>
        </div>
        <div class="kpi-card card-orange">
          <div class="kpi-label">全天通行总客流</div>
          <div class="kpi-value">{{ summary.todayTotal }} <span class="unit">人次</span></div>
          <div class="kpi-sub">∑ 进出双向汇总</div>
        </div>
      </div>

      <!-- 客群画像模块: 性别比例与主要年龄段 -->
      <div class="section-card">
        <div class="section-title">👥 客群画像与年龄分布</div>
        <div class="demo-box" v-if="summary.demographics">
          <div class="gender-bar-wrapper">
            <div class="gender-header">
              <span>男: {{ (summary.demographics.maleRatio * 100).toFixed(1) }}% ({{ summary.demographics.maleCount }}人)</span>
              <span>女: {{ (summary.demographics.femaleRatio * 100).toFixed(1) }}% ({{ summary.demographics.femaleCount }}人)</span>
            </div>
            <div class="gender-bar">
              <div class="gender-male" :style="{ width: (summary.demographics.maleRatio * 100) + '%' }"></div>
              <div class="gender-female" :style="{ width: (summary.demographics.femaleRatio * 100) + '%' }"></div>
            </div>
          </div>
          <div class="age-tag-box">
            <span class="age-label">主要到店客群年龄:</span>
            <van-tag type="primary" size="medium">{{ summary.demographics.topAgeGroup || '青年(18-35)' }}</van-tag>
          </div>
        </div>
      </div>

      <!-- 24小时通行趋势小图表 -->
      <div class="section-card">
        <div class="section-title">📊 今日通行时段走势</div>
        <div class="hourly-bars">
          <div
            v-for="item in activeHourly"
            :key="item.hour"
            class="hourly-col"
          >
            <div class="bar-fill" :style="{ height: Math.min(100, (item.entry + item.exit) * 5 + 4) + 'px' }"></div>
            <span class="col-label">{{ item.hour }}h</span>
          </div>
        </div>
      </div>

      <!-- 实时通行事件流水与证据视频回溯 -->
      <div class="section-card events-card">
        <div class="section-title">🎬 实时通行抓拍记录流水 (点击回放切片)</div>
        <van-list
          v-model:loading="eventsLoading"
          :finished="eventsFinished"
          finished-text="没有更多了"
          @load="loadEvents"
        >
          <div
            v-for="ev in eventList"
            :key="ev.id"
            class="event-item"
            @click="handlePlayClip(ev)"
          >
            <div class="ev-time">{{ ev.datetime ? ev.datetime.split(' ')[1] : '' }}</div>
            <div :class="['ev-dir', ev.direction === 'ENTRY' ? 'dir-in' : 'dir-out']">
              {{ ev.direction === 'ENTRY' ? '🟢 进' : '🔴 出' }}
            </div>
            <div class="ev-body">
              <div class="ev-target">目标 #{{ ev.trackId }}</div>
              <div class="ev-attr">{{ ev.gender }} / {{ ev.ageGroup }}</div>
            </div>
            <div class="ev-action">
              <van-button size="mini" type="primary" plain round icon="play">
                回放切片
              </van-button>
            </div>
          </div>
        </van-list>
      </div>
    </van-pull-refresh>

    <!-- 视频切片回放弹窗组件 -->
    <VideoReplayModal
      v-model="showVideoModal"
      :play-url="currentPlayUrl"
      :event-info="currentEvent"
    />
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue';
import { useRouter } from 'vue-router';
import { showToast, showDialog } from 'vant';
import { h5Api } from '../api';
import { usePointStore } from '../store/point';
import VideoReplayModal from '../components/VideoReplayModal.vue';

const router = useRouter();
const pointStore = usePointStore();

const activePointId = ref<number>(0);
const refreshing = ref(false);
const summary = ref<any>({
  currentOccupancy: 0,
  todayEntry: 0,
  todayExit: 0,
  todayTotal: 0,
  demographics: { maleRatio: 0.5, femaleRatio: 0.5, maleCount: 0, femaleCount: 0, topAgeGroup: '青年(18-35)' },
  hourlyChart: []
});

const eventList = ref<any[]>([]);
const eventsLoading = ref(false);
const eventsFinished = ref(false);
const currentPage = ref(1);

const showVideoModal = ref(false);
const currentPlayUrl = ref('');
const currentEvent = ref<any>(null);

let timer: any = null;

const pointOptions = computed(() => {
  return pointStore.points.map((p) => ({
    text: `${p.pointName} (${p.streamStatus === 'ONLINE' ? '在线' : '离线'})`,
    value: p.id
  }));
});

const activeHourly = computed(() => {
  if (!summary.value.hourlyChart || summary.value.hourlyChart.length === 0) {
    return [];
  }
  return summary.value.hourlyChart.filter((h: any) => h.hour >= 8 && h.hour <= 22);
});

const loadPoints = async () => {
  try {
    const list: any = await h5Api.getMyPoints();
    pointStore.setPoints(list);
    if (list.length > 0 && !activePointId.value) {
      activePointId.value = list[0].id;
    }
  } catch (e) {
    //
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

const loadEvents = async () => {
  if (!activePointId.value) return;
  eventsLoading.value = true;
  try {
    const res: any = await h5Api.getEvents(activePointId.value, currentPage.value, 20);
    const records = res.records || [];
    if (currentPage.value === 1) {
      eventList.value = records;
    } else {
      eventList.value.push(...records);
    }
    if (records.length < 20) {
      eventsFinished.value = true;
    } else {
      currentPage.value++;
    }
  } catch (e) {
    eventsFinished.value = true;
  } finally {
    eventsLoading.value = false;
  }
};

const onPointChange = (val: number) => {
  if (pointStore.isMediaPlaying) {
    showToast('视频回放中禁止切换点位！');
    return;
  }
  activePointId.value = val;
  currentPage.value = 1;
  eventsFinished.value = false;
  loadSummary();
  loadEvents();
};

const onRefresh = async () => {
  currentPage.value = 1;
  eventsFinished.value = false;
  await Promise.all([loadSummary(), loadEvents()]);
  refreshing.value = false;
  showToast('数据已刷新');
};

const handlePlayClip = (ev: any) => {
  if (!ev.playUrl) {
    showToast('该事件暂未关联切片视频');
    return;
  }
  currentPlayUrl.value = ev.playUrl;
  currentEvent.value = ev;
  showVideoModal.value = true;
};

const handleLogout = () => {
  showDialog({
    title: '提示',
    message: '确认退出当前账号吗？',
    showCancelButton: true
  }).then(() => {
    localStorage.removeItem('flowsight_token');
    router.push('/login');
  });
};

onMounted(async () => {
  await loadPoints();
  if (activePointId.value) {
    loadSummary();
    loadEvents();
  }
  // 每 15 秒定时轮询 (视频播放中由状态机互斥跳过)
  timer = setInterval(() => {
    if (!pointStore.isMediaPlaying && activePointId.value) {
      loadSummary();
    }
  }, 15000);
});

onUnmounted(() => {
  if (timer) clearInterval(timer);
});
</script>

<style scoped>
.dashboard-page {
  padding-bottom: 24px;
  background-color: #f7f8fa;
  min-height: 100vh;
}

.point-switcher-bar {
  background: #fff;
  border-bottom: 1px solid #ebedf0;
  position: relative;
}

.locked-tip {
  padding: 4px 16px;
  font-size: 11px;
  color: #ff976a;
  background-color: #fffbe8;
  display: flex;
  align-items: center;
}

.kpi-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 12px;
  padding: 12px;
}

.kpi-card {
  border-radius: 8px;
  padding: 14px 12px;
  color: #fff;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
}

.card-blue {
  background: linear-gradient(135deg, #1989fa, #0570db);
}
.card-green {
  background: linear-gradient(135deg, #07c160, #049f4e);
}
.card-red {
  background: linear-gradient(135deg, #ee0a24, #c8081e);
}
.card-orange {
  background: linear-gradient(135deg, #ff976a, #ed6a0c);
}

.kpi-label {
  font-size: 12px;
  opacity: 0.9;
  margin-bottom: 4px;
}

.kpi-value {
  font-size: 26px;
  font-weight: bold;
  font-family: 'Consolas', monospace;
}

.kpi-value .unit {
  font-size: 12px;
  font-weight: normal;
}

.kpi-sub {
  font-size: 11px;
  opacity: 0.8;
  margin-top: 4px;
}

.section-card {
  background: #fff;
  margin: 0 12px 12px 12px;
  border-radius: 8px;
  padding: 14px;
  box-shadow: 0 1px 4px rgba(0, 0, 0, 0.04);
}

.section-title {
  font-size: 14px;
  font-weight: 600;
  color: #323233;
  margin-bottom: 12px;
}

.gender-bar-wrapper {
  margin-bottom: 12px;
}

.gender-header {
  display: flex;
  justify-content: space-between;
  font-size: 12px;
  color: #666;
  margin-bottom: 6px;
}

.gender-bar {
  height: 10px;
  border-radius: 5px;
  display: flex;
  overflow: hidden;
  background-color: #ebedf0;
}

.gender-male {
  background-color: #1989fa;
}

.gender-female {
  background-color: #ee0a24;
}

.age-tag-box {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
  color: #555;
}

.hourly-bars {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  height: 120px;
  padding: 10px 0;
  border-bottom: 1px solid #eee;
}

.hourly-col {
  display: flex;
  flex-direction: column;
  align-items: center;
  flex: 1;
}

.bar-fill {
  width: 8px;
  background: linear-gradient(180deg, #1989fa, #a0cfff);
  border-radius: 4px 4px 0 0;
  min-height: 4px;
}

.col-label {
  font-size: 9px;
  color: #999;
  margin-top: 4px;
}

.event-item {
  display: flex;
  align-items: center;
  padding: 10px 0;
  border-bottom: 1px solid #f2f3f5;
  font-size: 13px;
}

.ev-time {
  font-family: monospace;
  color: #999;
  width: 65px;
}

.ev-dir {
  width: 45px;
  font-weight: 600;
}

.dir-in {
  color: #07c160;
}

.dir-out {
  color: #ee0a24;
}

.ev-body {
  flex: 1;
}

.ev-target {
  font-weight: 500;
  color: #333;
}

.ev-attr {
  font-size: 11px;
  color: #888;
}

.ev-action {
  margin-left: 8px;
}
</style>
