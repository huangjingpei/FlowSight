<template>
  <div class="playback-viewport">
    <!-- 1. 顶部嵌入式标题栏 -->
    <header class="flat-header">
      <div class="header-left">
        <span class="header-dot"></span>
        <span class="header-title">证据切片回放</span>
      </div>
      <div class="header-point-badge" v-if="selectedPointName">
        <span>{{ selectedPointName }}</span>
      </div>
    </header>

    <!-- 2. 主体：单屏纯平证据中心 (无页面级上下滚动) -->
    <main class="flat-main">
      <!-- 紧凑过滤控制栏 -->
      <div class="flat-filter-box">
        <div class="filter-line-1">
          <div class="point-select-wrap">
            <select v-model="filterPointId" class="flat-select" @change="onSearch">
              <option :value="0">全部通道点位</option>
              <option v-for="p in pointOptions" :key="p.id" :value="p.id">
                [{{ p.id }}] {{ p.pointName }}
              </option>
            </select>
          </div>
          <div class="dir-group">
            <span
              :class="['dir-pill', filterDirection === 'ALL' ? 'active' : '']"
              @click="setDirection('ALL')"
            >全</span>
            <span
              :class="['dir-pill', filterDirection === 'ENTRY' ? 'active green' : '']"
              @click="setDirection('ENTRY')"
            >进</span>
            <span
              :class="['dir-pill', filterDirection === 'EXIT' ? 'active orange' : '']"
              @click="setDirection('EXIT')"
            >出</span>
          </div>
        </div>

        <div class="filter-line-2">
          <div class="date-group">
            <span
              v-for="d in dateShortcuts"
              :key="d.key"
              :class="['date-pill', activeDateShortcut === d.key ? 'active' : '']"
              @click="setDateShortcut(d.key)"
            >
              {{ d.label }}
            </span>
          </div>
          <div class="action-group">
            <button class="btn-reset" @click="onReset">重置</button>
            <button class="btn-search" @click="onSearch">查询</button>
          </div>
        </div>
      </div>

      <!-- 切片流水容器 (自适应占满中间全部剩余高度，仅内部滚动) -->
      <div class="records-container">
        <!-- 加载状态 -->
        <div v-if="loading" class="state-center">
          <van-loading type="spinner" color="#00D2FF" size="22px">
            检索中...
          </van-loading>
        </div>

        <!-- 空状态 -->
        <div v-else-if="eventList.length === 0" class="state-center empty-text">
          未检索到符合条件的通行证据切片
        </div>

        <!-- 扁平卡片列表 (仅列表内部滚动) -->
        <div v-else class="event-list-scroll">
          <div
            v-for="ev in eventList"
            :key="ev.id"
            class="event-row"
            @click="handlePlayClip(ev)"
          >
            <div class="event-meta">
              <div class="meta-top">
                <span class="track-id">#{{ ev.trackId }}</span>
                <span :class="['dir-badge', ev.direction === 'ENTRY' ? 'badge-in' : 'badge-out']">
                  {{ ev.direction === 'ENTRY' ? '进入' : '离开' }}
                </span>
                <span class="point-name">{{ ev.pointName || ('通道#' + ev.pointId) }}</span>
              </div>
              <div class="meta-sub">
                <span class="time-str">{{ ev.datetime }}</span>
                <span class="attr-str">{{ ev.gender || '未知' }} · {{ ev.ageGroup || '未知' }}</span>
              </div>
            </div>

            <!-- 回放按钮 -->
            <button class="btn-play" @click.stop="handlePlayClip(ev)">
              <span>▶ 播放</span>
            </button>
          </div>
        </div>
      </div>

      <!-- 紧凑底部单行分页 -->
      <div class="flat-pagination" v-if="totalRecords > 0">
        <button
          class="page-btn"
          :disabled="currentPage <= 1"
          @click="goToPage(currentPage - 1)"
        >
          上一页
        </button>

        <span class="page-info">{{ currentPage }} / {{ totalPages }} (共{{ totalRecords }}条)</span>

        <button
          class="page-btn"
          :disabled="currentPage >= totalPages"
          @click="goToPage(currentPage + 1)"
        >
          下一页
        </button>
      </div>
    </main>

    <!-- 视频切片标准秒开回放弹窗 -->
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
import VideoReplayModal from '../components/VideoReplayModal.vue';

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

// 数据列表
const loading = ref<boolean>(false);
const eventList = ref<any[]>([]);
const pointOptions = ref<any[]>([]);

// 视频回放弹窗
const showVideoModal = ref<boolean>(false);
const currentPlayUrl = ref<string>('');
const currentEvent = ref<any>(null);

const dateShortcuts = [
  { key: 'today', label: '今天' },
  { key: 'yesterday', label: '昨天' },
  { key: 'week', label: '近7天' },
  { key: 'all', label: '全部' }
];

const selectedPointName = computed(() => {
  if (filterPointId.value === 0) return '全部通道';
  const found = pointOptions.value.find(p => p.id === filterPointId.value);
  return found ? found.pointName : '';
});

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
    const y = new Date(now.getTime() - 24 * 3600 * 1000);
    const s = formatYMD(y);
    startDate.value = s;
    endDate.value = s;
  } else if (key === 'week') {
    const w = new Date(now.getTime() - 7 * 24 * 3600 * 1000);
    startDate.value = formatYMD(w);
    endDate.value = formatYMD(now);
  } else if (key === 'all') {
    startDate.value = '';
    endDate.value = '';
  }
};

const setDirection = (dir: string) => {
  filterDirection.value = dir;
  currentPage.value = 1;
  fetchEvents();
};

const setDateShortcut = (key: string) => {
  activeDateShortcut.value = key;
  calculateDateRange(key);
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
  fetchEvents();
};

const goToPage = (page: number) => {
  if (page < 1 || page > totalPages.value || page === currentPage.value) return;
  currentPage.value = page;
  fetchEvents();
};

const fetchEvents = async () => {
  loading.value = true;
  try {
    const defaultPid = (pointOptions.value.length > 0) ? pointOptions.value[0].id : 0;
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
    } else {
      eventList.value = [];
      totalRecords.value = 0;
      totalPages.value = 1;
    }
  } catch (err: any) {
    showToast(err.message || '加载切片失败');
  } finally {
    loading.value = false;
  }
};

const fetchPoints = async () => {
  try {
    const res: any = await h5Api.getMyPoints();
    if (res && Array.isArray(res) && res.length > 0) {
      pointOptions.value = res;
      if (!filterPointId.value) {
        filterPointId.value = res[0].id;
      }
    }
  } catch (e) {}
};

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
/* 整个视图严格占满父容器，无页面级滚动 */
.playback-viewport {
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

.header-point-badge {
  background: #182030;
  color: #00D2FF;
  border: 1px solid #2A364F;
  padding: 2px 8px;
  border-radius: 4px;
  font-size: 11px;
  font-weight: 600;
}

/* 主体单屏 */
.cockpit-body, .flat-main {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  padding: 8px 10px;
  box-sizing: border-box;
  overflow: hidden;
  gap: 8px;
}

/* 紧凑过滤栏 */
.flat-filter-box {
  flex-shrink: 0;
  height: 68px;
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 6px 8px;
  box-sizing: border-box;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
}

.filter-line-1, .filter-line-2 {
  display: flex;
  align-items: center;
  gap: 6px;
}

.point-select-wrap {
  flex: 1;
}

.flat-select {
  width: 100%;
  height: 24px;
  background: #182030;
  color: #FFFFFF;
  border: 1px solid #2A364F;
  border-radius: 4px;
  padding: 0 6px;
  font-size: 11px;
  font-weight: 600;
  outline: none;
  color-scheme: dark;
}

.flat-select option {
  background-color: #121824 !important;
  color: #FFFFFF !important;
  font-size: 12px;
  padding: 6px 10px;
}

.dir-group {
  display: flex;
  gap: 3px;
}

.dir-pill {
  width: 28px;
  height: 24px;
  line-height: 24px;
  text-align: center;
  background: #182030;
  color: #94A3B8;
  border: 1px solid #2A364F;
  border-radius: 4px;
  font-size: 11px;
  font-weight: 700;
  cursor: pointer;
}
.dir-pill.active { background: #00D2FF; color: #0A0D14; border-color: #00D2FF; }
.dir-pill.active.green { background: #00E676; color: #0A0D14; border-color: #00E676; }
.dir-pill.active.orange { background: #FF9100; color: #0A0D14; border-color: #FF9100; }

.date-group {
  display: flex;
  gap: 3px;
  flex: 1;
}

.date-pill {
  flex: 1;
  height: 22px;
  line-height: 22px;
  text-align: center;
  background: #182030;
  color: #94A3B8;
  border: 1px solid #2A364F;
  border-radius: 3px;
  font-size: 10px;
  font-weight: 600;
  cursor: pointer;
}
.date-pill.active {
  background: #00D2FF;
  color: #0A0D14;
  border-color: #00D2FF;
  font-weight: 800;
}

.action-group {
  display: flex;
  gap: 4px;
}

.btn-reset, .btn-search {
  height: 22px;
  padding: 0 8px;
  border-radius: 3px;
  font-size: 10px;
  font-weight: 700;
  cursor: pointer;
  border: none;
}
.btn-reset {
  background: #182030;
  color: #94A3B8;
  border: 1px solid #2A364F;
}
.btn-search {
  background: #00D2FF;
  color: #0A0D14;
}

/* 切片容器 (填满中间剩余高度) */
.records-container {
  flex: 1;
  min-height: 0;
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 6px;
  box-sizing: border-box;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.state-center {
  flex: 1;
  display: flex;
  align-items: center;
  justify-content: center;
}
.empty-text {
  color: #64748B;
  font-size: 12px;
}

/* 列表滚动容器 */
.event-list-scroll {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.event-row {
  background: #182030;
  border: 1px solid #222C3E;
  border-radius: 6px;
  padding: 8px 10px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  cursor: pointer;
}
.event-row:active {
  background: #1E293F;
}

.event-meta {
  display: flex;
  flex-direction: column;
  gap: 3px;
  flex: 1;
}

.meta-top {
  display: flex;
  align-items: center;
  gap: 6px;
}

.track-id {
  font-size: 11px;
  font-weight: 800;
  color: #00D2FF;
}

.dir-badge {
  font-size: 10px;
  font-weight: 700;
  padding: 1px 5px;
  border-radius: 3px;
}
.badge-in { background: rgba(0, 230, 118, 0.15); color: #00E676; }
.badge-out { background: rgba(255, 145, 0, 0.15); color: #FF9100; }

.point-name {
  font-size: 10px;
  color: #94A3B8;
}

.meta-sub {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 10px;
  color: #64748B;
}

/* 播放按钮 */
.btn-play {
  flex-shrink: 0;
  margin-left: 8px;
  background: #00D2FF;
  color: #0A0D14;
  border: none;
  border-radius: 4px;
  padding: 4px 10px;
  font-size: 11px;
  font-weight: 800;
  cursor: pointer;
}
.btn-play:active {
  opacity: 0.85;
}

/* 底部单行分页 */
.flat-pagination {
  flex-shrink: 0;
  height: 30px;
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0 8px;
  box-sizing: border-box;
}

.page-btn {
  height: 22px;
  padding: 0 8px;
  background: #182030;
  color: #00D2FF;
  border: 1px solid #2A364F;
  border-radius: 3px;
  font-size: 10px;
  font-weight: 700;
  cursor: pointer;
}
.page-btn:disabled {
  opacity: 0.3;
  cursor: not-allowed;
  color: #64748B;
}

.page-info {
  font-size: 10px;
  color: #94A3B8;
  font-weight: 600;
}
</style>