<template>
  <div class="login-container">
    <div class="login-card">
      <div class="login-title">
        <span class="logo">👁️</span>
        <h2>客流智能统计系统 运营管理平台</h2>
        <p>多租户客流大数据与边缘设备管控中心</p>
      </div>

      <el-form :model="form" :rules="rules" ref="formRef" size="large" @keyup.enter="handleLogin">
        <el-form-item prop="phone">
          <el-input v-model="form.phone" placeholder="管理员手机号" prefix-icon="User" />
        </el-form-item>
        <el-form-item prop="password">
          <el-input v-model="form.password" type="password" placeholder="密码" prefix-icon="Lock" show-password />
        </el-form-item>
        <el-form-item>
          <el-button type="primary" :loading="loading" class="login-btn" @click="handleLogin">
            立即登录管理中台
          </el-button>
        </el-form-item>
      </el-form>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive } from 'vue';
import { useRouter } from 'vue-router';
import { ElMessage } from 'element-plus';
import { adminApi } from '../api';

const router = useRouter();
const formRef = ref();
const loading = ref(false);

const form = reactive({
  phone: '',
  password: ''
});

const rules = {
  phone: [{ required: true, message: '请输入管理员手机号', trigger: 'blur' }],
  password: [{ required: true, message: '请输入密码', trigger: 'blur' }]
};

const handleLogin = async () => {
  if (!formRef.value) return;
  await formRef.value.validate(async (valid: boolean) => {
    if (!valid) return;
    loading.value = true;
    try {
      const res: any = await adminApi.login({
        phone: form.phone,
        password: form.password
      });
      if (res.role !== 'ROLE_ADMIN') {
        ElMessage.error('该账号非超级管理员角色，无权访问管理中台！');
        return;
      }
      localStorage.setItem('flowsight_admin_token', res.token);
      localStorage.setItem('flowsight_admin_user', JSON.stringify(res));
      ElMessage.success('管理员登录成功');
      router.push('/overview');
    } catch (e) {
      //
    } finally {
      loading.value = false;
    }
  });
};
</script>

<style scoped>
.login-container {
  height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  background: linear-gradient(135deg, #1f2329 0%, #0d1117 100%);
}

.login-card {
  width: 420px;
  background: #fff;
  border-radius: 12px;
  padding: 40px;
  box-shadow: 0 12px 32px rgba(0, 0, 0, 0.25);
}

.login-title {
  text-align: center;
  margin-bottom: 28px;
}

.login-title .logo {
  font-size: 40px;
  display: block;
  margin-bottom: 8px;
}

.login-title h2 {
  margin: 0 0 6px 0;
  color: #1f2329;
  font-size: 22px;
}

.login-title p {
  margin: 0;
  font-size: 13px;
  color: #8c96a3;
}

.login-btn {
  width: 100%;
  font-size: 15px;
  border-radius: 6px;
}
</style>
