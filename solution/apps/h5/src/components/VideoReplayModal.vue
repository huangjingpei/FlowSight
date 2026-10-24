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
          <span>{{ currentEvent.gender || '未知' }} / {{ currentEvent.ageGroup || '未知' }}</span>
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
}

.video-modal-header {
  margin-bottom: 12px;
}

.video-modal-header .title {
  font-size: 16px;
  font-weight: bold;
  color: #1a1a1a;
  margin-bottom: 6px;
}

.video-modal-header .meta {
  font-size: 13px;
  color: #666;
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
  align-items: center;
}

.tag-entry {
  color: #07c160;
  font-weight: 600;
}

.tag-exit {
  color: #ee0a24;
  font-weight: 600;
}

.point-badge {
  background: #f0f4ff;
  color: #1989fa;
  padding: 2px 6px;
  border-radius: 4px;
  font-size: 12px;
  font-weight: 500;
}

.player-wrapper {
  flex: 1;
  background-color: #0d1117;
  border-radius: 10px;
  overflow: hidden;
  position: relative;
  display: flex;
  justify-content: center;
  align-items: center;
  box-shadow: inset 0 0 16px rgba(0, 0, 0, 0.4);
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
  background: rgba(0, 0, 0, 0.65);
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
  color: #e6edf3;
  font-size: 15px;
  font-weight: 600;
  margin-top: 12px;
}

.state-desc {
  color: #8b949e;
  font-size: 12px;
  line-height: 1.5;
  margin-top: 6px;
  max-width: 260px;
}

.btn-retry {
  margin-top: 14px;
  background: #1989fa;
  color: #fff;
  border: none;
  padding: 6px 16px;
  border-radius: 18px;
  font-size: 13px;
  display: flex;
  align-items: center;
  gap: 4px;
  cursor: pointer;
  transition: opacity 0.2s;
}

.btn-retry:active {
  opacity: 0.8;
}

.footer-tip {
  margin-top: 10px;
  font-size: 12px;
  color: #999;
  display: flex;
  align-items: center;
  gap: 4px;
}
</style>
