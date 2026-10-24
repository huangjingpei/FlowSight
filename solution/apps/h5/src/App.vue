<template>
  <div class="h5-app-root">
    <router-view />
    <van-tabbar
      v-if="showTabBar"
      v-model="activeTab"
      route
      fixed
      placeholder
      active-color="#58a6ff"
      inactive-color="#8b949e"
      class="global-tabbar"
    >
      <van-tabbar-item to="/dashboard" icon="bar-chart-o">数据看板</van-tabbar-item>
      <van-tabbar-item to="/playback" icon="video-o">切片回放</van-tabbar-item>
    </van-tabbar>
  </div>
</template>

<script setup lang="ts">
import { ref, computed } from 'vue';
import { useRoute } from 'vue-router';

const route = useRoute();
const activeTab = ref(0);

const showTabBar = computed(() => {
  return route.path !== '/login';
});
</script>

<style>
body {
  margin: 0;
  padding: 0;
  background-color: #060911;
  font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Display', 'Segoe UI', Roboto, 'PingFang SC', 'Hiragino Sans GB', 'Microsoft Yahei', sans-serif;
  color: #c9d1d9;
}

.global-tabbar {
  max-width: 520px !important;
  left: 50% !important;
  transform: translateX(-50%) !important;
  background: rgba(10, 15, 29, 0.85) !important;
  backdrop-filter: blur(20px) !important;
  -webkit-backdrop-filter: blur(20px) !important;
  border-top: 1px solid rgba(255, 255, 255, 0.08) !important;
  box-shadow: 0 -4px 20px rgba(0, 0, 0, 0.4) !important;
}

.global-tabbar .van-tabbar-item {
  background: transparent !important;
}

.global-tabbar .van-tabbar-item--active {
  color: #22d3ee !important;
  font-weight: bold;
}
</style>
