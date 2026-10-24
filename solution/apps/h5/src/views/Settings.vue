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

    <!-- 2. 主体内容区 (纯平菜单列表，拒绝页面内折叠跳动) -->
    <main class="settings-scroll-body">
      <!-- 商户租户卡片 -->
      <div class="profile-card">
        <div class="profile-top">
          <div class="avatar-box">🏢</div>
          <div class="profile-meta">
            <div class="company-name">{{ userProfile.companyName || '万科物业服务有限公司' }}</div>
            <div class="phone-str">📱 绑定账号: {{ userProfile.phone || '13454118762' }}</div>
          </div>
          <div class="auth-pill">已认证</div>
        </div>
      </div>

      <!-- 设置分组 1: 边端接入与安全凭据 -->
      <div class="group-title">边端接入与安全凭据</div>
      <div class="menu-group">
        <!-- 条目 1: 6 位动态配对码 -->
        <div class="menu-cell" @click="openModal('pairing')">
          <div class="cell-left">
            <span class="cell-icon">⚡</span>
            <div class="cell-text">
              <span class="cell-title">现场设备 6 位动态配对码</span>
              <span class="cell-sub">5 分钟动态 PIN 码 · 工控机 1 秒接入免输密码</span>
            </div>
          </div>
          <div class="cell-right">
            <span class="cell-action-pill cyan-pill">生成 PIN ➔</span>
          </div>
        </div>

        <!-- 条目 2: 边缘设备专属接入密码 (密码 2) -->
        <div class="menu-cell" @click="openModal('devicePwd')">
          <div class="cell-left">
            <span class="cell-icon">🛡️</span>
            <div class="cell-text">
              <span class="cell-title">边缘专属设备密码 (密码 2)</span>
              <span class="cell-sub">编码: {{ pointCredential.pointCode || 'POINT_VK_EAST_01' }} · 与主账号解耦</span>
            </div>
          </div>
          <div class="cell-right">
            <span class="cell-action-pill blue-pill">管理/重置 ➔</span>
          </div>
        </div>

        <!-- 条目 3: 修改登录密码 -->
        <div class="menu-cell" @click="openModal('password')">
          <div class="cell-left">
            <span class="cell-icon">🔐</span>
            <div class="cell-text">
              <span class="cell-title">修改账号登录密码</span>
              <span class="cell-sub">商户主账号登录与决策中台安全保护</span>
            </div>
          </div>
          <div class="cell-right">
            <span class="cell-action-pill gray-pill">修改 ➔</span>
          </div>
        </div>
      </div>

      <!-- 设置分组 2: 数据洞察与座舱偏好 -->
      <div class="group-title">数据洞察与座舱偏好</div>
      <div class="menu-group">
        <!-- 条目 4: 通道客群属性洞察画像 -->
        <div class="menu-cell" @click="openModal('demographics')">
          <div class="cell-left">
            <span class="cell-icon">👥</span>
            <div class="cell-text">
              <span class="cell-title">通道客群属性洞察画像</span>
              <span class="cell-sub">AI 年龄段与性别构成 · 商业客群特征</span>
            </div>
          </div>
          <div class="cell-right">
            <span class="cell-action-pill green-pill">查看洞察 ➔</span>
          </div>
        </div>

        <!-- 条目 5: 实时座舱刷新频率 -->
        <div class="menu-cell" @click="openModal('refreshRate')">
          <div class="cell-left">
            <span class="cell-icon">⏱️</span>
            <div class="cell-text">
              <span class="cell-title">实时座舱数据刷新频率</span>
              <span class="cell-sub">流速脉冲曲线与在场滞留步进周期</span>
            </div>
          </div>
          <div class="cell-right">
            <span class="cell-action-pill purple-pill">{{ activeRate }} 秒 ➔</span>
          </div>
        </div>

        <!-- 条目 6: 边端通道与设备信息 -->
        <div class="menu-cell" @click="openModal('deviceInfo')">
          <div class="cell-left">
            <span class="cell-icon">📡</span>
            <div class="cell-text">
              <span class="cell-title">边端连接与通道管理</span>
              <span class="cell-sub">{{ summary.pointName || '万科西区东门主入口' }}</span>
            </div>
          </div>
          <div class="cell-right">
            <span class="cell-action-pill" :class="summary.streamStatus === 'ONLINE' ? 'green-pill' : 'red-pill'">
              {{ summary.streamStatus === 'ONLINE' ? '● 在线' : '○ 离线' }} ➔
            </span>
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

    <!-- ==================== 统一弹窗对话框区域 (Modal Dialogs) ==================== -->

    <!-- 弹窗 1: 6 位动态配对码 (极速接入) -->
    <div v-if="activeModal === 'pairing'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <div class="dh-title-box">
            <span class="dh-icon">⚡</span>
            <span class="dh-title">现场设备 6 位动态配对码</span>
          </div>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="modal-sub-meta">
            <span class="msm-k">目标通道:</span>
            <span class="msm-v cyan-code">{{ pointCredential.pointName || summary.pointName || '当前通道' }} ({{ pointCredential.pointCode || 'POINT_01' }})</span>
          </div>

          <div v-if="!pairingData" class="pairing-init-box">
            <p class="modal-desc">
              无需向施工人员透露账号主密码。点击下方按钮即可生成 5 分钟有效的一次性 6 位数字配对码，工控机输入后即可自动完成身份鉴权与点位绑定。
            </p>
            <button class="btn-modal-action btn-cyan-fill" :disabled="pairingLoading" @click="handleGeneratePairingCode">
              {{ pairingLoading ? '正在生成配对码...' : '⚡ 立即生成 6 位接入配对码' }}
            </button>
          </div>

          <div v-else class="pairing-active-box">
            <div class="pairing-countdown">
              <span>⏳ 配对码有效期剩余: <b class="text-orange">{{ pairingRemainingSec }}秒</b></span>
            </div>
            <!-- 6 位大字号排版卡片 -->
            <div class="pin-digits-row">
              <span v-for="(digit, idx) in pairingDigits" :key="idx" class="pin-digit-box">
                {{ digit }}
              </span>
            </div>
            <div class="pairing-tip">
              💡 施工人员在工控机客户端「设置 → 云端协同」中输入上述 6 位数字即可 1 秒接入。
            </div>
            <div class="modal-action-row">
              <button class="btn-copy-pin" @click="handleCopyPin">
                📋 复制 6 位配对码
              </button>
              <button class="btn-refresh-pin" :disabled="pairingLoading" @click="handleGeneratePairingCode">
                🔄 重新生成
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 2: 边缘专属设备密码 (密码 2) -->
    <div v-if="activeModal === 'devicePwd'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <div class="dh-title-box">
            <span class="dh-icon">🛡️</span>
            <span class="dh-title">边缘专属设备密码 (密码 2)</span>
          </div>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="modal-sub-meta">
            <span class="msm-k">点位唯一编码:</span>
            <span class="msm-v cyan-code">{{ pointCredential.pointCode || 'POINT_VK_EAST_01' }}</span>
          </div>

          <div class="dual-pwd-hint">
            💡 <b>双密码安全隔离说明</b>：此密码仅供现场摄像头工控机接入鉴权，与您的商户主账号完全解耦。现场设备报废或移机时，在此重置即可吊销旧凭据。
          </div>

          <div class="modal-form">
            <div class="modal-form-item">
              <label class="mf-label">新设备专属密码:</label>
              <input
                v-model="newDevicePassword"
                type="password"
                placeholder="请输入新设备密码（至少6位）"
                class="mf-input"
              />
            </div>
            <button class="btn-modal-action btn-cyan-fill" :disabled="devPwdLoading" @click="handleResetDevicePassword">
              {{ devPwdLoading ? '正在重置凭证...' : '确认重置边缘专属密码 (密码 2)' }}
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 3: 修改登录密码 -->
    <div v-if="activeModal === 'password'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <div class="dh-title-box">
            <span class="dh-icon">🔐</span>
            <span class="dh-title">修改账号登录密码</span>
          </div>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="modal-form">
            <div class="modal-form-item">
              <label class="mf-label">当前原密码:</label>
              <input
                v-model="pwdForm.oldPassword"
                type="password"
                placeholder="请输入当前原密码"
                class="mf-input"
              />
            </div>
            <div class="modal-form-item">
              <label class="mf-label">新密码:</label>
              <input
                v-model="pwdForm.newPassword"
                type="password"
                placeholder="请输入新密码（至少6位）"
                class="mf-input"
              />
            </div>
            <div class="modal-form-item">
              <label class="mf-label">确认新密码:</label>
              <input
                v-model="pwdForm.confirmPassword"
                type="password"
                placeholder="再次输入新密码"
                class="mf-input"
              />
            </div>
            <button class="btn-modal-action btn-blue-fill" :disabled="pwdLoading" @click="handleChangePassword">
              {{ pwdLoading ? '密码验签提交中...' : '确认修改登录密码' }}
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 4: 通道客群属性洞察画像 -->
    <div v-if="activeModal === 'demographics'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <div class="dh-title-box">
            <span class="dh-icon">👥</span>
            <span class="dh-title">通道客群属性洞察画像</span>
          </div>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="demo-modal-content">
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
            </div>

            <div class="ai-meta-box">
              <div class="amb-item">
                <span class="amb-k">AI 多属性识别精度</span>
                <span class="amb-v text-cyan">94.6% 置信度</span>
              </div>
              <div class="amb-item">
                <span class="amb-k">今日抓拍样本量</span>
                <span class="amb-v">{{ summary.todayTotal || 0 }} 样本</span>
              </div>
              <div class="amb-item">
                <span class="amb-k">数据模型</span>
                <span class="amb-v">Edge YOLOv8n + ByteTrack</span>
              </div>
            </div>

            <button class="btn-modal-action btn-gray-fill" @click="closeModal">
              关闭洞察
            </button>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 5: 实时座舱数据刷新频率 -->
    <div v-if="activeModal === 'refreshRate'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <div class="dh-title-box">
            <span class="dh-icon">⏱️</span>
            <span class="dh-title">实时座舱数据刷新频率</span>
          </div>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <p class="modal-desc">
            选择手机端决策座舱近实时同步流速脉冲折线与滞留人数的刷新步进周期：
          </p>
          <div class="rate-select-list">
            <div
              v-for="r in refreshRates"
              :key="r.sec"
              :class="['rate-card', activeRate === r.sec ? 'rate-card-active' : '']"
              @click="handleSelectRate(r.sec)"
            >
              <div class="rc-left">
                <span class="rc-radio">{{ activeRate === r.sec ? '●' : '○' }}</span>
                <div class="rc-text">
                  <span class="rc-title">{{ r.label }}</span>
                  <span class="rc-desc">{{ r.desc }}</span>
                </div>
              </div>
              <span v-if="activeRate === r.sec" class="rc-tag">当前生效</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <!-- 弹窗 6: 边端通道与设备详情 -->
    <div v-if="activeModal === 'deviceInfo'" class="dialog-overlay" @click.self="closeModal">
      <div class="dialog-card">
        <div class="dialog-header">
          <div class="dh-title-box">
            <span class="dh-icon">📡</span>
            <span class="dh-title">边端连接与通道详情</span>
          </div>
          <button class="dh-close" @click="closeModal">✕</button>
        </div>
        <div class="dialog-body">
          <div class="node-meta-list">
            <div class="meta-row">
              <span class="m-k">当前点位:</span>
              <span class="m-v">{{ summary.pointName || '万科西区东门主入口' }} [ID: {{ summary.pointId || 1 }}]</span>
            </div>
            <div class="meta-row">
              <span class="m-k">通道编码:</span>
              <span class="m-v cyan-code">{{ pointCredential.pointCode || 'POINT_VK_EAST_01' }}</span>
            </div>
            <div class="meta-row">
              <span class="m-k">边端节点:</span>
              <span class="m-v">{{ summary.clientName || 'FlowSight-Edge-Node-01' }}</span>
            </div>
            <div class="meta-row">
              <span class="m-k">推流状态:</span>
              <span class="m-v" :class="summary.streamStatus === 'ONLINE' ? 'text-green' : 'text-orange'">
                {{ summary.streamStatus === 'ONLINE' ? '🟢 ONLINE (正常)' : '🔴 OFFLINE (离线)' }}
              </span>
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

          <button class="btn-modal-action btn-gray-fill" style="margin-top: 14px;" @click="closeModal">
            关闭
          </button>
        </div>
      </div>
    </div>

  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue';
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

// 对话框弹窗状态管理 (取代页面内部折叠)
type ModalType = 'pairing' | 'password' | 'devicePwd' | 'demographics' | 'refreshRate' | 'deviceInfo' | null;
const activeModal = ref<ModalType>(null);

const openModal = (type: ModalType) => {
  activeModal.value = type;
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
  { sec: 5, label: '5 秒 (近实时推荐)', desc: '动态流速波形每 5 秒刷新，感知极度灵敏' },
  { sec: 10, label: '10 秒 (标准模式)', desc: '标准中台步进周期，平衡设备与数据通量' },
  { sec: 30, label: '30 秒 (省流模式)', desc: '低频后台巡检，适合长期待机与节约流量' }
];
const activeRate = ref(Number(localStorage.getItem('flowsight_poll_rate') || 5));

const handleSelectRate = (sec: number) => {
  activeRate.value = sec;
  localStorage.setItem('flowsight_poll_rate', String(sec));
  showToast({ message: `座舱刷新已设为 ${sec} 秒`, position: 'top', duration: 1000 });
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
    localStorage.removeItem('flowsight_token');
    localStorage.removeItem('flowsight_user');
    showToast('已安全登出');
    router.push('/login');
  }).catch(() => {});
};

onMounted(() => {
  loadSummary();
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
  font-size: 14px;
  font-weight: 700;
  color: #FFFFFF;
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
  background: #121722;
  border: 1px solid #1E2636;
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
  color: #F8FAFC;
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
  background: #121722;
  border: 1px solid #1E2636;
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

.cell-text {
  display: flex;
  flex-direction: column;
  gap: 2px;
  min-width: 0;
}

.cell-title {
  font-size: 12.5px;
  font-weight: 700;
  color: #E2E8F0;
}

.cell-sub {
  font-size: 10px;
  color: #64748B;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.cell-right {
  flex-shrink: 0;
  margin-left: 8px;
}

.cell-action-pill {
  font-size: 11px;
  font-weight: 600;
  padding: 3px 8px;
  border-radius: 4px;
}
.cyan-pill {
  background: rgba(0, 210, 255, 0.12);
  color: #00D2FF;
}
.blue-pill {
  background: rgba(31, 111, 235, 0.15);
  color: #58A6FF;
}
.gray-pill {
  background: #1A2230;
  color: #94A3B8;
}
.green-pill {
  background: rgba(0, 230, 118, 0.12);
  color: #00E676;
}
.purple-pill {
  background: rgba(157, 78, 221, 0.15);
  color: #BC8CFF;
}
.red-pill {
  background: rgba(255, 61, 113, 0.15);
  color: #FF3D71;
}

/* 登出按钮 */
.logout-btn-box {
  margin-top: 10px;
}

.btn-logout {
  width: 100%;
  height: 40px;
  background: #182030;
  border: 1px solid #2A364F;
  border-radius: 6px;
  color: #FF3D71;
  font-size: 12px;
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
  max-width: 440px;
  background: #111827;
  border: 1px solid #1F293D;
  border-radius: 10px;
  overflow: hidden;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.5);
  display: flex;
  flex-direction: column;
}

.dialog-header {
  height: 44px;
  background: #182030;
  border-bottom: 1px solid #1F293D;
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
  color: #F8FAFC;
}

.dh-close {
  background: transparent;
  border: none;
  color: #94A3B8;
  font-size: 16px;
  cursor: pointer;
  padding: 4px;
}

.dialog-body {
  padding: 16px;
  box-sizing: border-box;
}

.modal-desc {
  font-size: 11.5px;
  color: #94A3B8;
  line-height: 150%;
  margin: 0 0 12px 0;
}

.modal-sub-meta {
  display: flex;
  justify-content: space-between;
  align-items: center;
  background: #0B0E14;
  padding: 8px 10px;
  border-radius: 4px;
  border: 1px solid #1A2230;
  margin-bottom: 12px;
}
.msm-k {
  font-size: 11px;
  color: #94A3B8;
}
.msm-v {
  font-size: 11px;
  font-weight: 700;
}
.cyan-code {
  color: #00D2FF;
  font-family: monospace;
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
  background: #0B0E14;
  border: 1px solid #2A364F;
  border-radius: 4px;
  padding: 0 10px;
  color: #FFFFFF;
  font-size: 12px;
  outline: none;
  box-sizing: border-box;
}
.mf-input:focus {
  border-color: #00D2FF;
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
  background: #0B0E14;
  border: 1px solid #1F293D;
  border-radius: 6px;
  padding: 10px 12px;
  cursor: pointer;
  transition: all 0.2s;
}
.rate-card:active {
  background: #182030;
}
.rate-card-active {
  border-color: #00D2FF;
  background: rgba(0, 210, 255, 0.06);
}

.rc-left {
  display: flex;
  align-items: center;
  gap: 10px;
}

.rc-radio {
  color: #00D2FF;
  font-size: 14px;
}

.rc-text {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.rc-title {
  font-size: 12px;
  font-weight: 700;
  color: #F8FAFC;
}

.rc-desc {
  font-size: 10px;
  color: #64748B;
}

.rc-tag {
  font-size: 9px;
  color: #00D2FF;
  background: rgba(0, 210, 255, 0.15);
  padding: 2px 6px;
  border-radius: 3px;
  font-weight: 600;
}

/* 节点详情元数据列表 */
.node-meta-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
  background: #0B0E14;
  border: 1px solid #1A2230;
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
  color: #64748B;
}

.m-v {
  color: #CBD5E1;
  font-weight: 600;
}
</style>
