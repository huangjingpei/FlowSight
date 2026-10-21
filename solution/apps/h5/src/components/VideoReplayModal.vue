<template>
  <van-popup
    v-model:show="visible"
    position="bottom"
    round
    :style="{ height: '65%' }"
    closeable
    @open="handleOpen"
    @closed="handleClosed"
  >
    <div class="video-modal-container">
      <div class="video-modal-header">
        <div class="title">🎬 通行事件切片复核</div>
        <div class="meta" v-if="eventInfo">
          <span>目标 #{{ eventInfo.trackId }}</span>
          <span :class="eventInfo.direction === 'ENTRY' ? 'tag-entry' : 'tag-exit'">
            {{ eventInfo.direction === 'ENTRY' ? '🟢 进入' : '🔴 离开' }}
          </span>
          <span>{{ eventInfo.gender }} / {{ eventInfo.ageGroup }}</span>
        </div>
      </div>

      <div class="player-wrapper">
        <video
          v-if="playUrl"
          ref="videoRef"
          :src="playUrl"
          controls
          autoplay
          playsinline
          webkit-playsinline
          class="html5-video"
        ></video>
        <div v-else class="empty-video">
          <van-loading type="spinner" size="24px">正在获取视频切片直链...</van-loading>
        </div>
      </div>

      <div class="footer-tip">
        <van-icon name="info-o" />
        <span>切片存储于云端 MinIO，播放时点位流切换已自动锁定保护</span>
      </div>
    </div>
  </van-popup>
</template>

<script setup lang="ts">
import { ref, watch } from 'vue';
import { usePointStore } from '../store/point';

const props = defineProps<{
  modelValue: boolean;
  playUrl: string;
  eventInfo: any;
}>();

const emit = defineEmits(['update:modelValue']);

const pointStore = usePointStore();
const visible = ref(props.modelValue);
const videoRef = ref<HTMLVideoElement | null>(null);

watch(() => props.modelValue, (val) => {
  visible.value = val;
});

const handleOpen = () => {
  // 核心业务硬约束: 播放视频时，锁定点位切换状态，禁止切换其他点位
  pointStore.setMediaPlaying(true);
};

const handleClosed = () => {
  if (videoRef.value) {
    videoRef.value.pause();
  }
  // 解锁点位切换
  pointStore.setMediaPlaying(false);
  emit('update:modelValue', false);
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

.player-wrapper {
  flex: 1;
  background-color: #000;
  border-radius: 8px;
  overflow: hidden;
  display: flex;
  justify-content: center;
  align-items: center;
}

.html5-video {
  width: 100%;
  height: 100%;
  object-fit: contain;
}

.empty-video {
  color: #fff;
  font-size: 13px;
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
