<template>
  <div class="system-config-container">
    <!-- 顶部状态提示 -->
    <el-alert
      type="info"
      :closable="false"
      show-icon
      class="mb-20 alert-box"
    >
      <template #title>
        <span style="font-weight: bold; font-size: 14px;">全局系统功能与特性开关规范：</span>
      </template>
      <div style="font-size: 13px; line-height: 1.6; margin-top: 6px;">
        此处配置全局生效的业务能力开关。<b>如果现场或云端暂未架设流媒体转码服务器及视频切片下载节点</b>，请保持<b>【视频播放与证据切片功能】为关闭状态</b>。
        关闭后，系统将自动隐藏 H5 移动决策端的底部【证据切片】导航条及相关回放入口，杜绝客户看到无法加载的空白视频；后续一旦部署就绪，随时在此一键开启即可实时恢复展示。
      </div>
    </el-alert>

    <!-- 配置项卡片列表 -->
    <el-card shadow="never" class="config-card">
      <template #header>
        <div class="card-header">
          <div class="header-title">
            <el-icon><Operation /></el-icon>
            <span>功能开关矩阵</span>
          </div>
          <el-button :icon="Refresh" circle @click="loadConfigs" :loading="loading" title="刷新配置状态" />
        </div>
      </template>

      <div v-loading="loading" class="feature-list">
        <!-- 核心功能 1: 视频播放与切片回放开关 -->
        <div class="feature-item">
          <div class="feature-left">
            <div class="feature-icon-box">
              <span class="f-icon">🎬</span>
            </div>
            <div class="feature-info">
              <div class="feature-name-row">
                <span class="f-name">视频播放与证据切片回放</span>
                <el-tag
                  :type="videoPlaybackEnabled ? 'success' : 'info'"
                  size="small"
                  effect="plain"
                  class="ml-10"
                >
                  {{ videoPlaybackEnabled ? '● 已开启 (H5展示切片)' : '○ 已关闭 (H5隐藏切片)' }}
                </el-tag>
                <el-tag size="small" type="warning" effect="light" class="ml-10">默认推荐：关闭</el-tag>
              </div>
              <p class="f-desc">
                控制全域是否开启现场视频推流与过车/过人证据短视频切片回放。当前无视频服务器时请关闭，H5 移动端将自适应隐藏底部【证据切片】导航栏。
              </p>
              <div class="f-meta">
                <span>配置键名: <code>video_playback_enabled</code></span>
                <span class="ml-20">影响范围: H5 移动决策中台底部 TabBar、事件切片回放组件</span>
              </div>
            </div>
          </div>

          <div class="feature-right">
            <el-switch
              v-model="videoPlaybackEnabled"
              active-text="开启"
              inactive-text="关闭"
              inline-prompt
              size="large"
              :loading="savingKey === 'video_playback_enabled'"
              @change="handleVideoSwitchChange"
            />
          </div>
        </div>

        <!-- 核心功能 2: 历年累计起算基准日期配置 -->
        <div class="feature-item mt-20">
          <div class="feature-left">
            <div class="feature-icon-box">
              <span class="f-icon">📅</span>
            </div>
            <div class="feature-info">
              <div class="feature-name-row">
                <span class="f-name">大屏历年累计起算基准日</span>
                <el-tag size="small" type="success" effect="plain" class="ml-10">当前展示: 自{{ historicalStartDate || '2026年10月26日' }}起</el-tag>
              </div>
              <p class="f-desc">
                控制数字大屏【现场客流/车流量统计】中【历年累计】卡片的起算日期说明，默认为系统部署上线日（2026年10月26日），您可在此自由选择或输入任意基准日期。
              </p>
              <div class="f-meta">
                <span>配置键名: <code>historical_start_date</code></span>
                <span class="ml-20">影响范围: 可视化数字大屏现场流量统计卡片</span>
              </div>
            </div>
          </div>

          <div class="feature-right">
            <el-date-picker
              v-model="historicalStartDate"
              type="date"
              format="YYYY年M月D日"
              value-format="YYYY年M月D日"
              placeholder="选择起算基准日"
              style="width: 175px; margin-right: 10px;"
            />
            <el-button
              type="primary"
              :loading="savingKey === 'historical_start_date'"
              @click="handleSaveHistoricalStartDate"
            >
              保存基准日
            </el-button>
          </div>
        </div>
      </div>
    </el-card>

    <!-- 底部运维技术状态卡片 -->
    <el-card shadow="never" class="config-card mt-20">
      <template #header>
        <div class="header-title">
          <el-icon><InfoFilled /></el-icon>
          <span>多端与边缘采集协同运行状态</span>
        </div>
      </template>

      <div class="status-grid">
        <div class="status-cell">
          <span class="s-k">当前配置生效模式</span>
          <span class="s-v text-blue">云端与 Redis 双层毫秒级热同步</span>
        </div>
        <div class="status-cell">
          <span class="s-k">H5 移动端自适应逻辑</span>
          <span class="s-v" :class="videoPlaybackEnabled ? 'text-green' : 'text-orange'">
            {{ videoPlaybackEnabled ? '4 栏经典导航 (含证据切片)' : '3 栏纯净导航 (证据切片已动态隐藏)' }}
          </span>
        </div>
        <div class="status-cell">
          <span class="s-k">视频切片存储规格</span>
          <span class="s-v">MinIO S3 / 本地离线持久化存储</span>
        </div>
        <div class="status-cell">
          <span class="s-k">边缘端算法上报策略</span>
          <span class="s-v text-green">结构化元数据与坐标流水优先 (零流量卡顿)</span>
        </div>
      </div>
    </el-card>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue';
import { ElMessage } from 'element-plus';
import { Refresh, Operation, InfoFilled, Calendar } from '@element-plus/icons-vue';
import { adminApi } from '../api';

const loading = ref(false);
const savingKey = ref('');
const videoPlaybackEnabled = ref(false);
const historicalStartDate = ref('2026年10月26日');

const loadConfigs = async () => {
  loading.value = true;
  try {
    const list: any = await adminApi.getSystemConfigs();
    if (Array.isArray(list)) {
      const videoCfg = list.find((item: any) => item.configKey === 'video_playback_enabled');
      if (videoCfg) {
        const val = String(videoCfg.configValue).trim().toLowerCase();
        videoPlaybackEnabled.value = (val === 'true' || val === '1');
      } else {
        videoPlaybackEnabled.value = false;
      }

      const dateCfg = list.find((item: any) => item.configKey === 'historical_start_date');
      if (dateCfg && dateCfg.configValue) {
        historicalStartDate.value = String(dateCfg.configValue).trim();
      }
    }
  } catch (err: any) {
    ElMessage.error(err.message || '加载系统配置失败');
  } finally {
    loading.value = false;
  }
};

const handleSaveHistoricalStartDate = async () => {
  const dateVal = historicalStartDate.value.trim();
  if (!dateVal) {
    ElMessage.warning('起始基准日不能为空！');
    return;
  }
  savingKey.value = 'historical_start_date';
  try {
    await adminApi.updateSystemConfig({
      configKey: 'historical_start_date',
      configValue: dateVal
    });
    ElMessage.success(`已保存历年累计起算基准日为: ${dateVal}`);
  } catch (err: any) {
    ElMessage.error(err.message || '保存起始基准日失败');
  } finally {
    savingKey.value = '';
  }
};

const handleVideoSwitchChange = async (val: any) => {
  const boolVal = !!val;
  savingKey.value = 'video_playback_enabled';
  try {
    await adminApi.updateSystemConfig({
      configKey: 'video_playback_enabled',
      configValue: boolVal ? 'true' : 'false'
    });
    ElMessage.success(
      boolVal
        ? '已开启视频切片功能！H5 移动端将实时展示【证据切片】导航栏'
        : '已关闭视频切片功能！H5 移动端已动态隐藏【证据切片】导航栏'
    );
  } catch (err: any) {
    videoPlaybackEnabled.value = !boolVal; // 失败回滚
    ElMessage.error(err.message || '更新功能开关失败');
  } finally {
    savingKey.value = '';
  }
};

onMounted(() => {
  loadConfigs();
});
</script>

<style scoped>
.system-config-container {
  padding: 10px;
}

.mb-20 {
  margin-bottom: 20px;
}

.mt-20 {
  margin-top: 20px;
}

.ml-10 {
  margin-left: 10px;
}

.ml-20 {
  margin-left: 20px;
}

.alert-box {
  border-radius: 8px;
  background-color: #f0f7ff;
  border: 1px solid #c8e1ff;
}

.config-card {
  border-radius: 8px;
  border: 1px solid #e1e4e8;
}

.card-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.header-title {
  display: flex;
  align-items: center;
  gap: 8px;
  font-weight: 600;
  font-size: 15px;
  color: #24292f;
}

.feature-list {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.feature-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 18px 20px;
  background: #f6f8fa;
  border-radius: 8px;
  border: 1px solid #d0d7de;
  transition: all 0.2s ease;
}

.feature-item:hover {
  border-color: #0969da;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.04);
}

.feature-left {
  display: flex;
  align-items: flex-start;
  gap: 16px;
}

.feature-icon-box {
  width: 44px;
  height: 44px;
  border-radius: 8px;
  background: #ffffff;
  border: 1px solid #d0d7de;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}

.f-icon {
  font-size: 22px;
}

.feature-info {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.feature-name-row {
  display: flex;
  align-items: center;
}

.f-name {
  font-size: 15px;
  font-weight: 600;
  color: #24292f;
}

.f-desc {
  font-size: 13px;
  color: #57606a;
  margin: 4px 0 6px 0;
  line-height: 1.5;
}

.f-meta {
  font-size: 12px;
  color: #8c959f;
}

.f-meta code {
  background: #eaeef2;
  padding: 2px 6px;
  border-radius: 4px;
  color: #0969da;
  font-family: Consolas, monospace;
}

.feature-right {
  margin-left: 24px;
}

.status-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 16px;
}

.status-cell {
  background: #f6f8fa;
  padding: 14px 16px;
  border-radius: 6px;
  border: 1px solid #d0d7de;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.s-k {
  font-size: 12px;
  color: #57606a;
}

.s-v {
  font-size: 13px;
  font-weight: 600;
  color: #24292f;
}

.text-blue {
  color: #0969da;
}

.text-green {
  color: #1a7f37;
}

.text-orange {
  color: #bf8700;
}
</style>
