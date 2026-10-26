<template>
  <van-popup
    v-model:show="visible"
    position="bottom"
    round
    :style="{ height: '68%' }"
    closeable
    @open="handleOpen"
    @closed="handleClosed"
  >
    <div class="video-modal-container">
      <div class="video-modal-header">
        <div class="title">🎬 通行事件切片复核</div>
        <div class="meta" v-if="currentEvent">
          <span class="point-badge" v-if="currentEvent.pointName">📍 {{ currentEvent.pointName }}</span>
          <span>目标 #{{ currentEvent.trackId }}</span>
          <span :class="currentEvent.direction === 'ENTRY' ? 'tag-entry' : 'tag-exit'">
            {{ currentEvent.direction === 'ENTRY' ? '🟢 进入' : '🔴 离开' }}
          </span>
          <span v-if="currentEvent.plateNumber" style="color: #58a6ff; font-weight: bold;">
            🚘 {{ currentEvent.plateNumber }} ({{ currentEvent.plateColor || '蓝牌' }}) · {{ currentEvent.vehicleColor || '' }}{{ currentEvent.vehicleType || '机动车' }}
          </span>
          <span v-else>{{ currentEvent.gender || '未知' }} / {{ currentEvent.ageGroup || '未知' }}</span>
        </div>
      </div>

      <div class="player-wrapper">
        <!-- 正常视频播放器 -->
        <video
          v-if="playUrl && !videoError"
          ref="videoRef"
          :src="playUrl"
          controls
          autoplay
          playsinline
          webkit-playsinline
          class="html5-video"
          @canplay="onCanPlay"
          @waiting="isBuffering = true"
          @playing="isBuffering = false"
          @error="onVideoError"
        ></video>

        <!-- 加载缓冲浮层 -->
        <div v-if="playUrl && !videoError && isBuffering" class="loading-overlay">
          <van-loading type="spinner" color="#1989fa" size="28px">视频缓冲中...</van-loading>
        </div>

        <!-- 视频解析/拉流失败状态 -->
        <div v-else-if="playUrl && videoError" class="state-container">
          <van-icon name="warning-o" size="46px" color="#ee0a24" />
          <div class="state-title">视频切片加载失败</div>
          <div class="state-desc">无法解析视频编码流或存储节点暂时无法响应</div>
          <button class="btn-retry" @click="handleRetry">
            <van-icon name="replay" /> 重新尝试播放
          </button>
        </div>

        <!-- 无切片直链状态 (未生成或异步上云队列中) -->
        <div v-else class="state-container">
          <van-icon name="video-o" size="46px" color="#666" />
          <div class="state-title">暂未关联云端切片视频</div>
          <div class="state-desc">切片正在边端排队异步上云中，或该通行事件未产生录像</div>
        </div>
      </div>

      <!-- 播放节奏控制条 -->
      <div v-if="playUrl && !videoError" class="speed-control-row">
        <span class="speed-label">🎥 播放节奏:</span>
        <div class="speed-pills">
          <span
            v-for="s in [0.5, 1.0, 1.5]"
            :key="s"
            :class="['speed-pill', playbackRate === s ? 'active' : '']"
            @click="setPlaybackRate(s)"
          >
            {{ s === 1.0 ? '1.0x 原速' : s === 0.5 ? '0.5x 慢速复核' : '1.5x 快速' }}
          </span>
        </div>
      </div>

      <div class="footer-tip">
        <van-icon name="info-o" />
        <span>切片存储于云端对象存储，播放时点位流切换已自动锁定保护</span>
      </div>
    </div>
  </van-popup>
</template>

<script setup lang="ts">
import { ref, watch, computed } from 'vue';
import { usePointStore } from '../store/point';

const props = withDefaults(defineProps<{
  modelValue?: boolean;
  show?: boolean;
  playUrl?: string;
  eventInfo?: any;
  eventData?: any;
}>(), {
  modelValue: false,
  show: false,
  playUrl: '',
  eventInfo: null,
  eventData: null
});

const emit = defineEmits(['update:modelValue', 'update:show']);

const pointStore = usePointStore();
const visible = ref(props.show || props.modelValue);
const videoRef = ref<HTMLVideoElement | null>(null);

const isBuffering = ref(true);
const videoError = ref(false);
const playbackRate = ref(1.0);

const setPlaybackRate = (rate: number) => {
  playbackRate.value = rate;
  if (videoRef.value) {
    videoRef.value.playbackRate = rate;
  }
};

const currentEvent = computed(() => props.eventData || props.eventInfo);

watch(() => props.show, (val) => {
  visible.value = val;
});

watch(() => props.modelValue, (val) => {
  visible.value = val;
});

watch(() => props.playUrl, () => {
  videoError.value = false;
  isBuffering.value = true;
  playbackRate.value = 1.0;
  if (videoRef.value) {
    videoRef.value.playbackRate = 1.0;
  }
});

const onCanPlay = () => {
  isBuffering.value = false;
  videoError.value = false;
};

const onVideoError = () => {
  isBuffering.value = false;
  videoError.value = true;
};

const handleRetry = () => {
  videoError.value = false;
  isBuffering.value = true;
  if (videoRef.value) {
    videoRef.value.load();
    videoRef.value.play().catch(() => {});
  }
};

const handleOpen = () => {
  // 核心业务硬约束: 播放视频时，锁定点位切换状态，禁止切换其他点位
  pointStore.setMediaPlaying(true);
  videoError.value = false;
  isBuffering.value = true;
};

const handleClosed = () => {
  if (videoRef.value) {
    videoRef.value.pause();
  }
  pointStore.setMediaPlaying(false);
  videoError.value = false;
  isBuffering.value = false;
  emit('update:modelValue', false);
  emit('update:show', false);
};
</script>

<style scoped>
.video-modal-container {
  display: flex;
  flex-direction: column;
  height: 100%;
  padding: 16px;
  box-sizing: border-box;
  background: #0C1220;
  color: #FFFFFF;
  border-top: 2px solid #00F0FF;
}

.video-modal-header {
  margin-bottom: 12px;
}

.video-modal-header .title {
  font-size: 16px;
  font-weight: 800;
  color: #FFFFFF;
  margin-bottom: 6px;
  letter-spacing: 0.5px;
}

.video-modal-header .meta {
  font-size: 12px;
  color: #94A3B8;
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  align-items: center;
}

.tag-entry {
  color: #00FF88;
  font-weight: 700;
  background: rgba(0, 255, 136, 0.15);
  border: 1px solid rgba(0, 255, 136, 0.35);
  padding: 2px 6px;
  border-radius: 4px;
}

.tag-exit {
  color: #FF9E00;
  font-weight: 700;
  background: rgba(255, 158, 0, 0.15);
  border: 1px solid rgba(255, 158, 0, 0.35);
  padding: 2px 6px;
  border-radius: 4px;
}

.point-badge {
  background: rgba(0, 240, 255, 0.15);
  color: #00F0FF;
  border: 1px solid rgba(0, 240, 255, 0.35);
  padding: 2px 6px;
  border-radius: 4px;
  font-size: 11px;
  font-weight: 700;
}

.player-wrapper {
  flex: 1;
  background-color: #050811;
  border: 1px solid rgba(0, 240, 255, 0.3);
  border-radius: 10px;
  overflow: hidden;
  position: relative;
  display: flex;
  justify-content: center;
  align-items: center;
  box-shadow: inset 0 0 20px rgba(0, 0, 0, 0.8), 0 4px 16px rgba(0, 240, 255, 0.15);
}

.html5-video {
  width: 100%;
  height: 100%;
  object-fit: contain;
}

.loading-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(5, 8, 17, 0.85);
  display: flex;
  justify-content: center;
  align-items: center;
  z-index: 10;
}

.state-container {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 24px;
}

.state-title {
  color: #FFFFFF;
  font-size: 15px;
  font-weight: 700;
  margin-top: 12px;
}

.state-desc {
  color: #94A3B8;
  font-size: 12px;
  line-height: 1.5;
  margin-top: 6px;
  max-width: 260px;
}

.btn-retry {
  margin-top: 14px;
  background: linear-gradient(135deg, #00F0FF 0%, #0099FF 100%);
  color: #050811;
  border: none;
  padding: 6px 18px;
  border-radius: 18px;
  font-size: 13px;
  font-weight: 800;
  display: flex;
  align-items: center;
  gap: 4px;
  cursor: pointer;
  box-shadow: 0 4px 14px rgba(0, 240, 255, 0.4);
}

.btn-retry:active {
  transform: scale(0.97);
}

.speed-control-row {
  margin-top: 10px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 6px 12px;
  background: #111624;
  border-radius: 6px;
  border: 1px solid #1F293D;
}

.speed-label {
  font-size: 11px;
  color: #94A3B8;
  font-weight: 600;
}

.speed-pills {
  display: flex;
  gap: 6px;
}

.speed-pill {
  font-size: 11px;
  padding: 2px 8px;
  border-radius: 4px;
  background: #182030;
  color: #94A3B8;
  border: 1px solid #2A364F;
  cursor: pointer;
}

.speed-pill.active {
  background: #00D2FF;
  color: #0A0D14;
  font-weight: 800;
  border-color: #00D2FF;
}

.footer-tip {
  margin-top: 10px;
  font-size: 11px;
  color: #64748B;
  display: flex;
  align-items: center;
  gap: 4px;
}
</style>
