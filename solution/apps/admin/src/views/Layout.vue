<template>
  <el-container class="layout-container">
    <el-aside width="240px" class="aside">
      <div class="logo-bar">
        <span class="logo-icon">👁️</span>
        <span class="logo-text">FlowSight Cloud</span>
      </div>
      <el-menu
        :default-active="route.path"
        router
        class="el-menu-vertical"
        background-color="#1a1d22"
        text-color="#c9d1d9"
        active-text-color="#58a6ff"
      >
        <el-menu-item index="/overview">
          <el-icon><DataLine /></el-icon>
          <span>运营监控大盘</span>
        </el-menu-item>
        <el-menu-item index="/users">
          <el-icon><UserFilled /></el-icon>
          <span>商户用户管理</span>
        </el-menu-item>
        <el-menu-item index="/points">
          <el-icon><VideoCamera /></el-icon>
          <span>点位资产管理</span>
        </el-menu-item>
        <el-menu-item index="/business-types">
          <el-icon><Operation /></el-icon>
          <span>业务类型管理</span>
        </el-menu-item>
        <el-menu-item index="/screen" @click.prevent="openScreenWindow">
          <el-icon><Monitor /></el-icon>
          <span style="color: #00f0ff; font-weight: bold;">🖥️ 实时数字大屏</span>
        </el-menu-item>
      </el-menu>
    </el-aside>

    <el-container>
      <el-header class="header">
        <div class="header-left">
          <span class="page-title">{{ route.meta.title || '管理平台' }}</span>
        </div>
        <div class="header-right">
          <el-button type="primary" size="small" class="screen-btn" @click="openScreenWindow">
            🖥️ 打开全景数字大屏
          </el-button>
          <span class="admin-badge">👑 超级管理员</span>
          <el-button type="danger" size="small" link @click="handleLogout">退出登录</el-button>
        </div>
      </el-header>

      <el-main class="main-content">
        <router-view />
      </el-main>
    </el-container>
  </el-container>
</template>

<script setup lang="ts">
import { useRoute, useRouter } from 'vue-router';
import { ElMessageBox } from 'element-plus';

const route = useRoute();
const router = useRouter();

const openScreenWindow = () => {
  window.open('#/screen', '_blank');
};

const handleLogout = () => {
  ElMessageBox.confirm('确认退出当前管理中台吗？', '提示', {
    confirmButtonText: '确定',
    cancelButtonText: '取消',
    type: 'warning'
  }).then(() => {
    localStorage.removeItem('flowsight_admin_token');
    router.push('/login');
  });
};
</script>

<style scoped>
.layout-container {
  height: 100vh;
}

.aside {
  background-color: #1a1d22;
  color: #fff;
  display: flex;
  flex-direction: column;
}

.logo-bar {
  height: 60px;
  display: flex;
  align-items: center;
  padding: 0 20px;
  background-color: #14161a;
  border-bottom: 1px solid #282c34;
}

.logo-icon {
  font-size: 24px;
  margin-right: 10px;
}

.logo-text {
  font-size: 16px;
  font-weight: bold;
  color: #58a6ff;
  letter-spacing: 0.5px;
}

.el-menu-vertical {
  border-right: none;
  flex: 1;
}

.header {
  background-color: #fff;
  border-bottom: 1px solid #e1e4e8;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 24px;
}

.page-title {
  font-size: 16px;
  font-weight: 600;
  color: #24292f;
}

.screen-btn {
  background: linear-gradient(90deg, #0072ff 0%, #00c6ff 100%);
  border: none;
  font-weight: bold;
  color: #fff;
  margin-right: 14px;
  box-shadow: 0 2px 8px rgba(0, 198, 255, 0.35);
  transition: all 0.25s ease;
}

.screen-btn:hover {
  background: linear-gradient(90deg, #005ce6 0%, #00aee6 100%);
  transform: translateY(-1px);
  box-shadow: 0 4px 12px rgba(0, 198, 255, 0.5);
}

.admin-badge {
  font-size: 13px;
  color: #57606a;
  margin-right: 16px;
}

.main-content {
  background-color: #f6f8fa;
  padding: 24px;
  overflow-y: auto;
}
</style>
