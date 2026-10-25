<template>
  <div class="h5-app-root">
    <div class="app-viewport-chassis">
      <!-- 单屏页面路由容器 (keep-alive 保持导航历史与图表平滑过渡，Login 页面除外) -->
      <div class="app-router-wrapper">
        <router-view v-slot="{ Component }">
          <keep-alive :exclude="['Login']">
            <component :is="Component" />
          </keep-alive>
        </router-view>
      </div>

      <!-- 扁平化底部 4 键导航栏 (完全嵌入视口内部，纯平质感无多余装饰) -->
      <nav v-if="showTabBar" class="flat-tabbar">
        <router-link to="/dashboard" class="tab-item" active-class="tab-active">
          <span class="tab-icon">⚡</span>
          <span class="tab-label">实时监控</span>
        </router-link>
        <router-link to="/trends" class="tab-item" active-class="tab-active">
          <span class="tab-icon">📈</span>
          <span class="tab-label">时序走势</span>
        </router-link>
        <router-link to="/playback" class="tab-item" active-class="tab-active">
          <span class="tab-icon">🎬</span>
          <span class="tab-label">证据切片</span>
        </router-link>
        <router-link to="/settings" class="tab-item" active-class="tab-active">
          <span class="tab-icon">⚙️</span>
          <span class="tab-label">系统设置</span>
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
/* ==================== 默认深邃科技暗黑主题 (Dark Theme) ==================== */
:root, :root.theme-dark {
  color-scheme: dark;
  --fs-body-bg: #000000;
  --fs-chassis-bg: #0A0D14;
  --fs-bg: #0A0D14;
  --fs-chassis-border: #1E2636;
  --fs-border: #1E2636;
  --fs-primary: #00D2FF;
  --fs-success: #00E676;
  --fs-header-bg: #0E131D;
  --fs-header-border: #1E2636;
  --fs-tabbar-bg: #0E131D;
  --fs-tabbar-border: #1E2636;
  --fs-card: #121722;
  --fs-card-secondary: #182030;
  --fs-card-border: #1E2636;
  --fs-card-border-subtle: #2A364F;
  --fs-select-bg: #161D2A;
  --fs-select-border: #233048;
  --fs-select-text: #00D2FF;
  --fs-select-option-bg: #121824;
  --fs-select-option-text: #FFFFFF;
  --fs-dialog-overlay: rgba(0, 0, 0, 0.75);
  --fs-dialog-card: #111827;
  --fs-dialog-header: #182030;
  --fs-dialog-border: #2A364F;
  --fs-dialog-inner-bg: #0B0E14;
  --fs-pill-bg: rgba(0, 210, 255, 0.12);
  --fs-pill-border: rgba(0, 210, 255, 0.35);

  --fs-cyan: #00D2FF;
  --fs-green: #00E676;
  --fs-orange: #FF9100;
  --fs-red: #FF3D71;
  --fs-purple: #9D4EDD;
  --fs-text-main: #FFFFFF;
  --fs-text-sub: #94A3B8;
  --fs-text-dim: #64748B;

  /* Vant 暗黑规范 */
  --van-background: #121722;
  --van-background-2: #182030;
  --van-text-color: #FFFFFF;
  --van-text-color-2: #94A3B8;
  --van-text-color-3: #64748B;
  --van-border-color: #1E2636;
  --van-primary-color: #00D2FF;
  --van-dialog-background: #111827;
  --van-dialog-message-color: #CBD5E1;
  --van-cell-background: #121722;
  --van-cell-text-color: #FFFFFF;
  --van-cell-value-color: #94A3B8;
}

/* ==================== 明亮浅色 / 适老化清晰主题 (Light Theme) ==================== */
:root.theme-light {
  color-scheme: light;
  --fs-body-bg: #E2E8F0;
  --fs-chassis-bg: #F8FAFC;
  --fs-bg: #F8FAFC;
  --fs-chassis-border: #CBD5E1;
  --fs-border: #E2E8F0;
  --fs-primary: #0284C7;
  --fs-success: #16A34A;
  --fs-header-bg: #FFFFFF;
  --fs-header-border: #E2E8F0;
  --fs-tabbar-bg: #FFFFFF;
  --fs-tabbar-border: #E2E8F0;
  --fs-card: #FFFFFF;
  --fs-card-secondary: #F1F5F9;
  --fs-card-border: #E2E8F0;
  --fs-card-border-subtle: #CBD5E1;
  --fs-select-bg: #FFFFFF;
  --fs-select-border: #CBD5E1;
  --fs-select-text: #0284C7;
  --fs-select-option-bg: #FFFFFF;
  --fs-select-option-text: #0F172A;
  --fs-dialog-overlay: rgba(15, 23, 42, 0.5);
  --fs-dialog-card: #FFFFFF;
  --fs-dialog-header: #F8FAFC;
  --fs-dialog-border: #CBD5E1;
  --fs-dialog-inner-bg: #F1F5F9;
  --fs-pill-bg: rgba(2, 132, 199, 0.1);
  --fs-pill-border: rgba(2, 132, 199, 0.4);

  --fs-cyan: #0284C7;
  --fs-green: #16A34A;
  --fs-orange: #EA580C;
  --fs-red: #DC2626;
  --fs-purple: #7C3AED;
  --fs-text-main: #0F172A;   /* 极高对比度纯正黑字 */
  --fs-text-sub: #334155;    /* 沉稳 slate-700 */
  --fs-text-dim: #64748B;

  /* Vant 浅色规范 */
  --van-background: #FFFFFF;
  --van-background-2: #F8FAFC;
  --van-text-color: #0F172A;
  --van-text-color-2: #334155;
  --van-text-color-3: #64748B;
  --van-border-color: #E2E8F0;
  --van-primary-color: #0284C7;
  --van-dialog-background: #FFFFFF;
  --van-dialog-message-color: #1E293B;
  --van-cell-background: #FFFFFF;
  --van-cell-text-color: #0F172A;
  --van-cell-value-color: #475569;
}

/* ==================== 适老化与无障碍字号缩放系统 ==================== */
:root, :root.font-scale-standard {
  --fs-font-scale: 1;
}

:root.font-scale-medium {
  --fs-font-scale: 1.1;
}

:root.font-scale-large {
  --fs-font-scale: 1.22;
}

.font-scale-medium .flat-header .header-title,
.font-scale-medium .company-name {
  font-size: 15px !important;
}
.font-scale-medium .kpi-num {
  font-size: 26px !important;
}
.font-scale-medium .kpi-title,
.font-scale-medium .cell-title,
.font-scale-medium .point-label,
.font-scale-medium .tab-label {
  font-size: 13px !important;
}

.font-scale-large .flat-header .header-title,
.font-scale-large .company-name {
  font-size: 17px !important;
  font-weight: 800 !important;
}
.font-scale-large .kpi-num {
  font-size: 29px !important;
  font-weight: 900 !important;
}
.font-scale-large .kpi-title,
.font-scale-large .cell-title,
.font-scale-large .point-label {
  font-size: 14.5px !important;
  font-weight: 800 !important;
}
.font-scale-large .tab-label {
  font-size: 12.5px !important;
  font-weight: 800 !important;
}
.font-scale-large .cell-sub,
.font-scale-large .kpi-footer,
.font-scale-large .status-msg {
  font-size: 12px !important;
  line-height: 140% !important;
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
  background-color: var(--fs-body-bg, #000000);
  /* 极致考究的中英与等宽数字无障碍字体栈：抗疲劳、高清晰度、数字等宽对齐 */
  font-family: -apple-system, BlinkMacSystemFont, 'Inter', 'SF Pro Text', 'PingFang SC',
               'Hiragino Sans GB', 'Microsoft YaHei', 'Source Han Sans CN', sans-serif;
  font-variant-numeric: tabular-nums;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  color: var(--fs-text-main, #FFFFFF);
  transition: background-color 0.25s ease, color 0.25s ease;
}

/* 全局下拉选择器双主题规范 */
select {
  color-scheme: dark;
}

.theme-light select {
  color-scheme: light;
}

select option,
select optgroup {
  color-scheme: dark;
  background-color: #121824 !important;
  color: #F8FAFC !important;
}

.theme-light select option,
.theme-light select optgroup {
  color-scheme: light;
  background-color: #FFFFFF !important;
  color: #0F172A !important;
}

.h5-app-root {
  height: 100vh;
  height: 100dvh;
  width: 100vw;
  background-color: var(--fs-body-bg, #000000);
  display: flex;
  justify-content: center;
  align-items: center;
  overflow: hidden;
  transition: background-color 0.25s ease;
}

/* 核心视口机架：严格 430px 单屏尺寸，100dvh，绝不滚动 */
.app-viewport-chassis {
  width: 100%;
  max-width: 430px;
  height: 100vh;
  height: 100dvh;
  background-color: var(--fs-chassis-bg, #0A0D14);
  display: flex;
  flex-direction: column;
  position: relative;
  overflow: hidden;
  border-left: 1px solid var(--fs-chassis-border, #1E2636);
  border-right: 1px solid var(--fs-chassis-border, #1E2636);
  box-sizing: border-box;
  transition: background-color 0.25s ease, border-color 0.25s ease;
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
  background: var(--fs-tabbar-bg, #0E131D);
  border-top: 1px solid var(--fs-tabbar-border, #1E2636);
  display: flex;
  align-items: center;
  justify-content: space-around;
  box-sizing: border-box;
  z-index: 50;
  transition: background-color 0.25s ease, border-color 0.25s ease;
}

.tab-item {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 2px;
  text-decoration: none;
  color: var(--fs-text-dim, #64748B);
  font-size: 11px;
  font-weight: 700;
  height: 100%;
  transition: color 0.15s ease;
}

.tab-icon {
  font-size: 15px;
}

.tab-active {
  color: var(--fs-cyan, #00D2FF) !important;
  font-weight: 800 !important;
}
</style>