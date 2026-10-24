<template>
  <div class="h5-app-root">
    <div class="app-viewport-chassis">
      <!-- 单屏页面路由容器 (严禁页面级滚动) -->
      <div class="app-router-wrapper">
        <router-view />
      </div>

      <!-- 扁平化底部 4 键导航栏 (完全嵌入视口内部，纯平质感无多余装饰) -->
      <nav v-if="showTabBar" class="flat-tabbar">
        <router-link to="/dashboard" class="tab-item" active-class="tab-active">
          <span class="tab-icon">⚡</span>
          <span class="tab-label">实时座舱</span>
        </router-link>
        <router-link to="/trends" class="tab-item" active-class="tab-active">
          <span class="tab-icon">📈</span>
          <span class="tab-label">时序走势</span>
        </router-link>
        <router-link to="/playback" class="tab-item" active-class="tab-active">
          <span class="tab-icon">🎬</span>
          <span class="tab-label">证据切片</span>
        </router-link>
        <router-link to="/insights" class="tab-item" active-class="tab-active">
          <span class="tab-icon">👥</span>
          <span class="tab-label">客群遥测</span>
        </router-link>
      </nav>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { useRoute } from 'vue-router';

const route = useRoute();

const showTabBar = computed(() => {
  return route.path !== '/login';
});
</script>

<style>
/* 扁平化极简主题规范 (去除光晕环套，注重对比与数据焦点) */
:root {
  --fs-bg: #0A0D14;
  --fs-card: #121722;
  --fs-card-border: #1E2636;
  --fs-cyan: #00D2FF;
  --fs-green: #00E676;
  --fs-orange: #FF9100;
  --fs-red: #FF3D71;
  --fs-purple: #9D4EDD;
  --fs-text-main: #FFFFFF;
  --fs-text-sub: #94A3B8;
  --fs-text-dim: #64748B;

  /* Vant 组件扁平化暗黑适配 */
  --van-background: #121722;
  --van-background-2: #182030;
  --van-text-color: #FFFFFF;
  --van-text-color-2: #94A3B8;
  --van-text-color-3: #64748B;
  --van-border-color: #1E2636;
  --van-primary-color: #00D2FF;
  --van-success-color: #00E676;
  --van-danger-color: #FF3D71;
  --van-warning-color: #FF9100;
  --van-dialog-background: #121722;
  --van-dialog-header-font-weight: 700;
  --van-dialog-message-color: #CBD5E1;
  --van-cell-background: #121722;
  --van-cell-text-color: #FFFFFF;
  --van-cell-value-color: #94A3B8;
  --van-cell-group-inset-padding: 0;
  --van-field-label-color: #94A3B8;
  --van-field-input-text-color: #FFFFFF;
}

* {
  box-sizing: border-box;
  -webkit-tap-highlight-color: transparent;
}

html, body {
  margin: 0;
  padding: 0;
  height: 100%;
  width: 100%;
  overflow: hidden;
  background-color: #000000;
  font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Display', 'PingFang SC', 'Hiragino Sans GB', 'Microsoft Yahei', sans-serif;
  color: #FFFFFF;
  -webkit-font-smoothing: antialiased;
}

.h5-app-root {
  height: 100vh;
  height: 100dvh;
  width: 100vw;
  background-color: #000000;
  display: flex;
  justify-content: center;
  align-items: center;
  overflow: hidden;
}

/* 核心视口机架：严格 430px 单屏尺寸，100dvh，绝不滚动 */
.app-viewport-chassis {
  width: 100%;
  max-width: 430px;
  height: 100vh;
  height: 100dvh;
  background-color: #0A0D14;
  display: flex;
  flex-direction: column;
  position: relative;
  overflow: hidden;
  border-left: 1px solid #1E2636;
  border-right: 1px solid #1E2636;
  box-sizing: border-box;
}

.app-router-wrapper {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

/* 扁平化底部 4 键导航栏 (高度固定 50px，完全嵌入视口内部) */
.flat-tabbar {
  flex-shrink: 0;
  height: 50px;
  background: #0E131D;
  border-top: 1px solid #1E2636;
  display: flex;
  align-items: center;
  justify-content: space-around;
  box-sizing: border-box;
  z-index: 50;
}

.tab-item {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 2px;
  text-decoration: none;
  color: #64748B;
  font-size: 11px;
  font-weight: 700;
  height: 100%;
  transition: color 0.15s ease;
}

.tab-icon {
  font-size: 15px;
}

.tab-active {
  color: #00D2FF !important;
  font-weight: 800 !important;
}
</style>