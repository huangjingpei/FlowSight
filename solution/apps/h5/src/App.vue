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
select, .flat-select {
  color-scheme: dark;
}

.theme-light select,
.theme-light .flat-select {
  color-scheme: light !important;
}

select option,
select optgroup,
.flat-select option {
  color-scheme: dark;
  background-color: #121824 !important;
  color: #F8FAFC !important;
}

.theme-light select option,
.theme-light select optgroup,
.theme-light .flat-select option {
  color-scheme: light !important;
  background-color: #FFFFFF !important;
  color: #0F172A !important;
}

.theme-light select option:checked,
.theme-light .flat-select option:checked {
  background-color: #E2E8F0 !important;
  color: #0284C7 !important;
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

/* ==================== 全局明亮浅色主题 (Light Theme) 级联注入 ==================== */
.theme-light .dashboard-viewport,
.theme-light .trends-viewport,
.theme-light .playback-viewport,
.theme-light .settings-viewport,
.theme-light .login-viewport {
  background-color: var(--fs-bg, #F8FAFC) !important;
}

.theme-light .flat-header {
  background: var(--fs-header-bg, #FFFFFF) !important;
  border-bottom: 1px solid var(--fs-border, #E2E8F0) !important;
}

.theme-light .flat-point-bar,
.theme-light .kpi-block,
.theme-light .flat-snapshot-strip,
.theme-light .realtime-wave-card,
.theme-light .kpi-cell,
.theme-light .chart-box,
.theme-light .flat-filter-box,
.theme-light .records-container,
.theme-light .flat-pagination,
.theme-light .profile-card,
.theme-light .menu-group,
.theme-light .login-card {
  background: var(--fs-card, #FFFFFF) !important;
  border-color: var(--fs-card-border, #CBD5E1) !important;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05) !important;
}

.theme-light .event-row {
  background: #F8FAFC !important;
  border-color: #E2E8F0 !important;
}

.theme-light .event-row:active {
  background: #F1F5F9 !important;
}

.theme-light .company-name,
.theme-light .header-title,
.theme-light .cell-title,
.theme-light .wave-title,
.theme-light .legend-title,
.theme-light .brand-title,
.theme-light .kpi-title,
.theme-light .snap-info-title,
.theme-light .text-white {
  color: #0F172A !important;
}

.theme-light .point-label,
.theme-light .cell-sub,
.theme-light .phone-str,
.theme-light .brand-sub,
.theme-light .form-label,
.theme-light .point-name,
.theme-light .w-leg-item,
.theme-light .leg-item,
.theme-light .page-info {
  color: #334155 !important;
}

.theme-light .kpi-footer,
.theme-light .cell-label,
.theme-light .cell-unit,
.theme-light .kpi-unit,
.theme-light .snap-info-sub,
.theme-light .meta-sub,
.theme-light .empty-text,
.theme-light .login-foot {
  color: #64748B !important;
}

.theme-light .text-cyan,
.theme-light .track-id,
.theme-light .quick-link {
  color: #0284C7 !important;
}

.theme-light .text-green {
  color: #16A34A !important;
}

.theme-light .text-orange {
  color: #EA580C !important;
}

.theme-light .text-purple {
  color: #7C3AED !important;
}

.theme-light .border-cyan { border-left-color: #0284C7 !important; }
.theme-light .border-green { border-left-color: #16A34A !important; }
.theme-light .border-orange { border-left-color: #EA580C !important; }
.theme-light .border-purple { border-left-color: #7C3AED !important; }

.theme-light .badge-cyan { background: rgba(2, 132, 199, 0.12) !important; color: #0284C7 !important; }
.theme-light .badge-green { background: rgba(22, 163, 74, 0.12) !important; color: #16A34A !important; }
.theme-light .badge-orange { background: rgba(234, 88, 12, 0.12) !important; color: #EA580C !important; }
.theme-light .badge-purple { background: rgba(124, 58, 237, 0.12) !important; color: #7C3AED !important; }

.theme-light .flat-select {
  background-color: #F1F5F9 !important;
  color: #0F172A !important;
  border-color: #CBD5E1 !important;
  color-scheme: light !important;
  appearance: none !important;
  -webkit-appearance: none !important;
  background-repeat: no-repeat !important;
  background-position: right 6px center !important;
  background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 24 24' fill='none' stroke='%230284C7' stroke-width='2.5' stroke-linecap='round' stroke-linejoin='round'%3E%3Cpath d='m6 9 6 6 6-6'/%3E%3C/svg%3E") !important;
}

.theme-light .flat-select option {
  background-color: #FFFFFF !important;
  color: #0F172A !important;
}

.theme-light .flat-select option:checked {
  background-color: #E2E8F0 !important;
  color: #0284C7 !important;
}

.theme-light .poll-badge {
  background: rgba(2, 132, 199, 0.1) !important;
  border-color: rgba(2, 132, 199, 0.4) !important;
  color: #0284C7 !important;
}

.theme-light .flat-status-bar {
  background: var(--fs-card, #FFFFFF) !important;
  border-color: var(--fs-card-border, #CBD5E1) !important;
  color: #0F172A !important;
}

.theme-light .flat-status-bar.status-ok {
  background: rgba(22, 163, 74, 0.08) !important;
  border-color: rgba(22, 163, 74, 0.3) !important;
  color: #15803D !important;
}

.theme-light .flat-segmented {
  background: #F1F5F9 !important;
  border-color: #CBD5E1 !important;
}

.theme-light .seg-btn {
  color: #475569 !important;
}

.theme-light .seg-btn.active {
  background: #0284C7 !important;
  color: #FFFFFF !important;
}

.theme-light .dir-pill,
.theme-light .date-pill,
.theme-light .btn-reset,
.theme-light .page-btn {
  background: #F1F5F9 !important;
  color: #475569 !important;
  border-color: #CBD5E1 !important;
}

.theme-light .dir-pill.active,
.theme-light .date-pill.active,
.theme-light .btn-search,
.theme-light .btn-play {
  background: #0284C7 !important;
  color: #FFFFFF !important;
  border-color: #0284C7 !important;
}

.theme-light .dir-pill.active.green {
  background: #16A34A !important;
  color: #FFFFFF !important;
  border-color: #16A34A !important;
}

.theme-light .dir-pill.active.orange {
  background: #EA580C !important;
  color: #FFFFFF !important;
  border-color: #EA580C !important;
}

.theme-light .menu-cell {
  border-bottom-color: #F1F5F9 !important;
}

.theme-light .menu-cell:active {
  background: #F8FAFC !important;
}

.theme-light .btn-logout {
  background: #FFFFFF !important;
  border-color: #CBD5E1 !important;
  color: #DC2626 !important;
}

.theme-light .dialog-overlay,
.theme-light .rate-modal-overlay {
  background: rgba(15, 23, 42, 0.45) !important;
}

.theme-light .dialog-card,
.theme-light .rate-modal-card {
  background: #FFFFFF !important;
  border-color: #CBD5E1 !important;
  color: #0F172A !important;
  box-shadow: 0 10px 30px rgba(15, 23, 42, 0.15) !important;
}

.theme-light .dialog-header,
.theme-light .rm-header {
  background: #F8FAFC !important;
  border-bottom-color: #E2E8F0 !important;
}

.theme-light .dh-title,
.theme-light .rm-title {
  color: #0F172A !important;
}

.theme-light .dh-close,
.theme-light .rm-close {
  color: #64748B !important;
}

.theme-light .dialog-body,
.theme-light .rm-body {
  background: #FFFFFF !important;
  color: #0F172A !important;
}

.theme-light .modal-desc,
.theme-light .rm-desc {
  color: #475569 !important;
}

.theme-light .modal-sub-meta,
.theme-light .rate-card {
  background: #F8FAFC !important;
  border-color: #E2E8F0 !important;
}

.theme-light .rate-card-active {
  background: rgba(2, 132, 199, 0.08) !important;
  border-color: #0284C7 !important;
}

.theme-light .rc-title {
  color: #0F172A !important;
}

.theme-light .rc-desc,
.theme-light .rc-sub {
  color: #64748B !important;
}

.theme-light .cell-value {
  color: #64748B !important;
}

.theme-light .cell-chevron {
  color: #94A3B8 !important;
}

.theme-light .mf-input,
.theme-light .flat-input {
  background: #F8FAFC !important;
  border-color: #CBD5E1 !important;
  color: #0F172A !important;
}

.theme-light .btn-modal-action.btn-gray-fill {
  background: #F1F5F9 !important;
  border-color: #CBD5E1 !important;
  color: #334155 !important;
}

.theme-light .node-meta-list {
  background: #F8FAFC !important;
  border-color: #E2E8F0 !important;
}

.theme-light .m-k {
  color: #64748B !important;
}

.theme-light .m-v {
  color: #0F172A !important;
}

.theme-light .btn-copy-pin,
.theme-light .btn-refresh-pin {
  background: #F8FAFC !important;
  border-color: #CBD5E1 !important;
  color: #0F172A !important;
}

.theme-light .pin-digit-box {
  background: #F8FAFC !important;
  border-color: #CBD5E1 !important;
  color: #0284C7 !important;
}

.theme-light .dual-pwd-hint {
  background: #F8FAFC !important;
  border-left-color: #0284C7 !important;
  color: #334155 !important;
}

.theme-light .font-preview-box {
  background: #F8FAFC !important;
  border-color: #0284C7 !important;
}

.theme-light .fp-content {
  color: #0F172A !important;
}

.theme-light .avatar-box {
  background: #F1F5F9 !important;
  border-color: #E2E8F0 !important;
}

.theme-light .group-title {
  color: #475569 !important;
}

.theme-light .gray-pill {
  background: #F1F5F9 !important;
  color: #475569 !important;
}

.theme-light .cyan-pill {
  background: rgba(2, 132, 199, 0.12) !important;
  color: #0284C7 !important;
}

.theme-light .blue-pill {
  background: rgba(37, 99, 235, 0.12) !important;
  color: #2563EB !important;
}

.theme-light .purple-pill {
  background: rgba(124, 58, 237, 0.12) !important;
  color: #7C3AED !important;
}

.theme-light .green-pill {
  background: rgba(22, 163, 74, 0.12) !important;
  color: #16A34A !important;
}

.theme-light .snapshot-modal-card {
  background: #FFFFFF !important;
  border-color: #CBD5E1 !important;
}

.theme-light .smc-header,
.theme-light .smc-footer {
  background: #F8FAFC !important;
  border-color: #E2E8F0 !important;
}

.theme-light .smc-title {
  color: #0F172A !important;
}
</style>