<template>
  <div class="login-wrapper">
    <div class="login-header">
      <div class="logo-box">👁️</div>
      <h2>FlowSight 移动决策中台</h2>
      <p>智能客流与客群画像运营分析系统</p>
    </div>

    <van-form @submit="onSubmit" class="login-form">
      <van-cell-group inset>
        <van-field
          v-model="phone"
          name="phone"
          label="手机号"
          placeholder="请输入注册手机号"
          type="tel"
          :rules="[{ required: true, message: '请填写手机号' }]"
        />
        <van-field
          v-model="password"
          type="password"
          name="password"
          label="密码"
          placeholder="请输入密码（默认同手机号）"
          :rules="[{ required: true, message: '请填写密码' }]"
        />
      </van-cell-group>
      <div style="margin: 24px 16px;">
        <van-button round block type="primary" native-type="submit" :loading="loading">
          立即登录
        </van-button>
      </div>
    </van-form>

    <div class="login-footer">
      <span>FlowSight AI Edge-Cloud Platform v1.2</span>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import { useRouter } from 'vue-router';
import { showToast } from 'vant';
import { authApi } from '../api';

const router = useRouter();
const phone = ref('13800000000');
const password = ref('13800000000');
const loading = ref(false);

const onSubmit = async () => {
  loading.value = true;
  try {
    const res: any = await authApi.login({
      phone: phone.value,
      password: password.value
    });
    localStorage.setItem('flowsight_token', res.token);
    localStorage.setItem('flowsight_user', JSON.stringify(res));
    showToast('登录成功');
    router.push('/dashboard');
  } catch (e: any) {
    // 错误在拦截器中已提示
  } finally {
    loading.value = false;
  }
};
</script>

<style scoped>
.login-wrapper {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  justify-content: center;
  background: linear-gradient(180deg, #1989fa 0%, #f7f8fa 40%);
  padding-bottom: 40px;
}

.login-header {
  text-align: center;
  color: #fff;
  margin-bottom: 24px;
}

.logo-box {
  font-size: 48px;
  margin-bottom: 8px;
}

.login-header h2 {
  margin: 0 0 6px 0;
  font-size: 22px;
}

.login-header p {
  margin: 0;
  font-size: 13px;
  opacity: 0.85;
}

.login-footer {
  text-align: center;
  font-size: 11px;
  color: #aaa;
  margin-top: 30px;
}
</style>
