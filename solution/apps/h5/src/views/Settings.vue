<template>
  <div class="settings-viewport">
    <!-- 1. 顶部嵌入式标题栏 -->
    <header class="flat-header">
      <div class="header-left">
        <span class="header-dot"></span>
        <span class="header-title">系统设置与商户中心</span>
      </div>
      <div class="role-badge">
        <span>商户认证</span>
      </div>
    </header>

    <!-- 2. 主体内容区 (可内部轻量滚动) -->
    <main class="settings-scroll-body">
      <!-- 商户租户卡片 -->
      <div class="profile-card">
        <div class="profile-top">
          <div class="avatar-box">🏢</div>
          <div class="profile-meta">
            <div class="company-name">{{ userProfile.companyName || '万科物业服务有限公司' }}</div>
            <div class="phone-str">📱 绑定账号: {{ userProfile.phone || '13454118762' }}</div>
          </div>
        </div>
      </div>

      <!-- 设置条目 1: 修改密码 -->
      <div class="settings-card">
        <div class="card-header" @click="showPwdForm = !showPwdForm">
          <div class="header-title-box">
            <span class="item-icon">🔐</span>
            <span class="item-title">账号登录密码修改</span>
          </div>
          <span class="toggle-btn">{{ showPwdForm ? '收起 ▲' : '修改 ▼' }}</span>
        </div>

        <div v-if="showPwdForm" class="pwd-form-wrap">
          <div class="form-item">
            <span class="f-label">原密码:</span>
            <input
              v-model="pwdForm.oldPassword"
              type="password"
              placeholder="请输入当前原密码"
              class="f-input"
            />
          </div>
          <div class="form-item">
            <span class="f-label">新密码:</span>
            <input
              v-model="pwdForm.newPassword"
              type="password"
              placeholder="请输入新密码（至少6位）"
              class="f-input"
            />
          </div>
          <div class="form-item">
            <span class="f-label">确认新密码:</span>
            <input
              v-model="pwdForm.confirmPassword"
              type="password"
              placeholder="再次输入新密码"
              class="f-input"
            />
          </div>
          <button class="btn-change-pwd" :disabled="pwdLoading" @click="handleChangePassword">
            {{ pwdLoading ? '密码验签提交中...' : '确认修改密码' }}
          </button>
        </div>
      </div>

      <!-- 设置条目 2: 通道客群属性画像 (收纳于设置条目中) -->
      <div class="settings-card">
        <div class="card-header" @click="showDemographics = !showDemographics">
          <div class="header-title-box">
            <span class="item-icon">👥</span>
            <span class="item-title">通道客群属性洞察</span>
          </div>
          <span class="toggle-btn">{{ showDemographics ? '收起 ▲' : '展开 ▼' }}</span>
        </div>

        <div v-if="showDemographics" class="demo-content">
          <div class="gender-text-row" v-if="summary.demographics">
            <span class="text-cyan">👨 男性: {{ (summary.demographics.maleRatio * 100).toFixed(1) }}% ({{ summary.demographics.maleCount }}人)</span>
            <span class="text-pink">👩 女性: {{ (summary.demographics.femaleRatio * 100).toFixed(1) }}% ({{ summary.demographics.femaleCount }}人)</span>
          </div>

          <div class="flat-progress-bar" v-if="summary.demographics">
            <div class="bar-cyan" :style="{ width: (summary.demographics.maleRatio * 100) + '%' }"></div>
            <div class="bar-pink" :style="{ width: (summary.demographics.femaleRatio * 100) + '%' }"></div>
          </div>

          <div class="age-info-row" v-if="summary.demographics">
            <span class="info-lbl">主力到场客群:</span>
            <span class="age-pill">{{ summary.demographics.topAgeGroup || '青年(18-35)' }}</span>
            <span class="conf-lbl">AI 置信度: 94.6%</span>
          </div>
        </div>
      </div>

      <!-- 设置条目 3: 通道点位与边缘连接状态 -->
      <div class="settings-card">
        <div class="card-header">
          <div class="header-title-box">
            <span class="item-icon">📡</span>
            <span class="item-title">边端连接与通道管理</span>
          </div>
          <span class="point-status-tag" :class="summary.streamStatus === 'ONLINE' ? 'online' : 'offline'">
            {{ summary.streamStatus === 'ONLINE' ? '🟢 在线' : '🔴 离线' }}
          </span>
        </div>
        <div class="node-meta-list">
          <div class="meta-row">
            <span class="m-k">当前点位:</span>
            <span class="m-v">{{ summary.pointName || '万科西区东门主入口' }} [ID: {{ summary.pointId || 1 }}]</span>
          </div>
          <div class="meta-row">
            <span class="m-k">边端节点:</span>
            <span class="m-v">{{ summary.clientName || 'FlowSight-Edge-Node-01' }}</span>
          </div>
          <div class="meta-row">
            <span class="m-k">软件版本:</span>
            <span class="m-v">{{ summary.appVersion || 'v1.2.0' }}</span>
          </div>
          <div class="meta-row">
            <span class="m-k">最近心跳:</span>
            <span class="m-v">{{ summary.lastHeartbeat || '实时心跳正常' }}</span>
          </div>
        </div>
      </div>

      <!-- 设置条目 4: 实时座舱数据刷新频率偏好 -->
      <div class="settings-card">
        <div class="card-header">
          <div class="header-title-box">
            <span class="item-icon">⏱️</span>
            <span class="item-title">实时座舱刷新频率</span>
          </div>
        </div>
        <div class="rate-select-row">
          <div
            v-for="r in refreshRates"
            :key="r.sec"
            :class="['rate-pill', activeRate === r.sec ? 'active' : '']"
            @click="setRefreshRate(r.sec)"
          >
            {{ r.label }}
          </div>
        </div>
      </div>

      <!-- 安全登出按钮 -->
      <div class="logout-btn-box">
        <button class="btn-logout" @click="handleLogout">
          <span>安全退出当前决策账号</span>
        </button>
      </div>
    </main>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted } from 'vue';
import { useRouter } from 'vue-router';
import { showToast, showDialog } from 'vant';
import { h5Api, DashboardSummaryVO } from '../api';
import { usePointStore } from '../store/point';

const router = useRouter();
const pointStore = usePointStore();

// 用户资料
const userProfile = computed(() => {
  try {
    const raw = localStorage.getItem('flowsight_user');
    if (raw) return JSON.parse(raw);
  } catch (e) {}
  return { phone: '13454118762', companyName: '万科物业服务有限公司(未来城项目)' };
});

// 折叠状态
const showPwdForm = ref(false);
const showDemographics = ref(true);

// 密码表单
const pwdForm = ref({
  oldPassword: '',
  newPassword: '',
  confirmPassword: ''
});
const pwdLoading = ref(false);

// 刷新频率选项
const refreshRates = [
  { sec: 5, label: '5 秒 (近实时推荐)' },
  { sec: 10, label: '10 秒 (标准)' },
  { sec: 30, label: '30 秒 (省流)' }
];
const activeRate = ref(Number(localStorage.getItem('flowsight_poll_rate') || 5));

const setRefreshRate = (sec: number) => {
  activeRate.value = sec;
  localStorage.setItem('flowsight_poll_rate', String(sec));
  showToast({ message: `座舱刷新已设为 ${sec} 秒`, position: 'top', duration: 1000 });
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
    }
  } catch (e) {}
};

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
    showPwdForm.value = false;
    setTimeout(() => {
      handleLogout();
    }, 1500);
  } catch (e: any) {
    // 错误在拦截器中提示
  } finally {
    pwdLoading.value = false;
  }
};

const handleLogout = () => {
  showDialog({
    title: '安全退出',
    message: '确定要退出当前 FlowSight 决策账号吗？',
    showCancelButton: true
  }).then(() => {
    localStorage.removeItem('flowsight_token');
    localStorage.removeItem('flowsight_user');
    showToast('已安全登出');
    router.push('/login');
  }).catch(() => {});
};

onMounted(() => {
  loadSummary();
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

.role-badge {
  background: #182030;
  color: #00E676;
  border: 1px solid #2A364F;
  padding: 2px 8px;
  border-radius: 4px;
  font-size: 10px;
  font-weight: 700;
}

/* 内部轻量滚动容器 */
.settings-scroll-body {
  flex: 1;
  min-height: 0;
  overflow-y: auto;
  padding: 10px;
  display: flex;
  flex-direction: column;
  gap: 8px;
  box-sizing: border-box;
}

/* 商户资料卡片 */
.profile-card {
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 12px;
}

.profile-top {
  display: flex;
  align-items: center;
  gap: 12px;
}

.avatar-box {
  font-size: 28px;
  background: #182030;
  border: 1px solid #2A364F;
  border-radius: 8px;
  padding: 6px;
}

.profile-meta {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.company-name {
  font-size: 13px;
  font-weight: 800;
  color: #FFFFFF;
}

.phone-str {
  font-size: 11px;
  color: #94A3B8;
  font-weight: 600;
}

/* 设置项通用卡片 */
.settings-card {
  background: #121722;
  border: 1px solid #1E2636;
  border-radius: 6px;
  padding: 10px 12px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  cursor: pointer;
}

.header-title-box {
  display: flex;
  align-items: center;
  gap: 6px;
}

.item-icon {
  font-size: 13px;
}

.item-title {
  font-size: 12px;
  font-weight: 700;
  color: #CBD5E1;
}

.toggle-btn {
  font-size: 11px;
  color: #00D2FF;
  font-weight: 600;
}

/* 密码修改表单 */
.pwd-form-wrap {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding-top: 8px;
  border-top: 1px solid #182030;
}

.form-item {
  display: flex;
  align-items: center;
  gap: 8px;
}

.f-label {
  width: 72px;
  font-size: 11px;
  color: #94A3B8;
  font-weight: 600;
  flex-shrink: 0;
}

.f-input {
  flex: 1;
  height: 28px;
  background: #182030;
  border: 1px solid #2A364F;
  border-radius: 4px;
  padding: 0 8px;
  color: #FFFFFF;
  font-size: 12px;
  outline: none;
}
.f-input:focus {
  border-color: #00D2FF;
}

.btn-change-pwd {
  margin-top: 4px;
  height: 30px;
  background: #00D2FF;
  color: #0A0D14;
  font-size: 12px;
  font-weight: 800;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

/* 客群画像折叠项 */
.demo-content {
  display: flex;
  flex-direction: column;
  gap: 6px;
  padding-top: 6px;
  border-top: 1px solid #182030;
}

.gender-text-row {
  display: flex;
  justify-content: space-between;
  font-size: 11px;
  font-weight: 700;
}
.text-cyan { color: #00D2FF; }
.text-pink { color: #FF4081; }

.flat-progress-bar {
  display: flex;
  height: 6px;
  border-radius: 3px;
  overflow: hidden;
  background: #182030;
}
.bar-cyan { background: #00D2FF; }
.bar-pink { background: #FF4081; }

.age-info-row {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 11px;
}
.info-lbl { color: #94A3B8; }
.age-pill { background: #182030; color: #00D2FF; font-weight: 700; padding: 1px 6px; border-radius: 3px; font-size: 10px; }
.conf-lbl { margin-left: auto; color: #64748B; font-size: 10px; }

/* 节点信息 */
.point-status-tag {
  font-size: 10px;
  font-weight: 700;
}
.online { color: #00E676; }
.offline { color: #FF3D71; }

.node-meta-list {
  display: flex;
  flex-direction: column;
  gap: 4px;
  font-size: 11px;
}

.meta-row {
  display: flex;
  justify-content: space-between;
}
.m-k { color: #94A3B8; }
.m-v { color: #FFFFFF; font-weight: 600; }

/* 刷新频率选择 */
.rate-select-row {
  display: flex;
  gap: 6px;
}

.rate-pill {
  flex: 1;
  height: 28px;
  line-height: 28px;
  text-align: center;
  background: #182030;
  border: 1px solid #2A364F;
  border-radius: 4px;
  font-size: 11px;
  color: #94A3B8;
  font-weight: 600;
  cursor: pointer;
}
.rate-pill.active {
  background: #00D2FF;
  color: #0A0D14;
  font-weight: 800;
  border-color: #00D2FF;
}

/* 登出按钮 */
.logout-btn-box {
  margin-top: 8px;
  margin-bottom: 8px;
}

.btn-logout {
  width: 100%;
  height: 36px;
  background: #1E2636;
  border: 1px solid #2A364F;
  color: #FF3D71;
  font-size: 13px;
  font-weight: 800;
  border-radius: 6px;
  cursor: pointer;
}
.btn-logout:active {
  background: rgba(255, 61, 113, 0.15);
}
</style>