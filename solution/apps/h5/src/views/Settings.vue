<template>
  <div class="settings-viewport">
    <!-- 顶部标题栏 -->
    <header class="flat-header">
      <div class="header-left">
        <span class="header-dot"></span>
        <span class="header-title">系统设置</span>
      </div>
    </header>

    <!-- 主体内容区 (纯平菜单列表) -->
    <main class="settings-scroll-body">
      <!-- 商户租户卡片 -->
      <div class="profile-card">
        <div class="profile-top">
          <div class="avatar-box">🏢</div>
          <div class="profile-meta">
            <div class="company-name">{{ userProfile.companyName || userProfile.nickname || '企业商户' }}</div>
            <div class="phone-str">账号：{{ userProfile.phone || '--' }}</div>
          </div>
          <div class="auth-pill">已认证</div>
        </div>
      </div>

      <!-- 分组 1: 设备与安全 -->
      <div class="group-title">设备与安全</div>
      <div class="menu-group">
        <div class="menu-cell" @click="openModal('pairing')">
          <div class="cell-left">
            <span class="cell-icon">⚡</span>
            <span class="cell-title">设备配对码</span>
          </div>
          <div class="cell-right">
            <span class="cell-value">生成</span>
            <span class="cell-chevron">›</span>
          </div>
        </div>

        <div class="menu-cell" @click="openModal('devicePwd')">
          <div class="cell-left">
            <span class="cell-icon">🛡️</span>
            <span class="cell-title">设备密码</span>
          </div>
          <div class="cell-right">
            <span class="cell-value">修改</span>
            <span class="cell-chevron">›</span>
          </div>
        </div>

        <div class="menu-cell" @click="openModal('password')">
          <div class="cell-left">
            <span class="cell-icon">🔐</span>
            <span class="cell-title">登录密码</span>
          </div>
          <div class="cell-right">
            <span class="cell-value">修改</span>
            <span class="cell-chevron">›</span>
          </div>
        </div>
      </div>

      <!-- 分组 2: 监控与数据 -->
      <div class="group-title">监控与数据</div>
      <div class="menu-group">
        <div class="menu-cell" @click="openModal('demographics')">
          <div class="cell-left">
            <span class="cell-icon">👥</span>
            <span class="cell-title">客群画像</span>
          </div>
          <div class="cell-right">
            <span class="cell-value">查看</span>
            <span class="cell-chevron">›</span>
          </div>
        </div>

        <div class="menu-cell" @click="openModal('refreshRate')">
          <div class="cell-left">
            <span class="cell-icon">⏱️</span>
            <span class="cell-title">数据刷新</span>
          </div>
          <div class="cell-right">
            <span class="cell-value">{{ activeRate }}秒</span>
            <span class="cell-chevron">›</span>
          </div>
        </div>

        <div class="menu-cell" @click="openModal('deviceInfo')">
          <div class="cell-left">
            <span class="cell-icon">📡</span>
            <span class="cell-title">通道设备</span>
          </div>
          <div class="cell-right">
            <span class="status-dot" :class="summary.streamStatus === 'ONLINE' ? 'online' : 'offline'">
              {{ summary.streamStatus === 'ONLINE' ? '● 在线' : '○ 离线' }}
            </span>
            <span class="cell-chevron">›</span>
          </div>
        </div>

        <div class="menu-cell" @click="openModal('defaultPoint')">
          <div class="cell-left">
            <span class="cell-icon">📌</span>
            <span class="cell-title">默认业务座舱</span>
          </div>
          <div class="cell-right">
            <span class="cell-value">{{ currentDefaultPointLabel }}</span>
            <span class="cell-chevron">›</span>
          </div>
        </div>

        <div class="menu-cell" @click="openModal('digitalScreen')">
          <div class="cell-left">
            <span class="cell-icon">🖥️</span>
            <span class="cell-title">我的专属数字大屏</span>
          </div>
          <div class="cell-right">
            <span class="cell-value" style="color: var(--fs-primary, #00D2FF);">投屏直达</span>
            <span class="cell-chevron">›</span>
          </div>
        </div>
      </div>

      <!-- 分组 3: 界面显示 -->
      <div class="group-title">界面显示</div>
      <div class="menu-group">
        <div class="menu-cell" @click="openModal('themeMode')">
          <div class="cell-left">
            <span class="cell-icon">🎨</span>
            <span class="cell-title">显示主题</span>
          </div>
          <div class="cell-right">
            <span class="cell-value">{{ currentThemeLabel }}</span>
            <span class="cell-chevron">›</span>
          </div>
        </div>

        <div class="menu-cell" @click="openModal('fontSize')">
          <div class="cell-left">
            <span class="cell-icon">🔠</span>
            <span class="cell-title">字号大小</span>
          </div>
          <div class="cell-right">
            <span class="cell-value">{{ fontScaleLabel }}</span>
            <span class="cell-chevron">›</span>
          </div>
        </div>
      </div>

      <!-- 退出登录按钮 -->
      <div class="logout-btn-box">
        <button class="btn-logout" @click="handleLogout">
          <span>退出登录</span>
        </button>
      </div>
    </main>

    <!-- ==================== 统一弹窗对话框区域 (Modal Dialogs) ==================== -->

    <!-- 弹窗 1: 设备配对码 -->
    <div v-if="activeModal === 'pairing'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">设备配对码</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="modal-sub-meta">
            <span class="msm-k">当前通道</span>
            <span class="msm-v">{{ pointCredential.pointName || summary.pointName || '当前通道' }}</span>
          </div>

          <div v-if="!pairingData" class="pairing-init-box">
            <button class="btn-modal-action btn-cyan-fill" :disabled="pairingLoading" @click="handleGeneratePairingCode">
              {{ pairingLoading ? '正在生成...' : '生成 6 位配对码' }}
            </button>
          </div>

          <div v-else class="pairing-active-box">
            <div class="pairing-countdown">
              <span>有效时间剩余 <b class="text-orange">{{ pairingRemainingSec }}秒</b></span>
            </div>
            <div class="pin-digits-row">
              <span v-for="(digit, idx) in pairingDigits" :key="idx" class="pin-digit-box">
                {{ digit }}
              </span>
            </div>
            <div class="modal-action-row">
              <button class="btn-copy-pin" @click="handleCopyPin">复制</button>
              <button class="btn-refresh-pin" :disabled="pairingLoading" @click="handleGeneratePairingCode">重新生成</button>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 2: 设备密码 -->
    <div v-if="activeModal === 'devicePwd'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">修改设备密码</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="modal-sub-meta">
            <span class="msm-k">通道编码</span>
            <span class="msm-v cyan-code">{{ pointCredential.pointCode || 'POINT_VK_EAST_01' }}</span>
          </div>

          <div class="modal-form">
            <div class="modal-form-item">
              <label class="mf-label">新设备密码</label>
              <input
                v-model="newDevicePassword"
                type="password"
                placeholder="请输入至少6位密码"
                class="mf-input"
              />
            </div>
            <button class="btn-modal-action btn-cyan-fill" :disabled="devPwdLoading" @click="handleResetDevicePassword">
              {{ devPwdLoading ? '正在保存...' : '保存密码' }}
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 3: 登录密码 -->
    <div v-if="activeModal === 'password'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">修改登录密码</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="modal-form">
            <div class="modal-form-item">
              <label class="mf-label">原密码</label>
              <input
                v-model="pwdForm.oldPassword"
                type="password"
                placeholder="请输入当前原密码"
                class="mf-input"
              />
            </div>
            <div class="modal-form-item">
              <label class="mf-label">新密码</label>
              <input
                v-model="pwdForm.newPassword"
                type="password"
                placeholder="请输入至少6位新密码"
                class="mf-input"
              />
            </div>
            <div class="modal-form-item">
              <label class="mf-label">确认密码</label>
              <input
                v-model="pwdForm.confirmPassword"
                type="password"
                placeholder="请再次输入新密码"
                class="mf-input"
              />
            </div>
            <button class="btn-modal-action btn-blue-fill" :disabled="pwdLoading" @click="handleChangePassword">
              {{ pwdLoading ? '正在保存...' : '确认修改' }}
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 4: 客群画像 -->
    <div v-if="activeModal === 'demographics'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">客群画像</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="demo-modal-content">
            <div class="gender-text-row" v-if="summary.demographics">
              <span class="text-cyan">男 {{ (summary.demographics.maleRatio * 100).toFixed(0) }}% ({{ summary.demographics.maleCount }}人)</span>
              <span class="text-pink">女 {{ (summary.demographics.femaleRatio * 100).toFixed(0) }}% ({{ summary.demographics.femaleCount }}人)</span>
            </div>

            <div class="flat-progress-bar" v-if="summary.demographics">
              <div class="bar-cyan" :style="{ width: (summary.demographics.maleRatio * 100) + '%' }"></div>
              <div class="bar-pink" :style="{ width: (summary.demographics.femaleRatio * 100) + '%' }"></div>
            </div>

            <div class="age-info-row" v-if="summary.demographics">
              <span class="info-lbl">主力年龄段:</span>
              <span class="age-pill">{{ summary.demographics.topAgeGroup || '青年(18-35)' }}</span>
            </div>

            <div class="demo-count-row">
              <span>今日累计样本：<b>{{ summary.todayTotal || 0 }}</b> 人次</span>
            </div>

            <button class="btn-modal-action btn-gray-fill" style="margin-top: 14px;" @click="closeModal">
              完成
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 5: 数据刷新频率 -->
    <div v-if="activeModal === 'refreshRate'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">数据刷新频率</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="rate-select-list">
            <div
              v-for="r in refreshRates"
              :key="r.sec"
              :class="['rate-card', activeRate === r.sec ? 'rate-card-active' : '']"
              @click="handleSelectRate(r.sec)"
            >
              <div class="rc-left">
                <span class="rc-radio">{{ activeRate === r.sec ? '●' : '○' }}</span>
                <span class="rc-title">{{ r.label }}</span>
              </div>
              <span class="rc-sub">{{ r.desc }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 6: 通道设备详情 -->
    <div v-if="activeModal === 'deviceInfo'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">通道设备</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="node-meta-list">
            <div class="meta-row">
              <span class="m-k">通道名称</span>
              <span class="m-v">{{ summary.pointName || '--' }}</span>
            </div>
            <div class="meta-row">
              <span class="m-k">通道编码</span>
              <span class="m-v cyan-code">{{ pointCredential.pointCode || '--' }}</span>
            </div>
            <div class="meta-row">
              <span class="m-k">连接状态</span>
              <span class="m-v" :class="summary.streamStatus === 'ONLINE' ? 'text-green' : 'text-orange'">
                {{ summary.streamStatus === 'ONLINE' ? '● 在线' : '○ 离线' }}
              </span>
            </div>
            <div class="meta-row">
              <span class="m-k">边端节点</span>
              <span class="m-v">{{ summary.clientName || 'FlowSight-Edge-Node-01' }}</span>
            </div>
            <div class="meta-row">
              <span class="m-k">软件版本</span>
              <span class="m-v">{{ summary.appVersion || 'v1.2.0' }}</span>
            </div>
          </div>

          <button class="btn-modal-action btn-gray-fill" style="margin-top: 14px;" @click="closeModal">
            完成
          </button>
        </div>
      </div>
    </div>

    <!-- 弹窗 7: 显示主题模式 -->
    <div v-if="activeModal === 'themeMode'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">显示主题</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="rate-select-list">
            <div
              v-for="t in themeOptions"
              :key="t.key"
              :class="['rate-card', currentTheme === t.key ? 'rate-card-active' : '']"
              @click="handleSelectTheme(t.key)"
            >
              <div class="rc-left">
                <span class="rc-radio">{{ currentTheme === t.key ? '●' : '○' }}</span>
                <span class="rc-title">{{ t.label }}</span>
              </div>
              <span class="rc-sub">{{ t.desc }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 8: 字号大小 -->
    <div v-if="activeModal === 'fontSize'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">字号大小</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="rate-select-list">
            <div
              v-for="f in fontScaleOptions"
              :key="f.key"
              :class="['rate-card', currentFontScale === f.key ? 'rate-card-active' : '']"
              @click="handleSelectFontScale(f.key)"
            >
              <div class="rc-left">
                <span class="rc-radio">{{ currentFontScale === f.key ? '●' : '○' }}</span>
                <span class="rc-title">{{ f.label }}</span>
              </div>
              <span class="rc-sub">{{ f.desc }}</span>
            </div>
          </div>

          <div class="font-preview-box">
            <div class="fp-title">字号效果预览</div>
            <div class="fp-content">
              累计通行 <b class="text-cyan">1,280 人次</b>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 9: 默认业务座舱选择 -->
    <div v-if="activeModal === 'defaultPoint'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">设置默认业务座舱</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="dialog-tip" style="margin-bottom: 12px; font-size: 12px; color: var(--fs-text-dim, #7A8CA5); line-height: 1.4;">
            选择每次登录或重新打开 FlowSight 时优先默认进入的业务通道。
          </div>
          <div class="rate-select-list">
            <!-- 默认首项 (自动不锁定) -->
            <div
              :class="['rate-card', pointStore.defaultPointId === null ? 'rate-card-active' : '']"
              @click="handleSelectDefaultPoint(null)"
            >
              <div class="rc-left">
                <span class="rc-radio">{{ pointStore.defaultPointId === null ? '●' : '○' }}</span>
                <span class="rc-title">跟随首项 (自动)</span>
              </div>
              <span class="rc-sub">默认加载首个通道</span>
            </div>

            <!-- 具体点位列表 -->
            <div
              v-for="p in pointStore.points"
              :key="p.id"
              :class="['rate-card', pointStore.defaultPointId === p.id ? 'rate-card-active' : '']"
              @click="handleSelectDefaultPoint(p.id)"
            >
              <div class="rc-left">
                <span class="rc-radio">{{ pointStore.defaultPointId === p.id ? '●' : '○' }}</span>
                <div>
                  <div class="rc-title" style="display:flex; align-items:center; gap:6px;">
                    <span>{{ p.pointName }}</span>
                    <span
                      style="font-size:10px; padding:1px 5px; border-radius:3px;"
                      :style="p.bizType === 'VEHICLE' ? 'background:rgba(255,160,0,0.15); color:#FFA000;' : 'background:rgba(0,210,255,0.15); color:#00D2FF;'"
                    >
                      {{ p.bizType === 'VEHICLE' ? '车流' : '客流' }}
                    </span>
                  </div>
                  <div style="font-size: 11px; color: var(--fs-text-dim, #7A8CA5);">{{ p.pointCode }}</div>
                </div>
              </div>
              <span class="rc-sub" :style="pointStore.defaultPointId === p.id ? 'color: var(--fs-primary, #00D2FF); font-weight:600;' : ''">
                {{ pointStore.defaultPointId === p.id ? '已设为默认' : '设为默认' }}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 10: 商户专属数字大屏投屏直达 -->
    <div v-if="activeModal === 'digitalScreen'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <span class="dh-title">🖥️ 专属数字大屏投屏直达</span>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="screen-dialog-alert">
            <span class="sda-icon">🔒</span>
            <div class="sda-text">
              本链接已附带您的商户长效免密安全凭据（180天有效），专用于企业前台电视、展厅监控墙或指挥大屏挂载，数据与其它客户严格物理隔离。
            </div>
          </div>

          <div class="modal-sub-meta" style="margin-top: 12px;">
            <span class="msm-k">所属企业</span>
            <span class="msm-v">{{ screenInfo.companyName || userProfile.companyName || '企业商户' }}</span>
          </div>

          <div class="screen-url-box">
            <div class="sub-label">大屏直达地址 (含免密凭证与用户标识)</div>
            <textarea
              class="screen-url-textarea"
              readonly
              :value="fullScreenUrl"
              rows="3"
            ></textarea>
          </div>

          <div class="modal-action-row" style="margin-top: 16px; display: flex; gap: 10px;">
            <button class="btn-modal-action btn-cyan-fill" style="flex: 1;" @click="handleCopyScreenUrl">
              📋 复制直达地址
            </button>
            <button class="btn-modal-action" style="flex: 1; background: rgba(0,210,255,0.12); color: #00D2FF; border: 1px solid rgba(0,210,255,0.3);" @click="handleOpenScreenPreview">
              🚀 预览打开
            </button>
          </div>
        </div>
      </div>
    </div>

  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, onActivated } from 'vue';
import { useRouter } from 'vue-router';
import { showToast, showDialog } from 'vant';
import { storeToRefs } from 'pinia';
import { h5Api, DashboardSummaryVO } from '../api';
import { usePointStore } from '../store/point';
import { useDashboardStore } from '../store/dashboard';
import { useThemeStore, ThemeMode, FontSizeScale } from '../store/theme';

const router = useRouter();
const pointStore = usePointStore();
const dashboardStore = useDashboardStore();
const themeStore = useThemeStore();

const { pollRateSec } = storeToRefs(dashboardStore);
const { currentTheme, currentFontScale } = storeToRefs(themeStore);
const activeRate = pollRateSec;

const fontScaleLabel = computed(() => {
  if (currentFontScale.value === 'large') return '关爱大字';
  if (currentFontScale.value === 'medium') return '中等';
  return '标准';
});

const currentThemeLabel = computed(() => {
  return currentTheme.value === 'light' ? '明亮浅色' : '深邃暗黑';
});

// 用户资料 (从本地缓存预热，并向云端实时同步)
const userProfile = ref<any>({
  phone: '',
  companyName: '',
  nickname: '',
  contactPerson: ''
});

const loadUserProfile = async () => {
  try {
    const raw = localStorage.getItem('flowsight_user');
    if (raw) {
      const u = JSON.parse(raw);
      if (u && (u.companyName || u.phone)) {
        userProfile.value = { ...userProfile.value, ...u };
      }
    }
  } catch (e) {}

  try {
    const remote: any = await h5Api.getUserProfile();
    if (remote && (remote.companyName || remote.phone)) {
      userProfile.value = { ...userProfile.value, ...remote };
      localStorage.setItem('flowsight_user', JSON.stringify(remote));
    }
  } catch (err) {
    console.warn('拉取云端商户资料异常:', err);
  }
};

// 对话框弹窗状态管理 (取代页面内部折叠)
type ModalType = 'pairing' | 'password' | 'devicePwd' | 'demographics' | 'refreshRate' | 'deviceInfo' | 'themeMode' | 'fontSize' | 'defaultPoint' | 'digitalScreen' | null;
const activeModal = ref<ModalType>(null);

// 商户专属数字大屏投屏凭证与地址
const screenInfo = ref<any>({
  userId: 0,
  companyName: '',
  screenToken: '',
  screenUrl: ''
});

const fullScreenUrl = computed(() => {
  if (!screenInfo.value?.screenUrl) return '';
  return window.location.origin + screenInfo.value.screenUrl;
});

const loadScreenUrl = async () => {
  try {
    const res: any = await h5Api.getScreenUrl();
    if (res) {
      screenInfo.value = res;
    }
  } catch (e) {
    showToast('获取大屏投屏链接失败');
  }
};

const handleCopyScreenUrl = () => {
  if (!fullScreenUrl.value) return;
  navigator.clipboard.writeText(fullScreenUrl.value).then(() => {
    showToast('大屏专属直达链接已复制到剪贴板！');
  }).catch(() => {
    showToast('复制失败，请长按文本手动复制');
  });
};

const handleOpenScreenPreview = () => {
  if (fullScreenUrl.value) {
    const win = window.open(fullScreenUrl.value, '_blank');
    if (!win) {
      window.location.href = fullScreenUrl.value;
    }
  }
};

const currentDefaultPointLabel = computed(() => {
  if (!pointStore.defaultPointId) return '跟随首项';
  const found = pointStore.points.find(p => p.id === pointStore.defaultPointId);
  return found ? found.pointName : '跟随首项';
});

const handleSelectDefaultPoint = async (pointId: number | null) => {
  try {
    await h5Api.setDefaultPoint(pointId);
    pointStore.setDefaultPoint(pointId);
    showToast(pointId ? '默认业务座舱已锁定' : '已恢复自动首项');
    closeModal();
  } catch (err: any) {
    showToast(err.message || '设置失败');
  }
};

const openModal = (type: ModalType) => {
  activeModal.value = type;
  if (type === 'digitalScreen') {
    screenInfo.value = {
      userId: userProfile.value.id || 0,
      companyName: userProfile.value.companyName || userProfile.value.nickname || '',
      screenToken: '',
      screenUrl: ''
    };
    loadScreenUrl();
  }
};

const closeModal = () => {
  activeModal.value = null;
};

// ⚡ 6 位动态配对码 (极速接入)
const pairingLoading = ref(false);
const pairingData = ref<any>(null);
const pairingRemainingSec = ref(0);
let pairingTimer: any = null;

const pairingDigits = computed(() => {
  if (!pairingData.value || !pairingData.value.pairingCode) return ['-', '-', '-', '-', '-', '-'];
  return String(pairingData.value.pairingCode).split('');
});

const handleGeneratePairingCode = async () => {
  const pid = summary.value.pointId || 1;
  pairingLoading.value = true;
  try {
    const res: any = await h5Api.generatePairingCode(pid);
    if (res) {
      pairingData.value = res;
      pairingRemainingSec.value = res.expiresInSeconds || 300;
      if (pairingTimer) clearInterval(pairingTimer);
      pairingTimer = setInterval(() => {
        if (pairingRemainingSec.value > 0) {
          pairingRemainingSec.value--;
        } else {
          clearInterval(pairingTimer);
          pairingData.value = null;
        }
      }, 1000);
      showToast({ message: '6 位配对码已生成，5 分钟内有效', position: 'top' });
    }
  } catch (e: any) {
    showToast(e.message || '生成失败');
  } finally {
    pairingLoading.value = false;
  }
};

const handleCopyPin = () => {
  if (!pairingData.value || !pairingData.value.pairingCode) return;
  navigator.clipboard.writeText(String(pairingData.value.pairingCode));
  showToast({ message: '配对码已复制到剪贴板', position: 'top' });
};

// 密码 1: 商户账号登录密码表单
const pwdForm = ref({
  oldPassword: '',
  newPassword: '',
  confirmPassword: ''
});
const pwdLoading = ref(false);

const handleChangePassword = async () => {
  if (!pwdForm.value.oldPassword) {
    showToast('请输入当前原密码');
    return;
  }
  if (!pwdForm.value.newPassword || pwdForm.value.newPassword.length < 6) {
    showToast('新密码长度不能少于6位');
    return;
  }
  if (pwdForm.value.newPassword !== pwdForm.value.confirmPassword) {
    showToast('两次输入的新密码不一致');
    return;
  }

  pwdLoading.value = true;
  try {
    await h5Api.changePassword({
      oldPassword: pwdForm.value.oldPassword,
      newPassword: pwdForm.value.newPassword
    });
    showToast({ message: '密码修改成功，请使用新密码重新登录', position: 'top' });
    pwdForm.value.oldPassword = '';
    pwdForm.value.newPassword = '';
    pwdForm.value.confirmPassword = '';
    closeModal();
    setTimeout(() => {
      handleLogout();
    }, 1500);
  } catch (e: any) {
  } finally {
    pwdLoading.value = false;
  }
};

// 密码 2: 边缘专属设备接入密码表单
const newDevicePassword = ref('');
const devPwdLoading = ref(false);
const pointCredential = ref<any>({
  pointId: 1,
  pointCode: 'POINT_VK_EAST_01',
  pointToken: '',
  hasDevicePassword: true
});

const loadPointCredential = async (pid: number) => {
  try {
    const res: any = await h5Api.getPointCredential(pid);
    if (res) pointCredential.value = res;
  } catch (e) {}
};

const handleResetDevicePassword = async () => {
  if (!newDevicePassword.value || newDevicePassword.value.length < 6) {
    showToast('新设备专属密码不能少于6位');
    return;
  }
  showDialog({
    title: '重置边缘设备专属密码 (密码 2)',
    message: '确认重置？重置后云端将立即吊销当前旧设备的连接凭证。现场边缘端必须重新输入此新密码认证后方可继续上传数据。',
    showCancelButton: true
  }).then(async () => {
    devPwdLoading.value = true;
    try {
      const pid = summary.value.pointId || 1;
      const res: any = await h5Api.resetDevicePassword(pid, { newDevicePassword: newDevicePassword.value.trim() });
      showToast({ message: '边缘设备专属接入密码已更新！', position: 'top' });
      newDevicePassword.value = '';
      closeModal();
      if (res) pointCredential.value = res;
    } catch (e: any) {
    } finally {
      devPwdLoading.value = false;
    }
  }).catch(() => {});
};

// 刷新频率选项
const refreshRates = [
  { sec: 5, label: '5 秒', desc: '近实时' },
  { sec: 10, label: '10 秒', desc: '标准' },
  { sec: 30, label: '30 秒', desc: '省流' }
];

const handleSelectRate = (sec: number) => {
  dashboardStore.setPollRate(sec);
  showToast({ message: `座舱刷新已设为 ${sec} 秒`, position: 'top', duration: 1000 });
  closeModal();
};

// 主题模式选项
const themeOptions = [
  {
    key: 'light' as ThemeMode,
    label: '明亮浅色',
    desc: '白底黑字 高对比'
  },
  {
    key: 'dark' as ThemeMode,
    label: '深邃暗黑',
    desc: '黑底科技 夜间柔和'
  }
];

const handleSelectTheme = (mode: ThemeMode) => {
  themeStore.setTheme(mode);
  showToast({ message: `已切换为${mode === 'light' ? '明亮浅色' : '深邃暗黑'}风格`, position: 'top', duration: 1000 });
  closeModal();
};

// 字号缩放无障碍选项
const fontScaleOptions = [
  {
    key: 'standard' as FontSizeScale,
    label: '标准',
    desc: '100% 默认'
  },
  {
    key: 'medium' as FontSizeScale,
    label: '中等',
    desc: '110% 放大'
  },
  {
    key: 'large' as FontSizeScale,
    label: '关爱大字',
    desc: '120% 适老'
  }
];

const handleSelectFontScale = (scale: FontSizeScale) => {
  themeStore.setFontScale(scale);
  showToast({ message: '字号设置已生效', position: 'top', duration: 1000 });
  closeModal();
};

// 汇总数据 (包含客群属性)
const summary = ref<DashboardSummaryVO>({
  pointId: 1,
  pointName: '',
  clientName: '',
  appVersion: 'v1.2.0',
  lastHeartbeat: '',
  streamStatus: 'ONLINE',
  currentOccupancy: 0,
  todayEntry: 0,
  todayExit: 0,
  todayTotal: 0,
  demographics: {
    maleCount: 0,
    femaleCount: 0,
    maleRatio: 0.5,
    femaleRatio: 0.5,
    topAgeGroup: '青年(18-35)'
  },
  hourlyChart: []
});

const loadSummary = async () => {
  try {
    const points: any = await h5Api.getMyPoints();
    if (points && points.length > 0) {
      const pid = points[0].id;
      const data: any = await h5Api.getDashboardSummary(pid);
      summary.value = data;
      loadPointCredential(pid);
    }
  } catch (e) {}
};

const handleLogout = () => {
  showDialog({
    title: '安全退出',
    message: '确定要退出当前 FlowSight 决策账号吗？',
    showCancelButton: true
  }).then(() => {
    dashboardStore.clearCache();
    localStorage.removeItem('flowsight_token');
    localStorage.removeItem('flowsight_user');
    localStorage.removeItem('flowsight_admin_token');
    screenInfo.value = {
      userId: 0,
      companyName: '',
      screenToken: '',
      screenUrl: ''
    };
    showToast('已安全登出');
    router.push('/login');
  }).catch(() => {});
};

onMounted(() => {
  dashboardStore.syncFromStorage();
  loadUserProfile();
  loadSummary();
});

onActivated(() => {
  dashboardStore.syncFromStorage();
  loadUserProfile();
});

onUnmounted(() => {
  if (pairingTimer) clearInterval(pairingTimer);
});
</script>

<style scoped>
.settings-viewport {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  box-sizing: border-box;
  background-color: var(--fs-bg, #0A0D14);
}

/* 顶部标题栏 */
.flat-header {
  flex-shrink: 0;
  height: 44px;
  background: var(--fs-header-bg, #0E131D);
  border-bottom: 1px solid var(--fs-header-border, #1E2636);
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
  background: var(--fs-primary, #00D2FF);
}

.header-title {
  font-size: 14px;
  font-weight: 700;
  color: var(--fs-text-main, #FFFFFF);
  letter-spacing: 0.5px;
}

.role-badge {
  background: rgba(0, 210, 255, 0.15);
  color: #00D2FF;
  font-size: 10px;
  font-weight: 700;
  padding: 2px 8px;
  border-radius: 4px;
  border: 1px solid rgba(0, 210, 255, 0.3);
}

/* 主体内容滚动区 */
.settings-scroll-body {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  -webkit-overflow-scrolling: touch;
  padding: 10px 12px 24px 12px;
  display: flex;
  flex-direction: column;
  gap: 12px;
  box-sizing: border-box;
}

/* 商户卡片 */
.profile-card {
  background: var(--fs-card, #121722);
  border: 1px solid var(--fs-card-border, #1E2636);
  border-radius: 8px;
  padding: 12px;
}

.profile-top {
  display: flex;
  align-items: center;
  gap: 10px;
}

.avatar-box {
  width: 38px;
  height: 38px;
  background: #182030;
  border-radius: 6px;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 18px;
  border: 1px solid #2A364F;
  flex-shrink: 0;
}

.profile-meta {
  flex: 1;
  min-width: 0;
}

.company-name {
  font-size: 13px;
  font-weight: 700;
  color: var(--fs-text-main, #F8FAFC);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.phone-str {
  font-size: 11px;
  color: #94A3B8;
  margin-top: 2px;
}

.auth-pill {
  font-size: 10px;
  color: #00E676;
  background: rgba(0, 230, 118, 0.12);
  padding: 2px 6px;
  border-radius: 3px;
  font-weight: 700;
  flex-shrink: 0;
}

/* 分组标题 */
.group-title {
  font-size: 11px;
  font-weight: 700;
  color: #64748B;
  padding-left: 4px;
  letter-spacing: 0.5px;
}

/* 纯平菜单列表卡片组 */
.menu-group {
  background: var(--fs-card, #121722);
  border: 1px solid var(--fs-card-border, #1E2636);
  border-radius: 8px;
  overflow: hidden;
  display: flex;
  flex-direction: column;
}

.menu-cell {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px;
  border-bottom: 1px solid #1A2230;
  cursor: pointer;
  transition: background 0.15s;
}
.menu-cell:last-child {
  border-bottom: none;
}
.menu-cell:active {
  background: #182030;
}

.cell-left {
  display: flex;
  align-items: center;
  gap: 10px;
  min-width: 0;
  flex: 1;
}

.cell-icon {
  font-size: 16px;
  flex-shrink: 0;
}

.cell-title {
  font-size: 13px;
  font-weight: 600;
  color: var(--fs-text-main, #E2E8F0);
}

.cell-right {
  display: flex;
  align-items: center;
  gap: 6px;
  flex-shrink: 0;
}

.cell-value {
  font-size: 12px;
  color: var(--fs-text-sub, #94A3B8);
  font-weight: 500;
}

.cell-chevron {
  font-size: 16px;
  color: var(--fs-text-dim, #64748B);
  font-weight: 300;
  line-height: 1;
}

.status-dot {
  font-size: 11px;
  font-weight: 600;
  padding: 1px 6px;
  border-radius: 3px;
}
.status-dot.online {
  color: #10B981;
  background: rgba(16, 185, 129, 0.12);
}
.status-dot.offline {
  color: #EF4444;
  background: rgba(239, 68, 68, 0.12);
}

/* 登出按钮 */
.logout-btn-box {
  margin-top: 10px;
}

.btn-logout {
  width: 100%;
  height: 40px;
  background: var(--fs-card, #182030);
  border: 1px solid var(--fs-card-border, #2A364F);
  border-radius: 6px;
  color: #FF3D71;
  font-size: 13px;
  font-weight: 700;
  cursor: pointer;
  transition: all 0.2s;
}
.btn-logout:active {
  background: rgba(255, 61, 113, 0.1);
  border-color: #FF3D71;
}

/* ================= 统一弹窗模态层 (Modal Overlays) ================= */
.dialog-overlay {
  position: fixed;
  inset: 0;
  background: rgba(0, 0, 0, 0.82);
  backdrop-filter: blur(4px);
  z-index: 1000;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 16px;
  box-sizing: border-box;
}

.dialog-card {
  width: 100%;
  max-width: 360px;
  background: var(--fs-card, #111827);
  border: 1px solid var(--fs-card-border, #1F293D);
  border-radius: 10px;
  overflow: hidden;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.4);
  display: flex;
  flex-direction: column;
}

.dialog-header {
  height: 44px;
  background: var(--fs-header-bg, #182030);
  border-bottom: 1px solid var(--fs-card-border, #1F293D);
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 14px;
}

.dh-title-box {
  display: flex;
  align-items: center;
  gap: 8px;
}

.dh-icon {
  font-size: 15px;
}

.dh-title {
  font-size: 13px;
  font-weight: 700;
  color: var(--fs-text-main, #F8FAFC);
}

.dh-close {
  background: transparent;
  border: none;
  color: var(--fs-text-sub, #94A3B8);
  font-size: 16px;
  cursor: pointer;
  padding: 4px;
}

.dialog-body {
  padding: 16px;
  box-sizing: border-box;
  background: var(--fs-card, #111827);
  color: var(--fs-text-main, #F8FAFC);
}

.modal-desc {
  font-size: 11.5px;
  color: var(--fs-text-sub, #94A3B8);
  line-height: 150%;
  margin: 0 0 12px 0;
}

.modal-sub-meta {
  display: flex;
  justify-content: space-between;
  align-items: center;
  background: var(--fs-card-secondary, #0B0E14);
  padding: 8px 10px;
  border-radius: 4px;
  border: 1px solid var(--fs-card-border, #1A2230);
  margin-bottom: 12px;
}
.msm-k {
  font-size: 11px;
  color: var(--fs-text-dim, #64748B);
}
.msm-v {
  font-size: 11px;
  font-weight: 700;
  color: var(--fs-text-main, #FFFFFF);
}
.cyan-code {
  color: #00D2FF;
  font-family: monospace;
}

.screen-dialog-alert {
  display: flex;
  gap: 8px;
  background: rgba(0, 210, 255, 0.08);
  border: 1px solid rgba(0, 210, 255, 0.25);
  border-radius: 6px;
  padding: 10px;
  font-size: 11px;
  line-height: 1.45;
  color: var(--fs-text-sub, #94A3B8);
}
.sda-icon {
  font-size: 14px;
  flex-shrink: 0;
}
.screen-url-box {
  margin-top: 10px;
}
.screen-url-box .sub-label {
  font-size: 11px;
  color: var(--fs-text-dim, #7A8CA5);
  margin-bottom: 4px;
}
.screen-url-textarea {
  width: 100%;
  box-sizing: border-box;
  background: rgba(0, 0, 0, 0.3);
  border: 1px solid var(--fs-card-border, #1F293D);
  border-radius: 6px;
  padding: 8px;
  font-size: 11px;
  color: var(--fs-primary, #00D2FF);
  font-family: monospace;
  resize: none;
  word-break: break-all;
}

/* 按钮样式规范 */
.btn-modal-action {
  width: 100%;
  height: 38px;
  border-radius: 6px;
  font-size: 12.5px;
  font-weight: 700;
  cursor: pointer;
  border: none;
  transition: all 0.2s;
}
.btn-cyan-fill {
  background: #00D2FF;
  color: #0A0D14;
}
.btn-cyan-fill:active {
  transform: scale(0.98);
}
.btn-blue-fill {
  background: #1F6FEB;
  color: #FFFFFF;
}
.btn-gray-fill {
  background: #182030;
  color: #CBD5E1;
  border: 1px solid #2A364F;
}

/* 6 位配对码大字号组件 */
.pairing-active-box {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.pairing-countdown {
  font-size: 11px;
  color: #94A3B8;
  text-align: center;
}
.text-orange {
  color: #FF9800;
  font-weight: 700;
}

.pin-digits-row {
  display: flex;
  justify-content: center;
  align-items: center;
  gap: 8px;
  margin: 8px 0;
}

.pin-digit-box {
  width: 42px;
  height: 50px;
  background: #0D1424;
  border: 1.5px solid #00D2FF;
  border-radius: 6px;
  color: #00D2FF;
  font-size: 24px;
  font-weight: 800;
  display: flex;
  align-items: center;
  justify-content: center;
  box-shadow: 0 0 12px rgba(0, 210, 255, 0.25);
  font-family: monospace;
}

.pairing-tip {
  font-size: 11px;
  color: #64748B;
  text-align: center;
  line-height: 140%;
}

.modal-action-row {
  display: flex;
  gap: 10px;
  margin-top: 6px;
}

.btn-copy-pin {
  flex: 1;
  height: 38px;
  background: #00D2FF;
  color: #0A0D14;
  border: none;
  font-size: 12px;
  font-weight: 700;
  border-radius: 6px;
  cursor: pointer;
}

.btn-refresh-pin {
  height: 38px;
  padding: 0 14px;
  background: #182030;
  color: #94A3B8;
  border: 1px solid #2A364F;
  font-size: 12px;
  border-radius: 6px;
  cursor: pointer;
}

/* 表单输入框 */
.modal-form {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.modal-form-item {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.mf-label {
  font-size: 11px;
  color: #94A3B8;
  font-weight: 600;
}

.mf-input {
  height: 36px;
  background: var(--fs-card-secondary, #0B0E14);
  border: 1px solid var(--fs-card-border, #2A364F);
  border-radius: 4px;
  padding: 0 10px;
  color: var(--fs-text-main, #FFFFFF);
  font-size: 12px;
  outline: none;
  box-sizing: border-box;
}
.mf-input:focus {
  border-color: var(--fs-primary, #00D2FF);
}

.dual-pwd-hint {
  font-size: 11px;
  color: #94A3B8;
  background: #0B0E14;
  border-left: 3px solid #00D2FF;
  padding: 8px 10px;
  border-radius: 2px;
  line-height: 150%;
  margin-bottom: 12px;
}

/* 客群画像内容 */
.demo-modal-content {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.gender-text-row {
  display: flex;
  justify-content: space-between;
  font-size: 11px;
  font-weight: 700;
}
.text-cyan { color: #00D2FF; }
.text-pink { color: #FF3D71; }
.text-green { color: #00E676; }

.flat-progress-bar {
  width: 100%;
  height: 8px;
  background: #0B0E14;
  border-radius: 4px;
  overflow: hidden;
  display: flex;
}

.bar-cyan {
  background: #00D2FF;
  height: 100%;
}

.bar-pink {
  background: #FF3D71;
  height: 100%;
}

.age-info-row {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 11px;
}

.info-lbl {
  color: #94A3B8;
}

.age-pill {
  font-size: 11px;
  font-weight: 700;
  color: #FFFFFF;
  background: #182030;
  border: 1px solid #2A364F;
  padding: 2px 8px;
  border-radius: 4px;
}

.ai-meta-box {
  background: #0B0E14;
  border: 1px solid #1A2230;
  border-radius: 6px;
  padding: 8px 12px;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.amb-item {
  display: flex;
  justify-content: space-between;
  font-size: 11px;
}
.amb-k { color: #64748B; }
.amb-v { color: #CBD5E1; font-weight: 600; }

/* 刷新频率选择卡片 */
.rate-select-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.rate-card {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: var(--fs-card-secondary, #182030);
  border: 1px solid var(--fs-card-border, #1F293D);
  border-radius: 8px;
  padding: 11px 14px;
  cursor: pointer;
  transition: all 0.15s;
}
.rate-card:active {
  opacity: 0.85;
}
.rate-card-active {
  border-color: var(--fs-primary, #00D2FF);
  background: rgba(0, 210, 255, 0.08);
}

.rc-left {
  display: flex;
  align-items: center;
  gap: 10px;
}

.rc-radio {
  color: var(--fs-primary, #00D2FF);
  font-size: 13px;
}

.rc-title {
  font-size: 13px;
  font-weight: 600;
  color: var(--fs-text-main, #F8FAFC);
}

.rc-sub {
  font-size: 11px;
  color: var(--fs-text-dim, #64748B);
}

.demo-count-row {
  margin-top: 12px;
  font-size: 12px;
  color: var(--fs-text-sub, #94A3B8);
  text-align: center;
}

/* 节点详情元数据列表 */
.node-meta-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
  background: var(--fs-card-secondary, #0B0E14);
  border: 1px solid var(--fs-card-border, #1A2230);
  border-radius: 6px;
  padding: 10px 12px;
}

.meta-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 11px;
}

.m-k {
  color: var(--fs-text-dim, #64748B);
}

.m-v {
  color: var(--fs-text-main, #CBD5E1);
  font-weight: 600;
}

/* 字号实时预览卡片 */
.font-preview-box {
  margin-top: 14px;
  background: var(--fs-card-secondary, #0B0E14);
  border: 1px dashed var(--fs-primary, #00D2FF);
  border-radius: 6px;
  padding: 10px 12px;
}
.fp-title {
  font-size: 11px;
  color: var(--fs-text-dim, #64748B);
  margin-bottom: 4px;
}
.fp-content {
  font-size: 13px;
  color: var(--fs-text-main, #F8FAFC);
  line-height: 140%;
}

/* ==================== 明亮浅色模式适配 (Light Theme) ==================== */
:global(.theme-light) .settings-viewport {
  background-color: var(--fs-chassis-bg);
}
:global(.theme-light) .flat-header {
  background: var(--fs-header-bg);
  border-bottom: 1px solid var(--fs-header-border);
}
:global(.theme-light) .header-title {
  color: var(--fs-text-main);
}
:global(.theme-light) .profile-card {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
}
:global(.theme-light) .company-name {
  color: var(--fs-text-main);
}
:global(.theme-light) .avatar-box {
  background: #F1F5F9;
  border-color: #E2E8F0;
}
:global(.theme-light) .phone-str {
  color: var(--fs-text-sub);
}
:global(.theme-light) .group-title {
  color: #475569;
}
:global(.theme-light) .menu-group {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.05);
}
:global(.theme-light) .menu-cell {
  border-bottom-color: #F1F5F9;
}
:global(.theme-light) .menu-cell:active {
  background: #F8FAFC;
}
:global(.theme-light) .cell-title {
  color: var(--fs-text-main);
}
:global(.theme-light) .cell-sub {
  color: var(--fs-text-sub);
}
:global(.theme-light) .gray-pill {
  background: #F1F5F9;
  color: #475569;
}
:global(.theme-light) .cyan-pill {
  background: rgba(2, 132, 199, 0.12);
  color: #0284C7;
}
:global(.theme-light) .blue-pill {
  background: rgba(37, 99, 235, 0.12);
  color: #2563EB;
}
:global(.theme-light) .purple-pill {
  background: rgba(124, 58, 237, 0.12);
  color: #7C3AED;
}
:global(.theme-light) .green-pill {
  background: rgba(22, 163, 74, 0.12);
  color: #16A34A;
}
:global(.theme-light) .btn-logout {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
}
:global(.theme-light) .dialog-card {
  background: #FFFFFF;
  border-color: #CBD5E1;
  box-shadow: 0 10px 30px rgba(15, 23, 42, 0.15);
}
:global(.theme-light) .dialog-header {
  background: #F8FAFC;
  border-bottom-color: #E2E8F0;
}
:global(.theme-light) .dh-title {
  color: var(--fs-text-main);
}
:global(.theme-light) .modal-desc {
  color: var(--fs-text-sub);
}
:global(.theme-light) .modal-sub-meta {
  background: #F8FAFC;
  border-color: #E2E8F0;
}
:global(.theme-light) .rate-card {
  background: #F8FAFC;
  border-color: #E2E8F0;
}
:global(.theme-light) .rate-card-active {
  border-color: #0284C7;
  background: rgba(2, 132, 199, 0.08);
}
:global(.theme-light) .rc-title {
  color: var(--fs-text-main);
}
:global(.theme-light) .rc-desc {
  color: var(--fs-text-dim);
}
:global(.theme-light) .mf-input {
  background: #F8FAFC;
  border-color: #CBD5E1;
  color: #0F172A;
}
:global(.theme-light) .btn-modal-action.btn-gray-fill {
  background: #F1F5F9;
  border-color: #CBD5E1;
  color: #334155;
}
:global(.theme-light) .node-meta-list {
  background: #F8FAFC;
  border-color: #E2E8F0;
}
:global(.theme-light) .m-v {
  color: var(--fs-text-main);
}
:global(.theme-light) .btn-copy-pin,
:global(.theme-light) .btn-refresh-pin {
  background: #F8FAFC;
  border-color: #CBD5E1;
  color: #0F172A;
}
:global(.theme-light) .pin-digit-box {
  background: #F8FAFC;
  border-color: #CBD5E1;
  color: #0284C7;
}
:global(.theme-light) .dual-pwd-hint {
  background: #F8FAFC;
  border-left-color: #0284C7;
  color: #334155;
}
:global(.theme-light) .font-preview-box {
  background: #F8FAFC;
  border-color: #0284C7;
}
:global(.theme-light) .fp-content {
  color: #0F172A;
}
</style>
