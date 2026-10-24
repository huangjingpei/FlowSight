<template>
  <div class="playback-page">
    <!-- 顶部导航栏 -->
    <van-nav-bar fixed placeholder class="gov-nav-bar">
      <template #left>
        <div class="nav-brand">
          <span class="brand-logo">🎬</span>
          <span class="brand-title">FlowSight 证据切片回放中心</span>
        </div>
      </template>
      <template #right>
        <div class="header-point-badge" v-if="selectedPointName">
          <span>{{ selectedPointName }}</span>
        </div>
      </template>
    </van-nav-bar>

    <div class="playback-body">
      <!-- 1. 多维度组合搜索过滤卡片 -->
      <div class="filter-card">
        <div class="filter-title">
          <span>🔍</span> 证据切片条件检索
        </div>

        <!-- 筛选行 1: 通道点位选择 -->
        <div class="filter-row">
          <span class="row-label">通道点位:</span>
          <div class="point-select-wrap">
            <select v-model="filterPointId" class="custom-select" @change="onSearch">
              <option :value="0">🌐 全部通道点位</option>
              <option v-for="p in pointOptions" :key="p.id" :value="p.id">
                [{{ p.id }}] {{ p.pointName }}
              </option>
            </select>
          </div>
        </div>

        <!-- 筛选行 2: 通行方向 -->
        <div class="filter-row">
          <span class="row-label">通行方向:</span>
          <div class="dir-btn-group">
            <div
              :class="['dir-btn', filterDirection === 'ALL' ? 'active-all' : '']"
              @click="setDirection('ALL')"
            >
              全部
            </div>
            <div
              :class="['dir-btn', filterDirection === 'ENTRY' ? 'active-entry' : '']"
              @click="setDirection('ENTRY')"
            >
              🟢 进入 (In)
            </div>
            <div
              :class="['dir-btn', filterDirection === 'EXIT' ? 'active-exit' : '']"
              @click="setDirection('EXIT')"
            >
              🟠 离开 (Out)
            </div>
          </div>
        </div>

        <!-- 筛选行 3: 快捷日期范围 -->
        <div class="filter-row">
          <span class="row-label">检索时间:</span>
          <div class="date-quick-group">
            <div
              v-for="d in dateShortcuts"
              :key="d.key"
              :class="['date-chip', activeDateShortcut === d.key ? 'active' : '']"
              @click="setDateShortcut(d.key)"
            >
              {{ d.label }}
            </div>
          </div>
        </div>

        <!-- 操作按钮与当前筛选说明 -->
        <div class="filter-actions">
          <span class="filter-summary">
            {{ selectedDateRangeText }}
          </span>
          <div class="action-btns">
            <button class="btn-reset" @click="onReset">↺ 重置</button>
            <button class="btn-search" @click="onSearch">🔍 查询</button>
          </div>
        </div>
      </div>

      <!-- 2. 切片回放记录列表 -->
      <div class="records-card">
        <div class="records-header">
          <div class="header-left">
            <span class="header-icon">📹</span>
            <span class="header-title">事件切片列表</span>
            <span class="total-tag">共 {{ totalRecords }} 条</span>
          </div>
          <div class="page-size-selector">
            <span class="size-lbl">每页:</span>
            <select v-model="pageSize" class="size-select" @change="onPageSizeChange">
              <option :value="10">10条</option>
              <option :value="20">20条</option>
              <option :value="50">50条</option>
            </select>
          </div>
        </div>

        <!-- 加载中状态 -->
        <div v-if="loading" class="loading-box">
          <van-loading type="spinner" color="#58a6ff" size="28px">
            正在检索切片证据流水...
          </van-loading>
        </div>

        <!-- 空数据提示 -->
        <div v-else-if="eventList.length === 0" class="empty-box">
          <van-empty image="search" description="未检索到符合条件的通行切片数据" />
        </div>

        <!-- 记录卡片列表 -->
        <div v-else class="event-cards-list">
          <div
            v-for="ev in eventList"
            :key="ev.id"
            class="event-card"
            @click="handlePlayClip(ev)"
          >
            <div class="card-top">
              <div class="point-badge">
                <span class="loc-icon">📍</span>
                <span class="loc-name">{{ ev.pointName || ('通道 #' + ev.pointId) }}</span>
              </div>
              <div :class="['dir-pill', ev.direction === 'ENTRY' ? 'pill-in' : 'pill-out']">
                {{ ev.direction === 'ENTRY' ? '🟢 进入' : '🟠 离开' }}
              </div>
              <div class="card-time">{{ ev.datetime }}</div>
            </div>

            <div class="card-middle">
              <div class="target-meta">
                <span class="track-tag">目标 #{{ ev.trackId }}</span>
                <span class="attr-tag">{{ ev.gender || '未知' }}</span>
                <span class="attr-tag">{{ ev.ageGroup || '未知' }}</span>
                <span class="conf-tag" v-if="ev.confidence">
                  置信度: {{ (ev.confidence * 100).toFixed(0) }}%
                </span>
              </div>
              <div class="play-action-wrap">
                <button class="btn-play-clip">
                  <span class="play-icon">▶</span>
                  <span>播放切片</span>
                </button>
              </div>
            </div>
          </div>
        </div>

        <!-- 3. 全功能综合分页条 (首页 / 上一页 / 选页 / 下一页 / 尾页 / 跳转) -->
        <div class="pagination-footer" v-if="totalRecords > 0">
          <div class="pagination-info">
            第 <span class="num-highlight">{{ currentPage }}</span> / {{ totalPages }} 页 (共 {{ totalRecords }} 条)
          </div>

          <!-- 分页按钮控制条 -->
          <div class="pagination-bar">
            <button
              class="page-nav-btn"
              :disabled="currentPage <= 1"
              @click="goToPage(1)"
              title="跳转到第一页"
            >
              ⏮ 首页
            </button>
            <button
              class="page-nav-btn"
              :disabled="currentPage <= 1"
              @click="goToPage(currentPage - 1)"
            >
              ◀ 上一页
            </button>

            <!-- 中间当前页码胶囊 -->
            <div class="page-current-pill">
              <span>{{ currentPage }} / {{ totalPages }}</span>
            </div>

            <button
              class="page-nav-btn"
              :disabled="currentPage >= totalPages"
              @click="goToPage(currentPage + 1)"
            >
              下一页 ▶
            </button>
            <button
              class="page-nav-btn"
              :disabled="currentPage >= totalPages"
              @click="goToPage(totalPages)"
              title="跳转到最后一页"
            >
              尾页 ⏭
            </button>
          </div>

          <!-- 快速跳转指定页码 -->
          <div class="jump-row">
            <span class="jump-label">跳转至:</span>
            <input
              type="number"
              v-model.number="jumpTargetPage"
              class="jump-input"
              :min="1"
              :max="totalPages"
              @keyup.enter="handleJump"
            />
            <span class="jump-unit">页</span>
            <button class="btn-jump" @click="handleJump">前往</button>
          </div>
        </div>
      </div>
    </div>

    <!-- 视频切片标准秒开回放弹窗 (触发状态机互斥锁) -->
    <VideoReplayModal
      v-model:show="showVideoModal"
      :play-url="currentPlayUrl"
      :event-data="currentEvent"
    />
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue';
import { showToast } from 'vant';
import { h5Api, EventQueryParams } from '../api';
import { usePointStore } from '../store/point';
import VideoReplayModal from '../components/VideoReplayModal.vue';

const pointStore = usePointStore();

// 筛选状态
const filterPointId = ref<number>(0);
const filterDirection = ref<string>('ALL');
const activeDateShortcut = ref<string>('today');
const startDate = ref<string>('');
const endDate = ref<string>('');

// 分页状态
const currentPage = ref<number>(1);
const pageSize = ref<number>(10);
const totalRecords = ref<number>(0);
const totalPages = ref<number>(1);
const jumpTargetPage = ref<number>(1);

// 数据列表与加载状态
const loading = ref<boolean>(false);
const eventList = ref<any[]>([]);
const pointOptions = ref<any[]>([]);

// 视频回放弹窗
const showVideoModal = ref<boolean>(false);
const currentPlayUrl = ref<string>('');
const currentEvent = ref<any>(null);

// 快捷日期选项
const dateShortcuts = [
  { key: 'today', label: '今天' },
  { key: 'yesterday', label: '昨天' },
  { key: 'week', label: '近7天' },
  { key: 'all', label: '全部时间' }
];

const selectedPointName = computed(() => {
  if (filterPointId.value === 0) return '全部点位';
  const found = pointOptions.value.find(p => p.id === filterPointId.value);
  return found ? found.pointName : '';
});

const selectedDateRangeText = computed(() => {
  if (activeDateShortcut.value === 'today') return '已选: 今天全天通行记录';
  if (activeDateShortcut.value === 'yesterday') return '已选: 昨天全天记录';
  if (activeDateShortcut.value === 'week') return '已选: 近 7 天事件流水';
  if (activeDateShortcut.value === 'all') return '已选: 不限时间跨度';
  return `已选: ${startDate.value} ~ ${endDate.value}`;
});

// 计算日期快捷参数
const calculateDateRange = (key: string) => {
  const now = new Date();
  const formatYMD = (d: Date) => {
    const y = d.getFullYear();
    const m = String(d.getMonth() + 1).padStart(2, '0');
    const day = String(d.getDate()).padStart(2, '0');
    return `${y}-${m}-${day}`;
  };

  if (key === 'today') {
    const s = formatYMD(now);
    startDate.value = s;
    endDate.value = s;
  } else if (key === 'yesterday') {
    const yest = new Date(now.getTime() - 24 * 3600 * 1000);
    const s = formatYMD(yest);
    startDate.value = s;
    endDate.value = s;
  } else if (key === 'week') {
    const weekAgo = new Date(now.getTime() - 6 * 24 * 3600 * 1000);
    startDate.value = formatYMD(weekAgo);
    endDate.value = formatYMD(now);
  } else {
    startDate.value = '';
    endDate.value = '';
  }
};

const setDateShortcut = (key: string) => {
  activeDateShortcut.value = key;
  calculateDateRange(key);
  currentPage.value = 1;
  fetchEvents();
};

const setDirection = (dir: string) => {
  filterDirection.value = dir;
  currentPage.value = 1;
  fetchEvents();
};

const onSearch = () => {
  currentPage.value = 1;
  fetchEvents();
};

const onReset = () => {
  filterPointId.value = 0;
  filterDirection.value = 'ALL';
  activeDateShortcut.value = 'today';
  calculateDateRange('today');
  currentPage.value = 1;
  pageSize.value = 10;
  fetchEvents();
};

const onPageSizeChange = () => {
  currentPage.value = 1;
  fetchEvents();
};

const goToPage = (page: number) => {
  if (page < 1 || page > totalPages.value || page === currentPage.value) return;
  currentPage.value = page;
  jumpTargetPage.value = page;
  fetchEvents();
};

const handleJump = () => {
  if (!jumpTargetPage.value || jumpTargetPage.value < 1) {
    showToast('请输入有效的目标页码');
    return;
  }
  let target = jumpTargetPage.value;
  if (target > totalPages.value) target = totalPages.value;
  goToPage(target);
};

// 获取事件分页列表
const fetchEvents = async () => {
  loading.value = true;
  try {
    const defaultPid = pointOptions.value.length > 0 ? pointOptions.value[0].id : 1;
    const pid = filterPointId.value > 0 ? filterPointId.value : defaultPid;

    const params: EventQueryParams = {
      pointId: pid,
      direction: filterDirection.value !== 'ALL' ? filterDirection.value : undefined,
      startDate: startDate.value || undefined,
      endDate: endDate.value || undefined,
      page: currentPage.value,
      size: pageSize.value
    };

    const res: any = await h5Api.getEvents(params);
    if (res && res.records) {
      eventList.value = res.records;
      totalRecords.value = res.total || res.records.length;
      totalPages.value = Math.max(1, Math.ceil(totalRecords.value / pageSize.value));
      currentPage.value = res.current || 1;
      jumpTargetPage.value = currentPage.value;
    } else {
      eventList.value = [];
      totalRecords.value = 0;
      totalPages.value = 1;
    }
  } catch (err: any) {
    showToast(err.message || '加载事件切片失败');
  } finally {
    loading.value = false;
  }
};

// 获取点位列表
const fetchPoints = async () => {
  try {
    const res: any = await h5Api.getMyPoints();
    if (res && Array.isArray(res) && res.length > 0) {
      pointOptions.value = res;
      if (!filterPointId.value) {
        filterPointId.value = res[0].id;
      }
    }
  } catch (e) {
    console.error('获取点位失败', e);
  }
};

// 点击播放视频切片
const handlePlayClip = (ev: any) => {
  if (!ev.playUrl) {
    showToast('该记录尚未关联或生成有效切片视频');
    return;
  }
  currentEvent.value = ev;
  currentPlayUrl.value = ev.playUrl;
  showVideoModal.value = true;
};

onMounted(async () => {
  calculateDateRange('today');
  await fetchPoints();
  await fetchEvents();
});
</script>

<style scoped>
.playback-page {
  max-width: 520px;
  margin: 0 auto;
  min-height: 100vh;
  background-color: #060911;
  color: #c9d1d9;
  padding-bottom: 72px; /* 为全局底部导航栏留足空间 */
  box-sizing: border-box;
}

/* 顶部政务物业风格导航 */
.gov-nav-bar {
  max-width: 520px !important;
  left: 50% !important;
  transform: translateX(-50%) !important;
  background: rgba(10, 15, 29, 0.85) !important;
  backdrop-filter: blur(20px) !important;
  -webkit-backdrop-filter: blur(20px) !important;
  border-bottom: 1px solid rgba(255, 255, 255, 0.08) !important;
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
  color: #f0f6fc;
  font-size: 15px;
  font-weight: 700;
  letter-spacing: 0.5px;
}

.header-point-badge {
  background: #21262d;
  color: #58a6ff;
  border: 1px solid #388bfd33;
  padding: 3px 8px;
  border-radius: 12px;
  font-size: 11px;
  font-weight: 600;
}

.playback-body {
  padding: 12px 14px;
}

/* 1. 过滤检索卡片 */
.filter-card {
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 8px;
  padding: 14px;
  margin-bottom: 12px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
}

.filter-title {
  font-size: 14px;
  font-weight: 700;
  color: #58a6ff;
  margin-bottom: 12px;
  display: flex;
  align-items: center;
  gap: 6px;
}

.filter-row {
  display: flex;
  align-items: center;
  margin-bottom: 10px;
  font-size: 13px;
}

.row-label {
  width: 72px;
  color: #8b949e;
  font-weight: 500;
  flex-shrink: 0;
}

.point-select-wrap {
  flex: 1;
}

.custom-select {
  width: 100%;
  background: #0d1117;
  color: #f0f6fc;
  border: 1px solid #30363d;
  border-radius: 6px;
  padding: 6px 10px;
  font-size: 13px;
  outline: none;
}

.dir-btn-group {
  display: flex;
  gap: 8px;
  flex: 1;
}

.dir-btn {
  flex: 1;
  text-align: center;
  padding: 6px 0;
  background: #0d1117;
  color: #8b949e;
  border: 1px solid #30363d;
  border-radius: 6px;
  font-size: 12px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.2s;
}

.dir-btn.active-all {
  background: #1f6feb;
  color: #ffffff;
  border-color: #388bfd;
  font-weight: bold;
}

.dir-btn.active-entry {
  background: #238636;
  color: #ffffff;
  border-color: #2ea043;
  font-weight: bold;
}

.dir-btn.active-exit {
  background: #d29922;
  color: #0d1117;
  border-color: #d29922;
  font-weight: bold;
}

.date-quick-group {
  display: flex;
  gap: 6px;
  flex: 1;
}

.date-chip {
  flex: 1;
  text-align: center;
  padding: 5px 0;
  background: #0d1117;
  color: #8b949e;
  border: 1px solid #30363d;
  border-radius: 4px;
  font-size: 12px;
  cursor: pointer;
  transition: all 0.2s;
}

.date-chip.active {
  background: #388bfd26;
  color: #58a6ff;
  border-color: #58a6ff;
  font-weight: 600;
}

.filter-actions {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-top: 12px;
  padding-top: 10px;
  border-top: 1px solid #21262d;
}

.filter-summary {
  font-size: 11px;
  color: #8b949e;
}

.action-btns {
  display: flex;
  gap: 8px;
}

.btn-reset {
  background: #21262d;
  color: #c9d1d9;
  border: 1px solid #30363d;
  padding: 5px 12px;
  border-radius: 4px;
  font-size: 12px;
  cursor: pointer;
}

.btn-search {
  background: #1f6feb;
  color: #ffffff;
  border: 1px solid #388bfd;
  padding: 5px 16px;
  border-radius: 4px;
  font-size: 12px;
  font-weight: bold;
  cursor: pointer;
}

/* 2. 切片记录列表卡片 */
.records-card {
  background: #161b22;
  border: 1px solid #30363d;
  border-radius: 8px;
  padding: 14px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
}

.records-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 12px;
  padding-bottom: 8px;
  border-bottom: 1px solid #21262d;
}

.header-left {
  display: flex;
  align-items: center;
  gap: 6px;
}

.header-icon {
  font-size: 15px;
}

.header-title {
  font-size: 14px;
  font-weight: 700;
  color: #f0f6fc;
}

.total-tag {
  background: #21262d;
  color: #58a6ff;
  padding: 2px 6px;
  border-radius: 10px;
  font-size: 11px;
  margin-left: 4px;
}

.page-size-selector {
  display: flex;
  align-items: center;
  gap: 4px;
  font-size: 12px;
  color: #8b949e;
}

.size-select {
  background: #0d1117;
  color: #c9d1d9;
  border: 1px solid #30363d;
  border-radius: 4px;
  padding: 2px 4px;
  font-size: 11px;
  outline: none;
}

.loading-box, .empty-box {
  padding: 30px 0;
  text-align: center;
}

/* 事件证据卡片样式 */
.event-cards-list {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.event-card {
  background: #0d1117;
  border: 1px solid #21262d;
  border-radius: 6px;
  padding: 10px 12px;
  transition: all 0.2s;
  cursor: pointer;
}

.event-card:hover {
  border-color: #388bfd;
  background: #111822;
}

.card-top {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 8px;
}

.point-badge {
  background: #1f242c;
  color: #79c0ff;
  font-size: 11px;
  font-weight: 600;
  padding: 2px 6px;
  border-radius: 4px;
  display: flex;
  align-items: center;
  gap: 2px;
}

.dir-pill {
  padding: 2px 6px;
  border-radius: 4px;
  font-size: 11px;
  font-weight: 700;
}

.pill-in {
  background: #23863626;
  color: #3fb950;
  border: 1px solid #2386364d;
}

.pill-out {
  background: #d2992226;
  color: #e3b341;
  border: 1px solid #d299224d;
}

.card-time {
  margin-left: auto;
  font-size: 11px;
  color: #8b949e;
}

.card-middle {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.target-meta {
  display: flex;
  align-items: center;
  gap: 6px;
  flex-wrap: wrap;
}

.track-tag {
  color: #f0f6fc;
  font-weight: 600;
  font-size: 12px;
}

.attr-tag {
  background: #21262d;
  color: #8b949e;
  font-size: 11px;
  padding: 2px 6px;
  border-radius: 4px;
}

.conf-tag {
  color: #58a6ff;
  font-size: 11px;
}

.btn-play-clip {
  background: linear-gradient(135deg, #1f6feb 0%, #238636 100%);
  color: #ffffff;
  border: none;
  padding: 6px 12px;
  border-radius: 14px;
  font-size: 11px;
  font-weight: bold;
  display: flex;
  align-items: center;
  gap: 4px;
  box-shadow: 0 2px 6px rgba(31, 111, 235, 0.4);
  cursor: pointer;
  transition: transform 0.1s;
}

.btn-play-clip:active {
  transform: scale(0.96);
}

.play-icon {
  font-size: 9px;
}

/* 3. 分页控制器组件 */
.pagination-footer {
  margin-top: 14px;
  padding-top: 12px;
  border-top: 1px solid #21262d;
}

.pagination-info {
  text-align: center;
  font-size: 12px;
  color: #8b949e;
  margin-bottom: 8px;
}

.num-highlight {
  color: #58a6ff;
  font-weight: bold;
}

.pagination-bar {
  display: flex;
  justify-content: center;
  align-items: center;
  gap: 6px;
  margin-bottom: 10px;
}

.page-nav-btn {
  background: #21262d;
  color: #c9d1d9;
  border: 1px solid #30363d;
  padding: 5px 8px;
  border-radius: 4px;
  font-size: 11px;
  cursor: pointer;
  transition: all 0.2s;
}

.page-nav-btn:hover:not(:disabled) {
  background: #30363d;
  color: #58a6ff;
  border-color: #58a6ff;
}

.page-nav-btn:disabled {
  opacity: 0.35;
  cursor: not-allowed;
}

.page-current-pill {
  background: #0d1117;
  color: #58a6ff;
  border: 1px solid #388bfd66;
  padding: 5px 10px;
  border-radius: 4px;
  font-size: 11px;
  font-weight: 700;
}

.jump-row {
  display: flex;
  justify-content: center;
  align-items: center;
  gap: 6px;
  font-size: 12px;
  color: #8b949e;
}

.jump-label {
  font-size: 11px;
}

.jump-input {
  width: 48px;
  background: #0d1117;
  color: #f0f6fc;
  border: 1px solid #30363d;
  border-radius: 4px;
  padding: 3px 6px;
  font-size: 12px;
  text-align: center;
  outline: none;
}

.jump-unit {
  font-size: 11px;
}

.btn-jump {
  background: #21262d;
  color: #58a6ff;
  border: 1px solid #388bfd4d;
  padding: 3px 10px;
  border-radius: 4px;
  font-size: 11px;
  font-weight: 600;
  cursor: pointer;
}

.btn-jump:hover {
  background: #1f6feb;
  color: #ffffff;
}
</style>
