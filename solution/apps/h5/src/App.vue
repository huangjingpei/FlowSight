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
  background-color: #0d1117;
  font-family: -apple-system, BlinkMacSystemFont, 'Helvetica Neue', Helvetica, Segoe UI, Arial, Roboto, 'PingFang SC', 'miui', 'Hiragino Sans GB', 'Microsoft Yahei', sans-serif;
  color: #c9d1d9;
}

.global-tabbar {
  background: #161b22 !important;
  border-top: 1px solid #30363d !important;
}

.global-tabbar .van-tabbar-item {
  background: transparent !important;
}

.global-tabbar .van-tabbar-item--active {
  color: #58a6ff !important;
  font-weight: bold;
}
</style>
