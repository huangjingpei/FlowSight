<template>
  <div class="overview-container">
    <!-- 顶栏 KPI 指标矩阵 -->
    <el-row :gutter="20" class="mb-4">
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card border-blue">
          <div class="stat-num">{{ userTotal }}</div>
          <div class="stat-label">👥 注册商户总数</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card border-green">
          <div class="stat-num">{{ pointTotal }}</div>
          <div class="stat-label">📹 在线与布防点位数</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card border-orange">
          <div class="stat-num">{{ onlinePointCount }}</div>
          <div class="stat-label">⚡ 实时活跃推流通道</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="hover" class="stat-card border-purple">
          <div class="stat-num">YOLOv8 + ByteTrack</div>
          <div class="stat-label">🧠 边缘端核心算法底座</div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 点位实时监控列表概览 -->
    <el-card shadow="never" class="mt-4">
      <template #header>
        <div class="card-header">
          <span>点位全景状态速查 (实时心跳与设备负载)</span>
          <el-button type="primary" size="small" @click="fetchData">刷新数据</el-button>
        </div>
      </template>

      <el-table :data="pointList" v-loading="loading" stripe style="width: 100%">
        <el-table-column prop="id" label="ID" width="80" />
        <el-table-column prop="pointName" label="点位名称" width="160" />
        <el-table-column prop="userPhone" label="归属商户手机号" width="150" />
        <el-table-column prop="address" label="物理安装地址" min-width="180" />
        <el-table-column prop="resolution" label="流分辨率" width="120" />
        <el-table-column prop="currentFps" label="实时FPS" width="100">
          <template #default="{ row }">
            <el-tag size="small" :type="row.currentFps > 10 ? 'success' : 'info'">
              {{ row.currentFps ? row.currentFps.toFixed(1) + ' FPS' : 'N/A' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="currentOccupancy" label="瞬时滞留" width="100">
          <template #default="{ row }">
            <span style="font-weight: bold; color: #1989fa;">{{ row.currentOccupancy || 0 }} 人</span>
          </template>
        </el-table-column>
        <el-table-column prop="streamStatus" label="状态" width="100">
          <template #default="{ row }">
            <el-tag :type="row.streamStatus === 'ONLINE' ? 'success' : 'danger'" size="small">
              {{ row.streamStatus === 'ONLINE' ? '在线' : '离线' }}
            </el-tag>
          </template>
        </el-table-column>
      </el-table>
    </el-card>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue';
import { adminApi } from '../api';

const loading = ref(false);
const userTotal = ref(0);
const pointTotal = ref(0);
const onlinePointCount = ref(0);
const pointList = ref<any[]>([]);

const fetchData = async () => {
  loading.value = true;
  try {
    const [uRes, pRes]: [any, any] = await Promise.all([
      adminApi.getUsers(1, 1),
      adminApi.getPoints(1, 20)
    ]);
    userTotal.value = uRes.total || 0;
    pointTotal.value = pRes.total || 0;
    pointList.value = pRes.records || [];
    onlinePointCount.value = pointList.value.filter((p) => p.streamStatus === 'ONLINE').length;
  } catch (e) {
    //
  } finally {
    loading.value = false;
  }
};

onMounted(() => {
  fetchData();
});
</script>

<style scoped>
.mb-4 {
  margin-bottom: 20px;
}
.stat-card {
  border-radius: 8px;
  text-align: center;
  padding: 10px 0;
}
.border-blue { border-top: 4px solid #1989fa; }
.border-green { border-top: 4px solid #07c160; }
.border-orange { border-top: 4px solid #ff976a; }
.border-purple { border-top: 4px solid #7232dd; }

.stat-num {
  font-size: 26px;
  font-weight: bold;
  color: #24292f;
  margin-bottom: 6px;
  font-family: 'Consolas', monospace;
}
.stat-label {
  font-size: 13px;
  color: #57606a;
}
.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-weight: 600;
}
</style>
