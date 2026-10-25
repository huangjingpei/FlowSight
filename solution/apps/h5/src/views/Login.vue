<template>
  <div class="login-viewport">
    <div class="login-card">
      <!-- 极简品牌头部 (纯平设计，无旋转圈套) -->
      <div class="brand-box">
        <h1 class="brand-title">FlowSight 移动决策中台</h1>
        <p class="brand-sub">边缘视觉感知 · 商业客流数字孪生中枢</p>
      </div>

      <!-- 扁平登录表单 -->
      <form @submit.prevent="onSubmit" class="login-form">
        <div class="form-group">
          <label class="form-label">账号手机号</label>
          <input
            v-model="phone"
            type="tel"
            placeholder="请输入登录手机号"
            class="flat-input"
            required
          />
        </div>

        <div class="form-group">
          <label class="form-label">访问口令</label>
          <input
            v-model="password"
            type="password"
            placeholder="请输入密码"
            class="flat-input"
            required
          />
        </div>

        <!-- 快捷填入万科物业账号 -->
        <div class="quick-row" @click="fillVankeAccount">
          <span class="quick-link">⚡ 填入万科物业测试账号 (13454118762)</span>
        </div>

        <button type="submit" class="btn-submit" :disabled="loading">
          <span>{{ loading ? '身份验证中...' : '登录中台 ➔' }}</span>
        </button>
      </form>

      <div class="login-foot">
        <span>FlowSight AI Edge-Cloud Platform v1.2</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';
import { useRouter } from 'vue-router';
import { showToast } from 'vant';
import { authApi } from '../api';

const router = useRouter();
const phone = ref('13454118762');
const password = ref('13454118762');
const loading = ref(false);

const fillVankeAccount = () => {
  phone.value = '13454118762';
  password.value = '13454118762';
  showToast({ message: '已填入万科物业账号', position: 'top', duration: 1000 });
};

const onSubmit = async () => {
  if (!phone.value || !password.value) {
    showToast('请输入手机号和密码');
    return;
  }
  loading.value = true;
  try {
    const res: any = await authApi.login({
      phone: phone.value,
      password: password.value
    });
    localStorage.setItem('flowsight_token', res.token);
    localStorage.setItem('flowsight_user', JSON.stringify(res));
    showToast({ message: '登录成功', position: 'top' });
    router.push('/dashboard');
  } catch (e: any) {
    //
  } finally {
    loading.value = false;
  }
};
</script>

<style scoped>
.login-viewport {
  width: 100%;
  height: 100%;
  background-color: var(--fs-bg, #0A0D14);
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 16px;
  box-sizing: border-box;
}

.login-card {
  width: 100%;
  max-width: 360px;
  background: var(--fs-card, #121722);
  border: 1px solid var(--fs-card-border, #1E2636);
  border-radius: 8px;
  padding: 24px 20px;
  box-sizing: border-box;
  display: flex;
  flex-direction: column;
}

.brand-box {
  text-align: center;
  margin-bottom: 24px;
}

.brand-title {
  margin: 0 0 6px 0;
  font-size: 20px;
  font-weight: 800;
  color: #FFFFFF;
}

.brand-sub {
  margin: 0;
  font-size: 12px;
  color: #94A3B8;
}

.form-group {
  margin-bottom: 14px;
}

.form-label {
  display: block;
  font-size: 12px;
  font-weight: 700;
  color: #CBD5E1;
  margin-bottom: 6px;
}

.flat-input {
  width: 100%;
  height: 38px;
  background: #182030;
  border: 1px solid #2A364F;
  border-radius: 6px;
  padding: 0 12px;
  font-size: 14px;
  color: #FFFFFF;
  font-weight: 600;
  outline: none;
  box-sizing: border-box;
}
.flat-input:focus {
  border-color: #00D2FF;
}

.quick-row {
  margin-bottom: 18px;
  text-align: right;
  cursor: pointer;
}

.quick-link {
  font-size: 11px;
  color: #00D2FF;
  font-weight: 600;
}

.btn-submit {
  width: 100%;
  height: 42px;
  background: #00D2FF;
  color: #0A0D14;
  border: none;
  border-radius: 6px;
  font-size: 14px;
  font-weight: 800;
  cursor: pointer;
  transition: opacity 0.15s;
}
.btn-submit:active {
  opacity: 0.85;
}
.btn-submit:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.login-foot {
  margin-top: 20px;
  text-align: center;
  font-size: 10px;
  color: #64748B;
}

/* ================= 浅色高对比度主题适配 ================= */
:global(.theme-light) .login-viewport {
  background-color: var(--fs-bg);
}

:global(.theme-light) .login-card {
  background: var(--fs-card);
  border-color: var(--fs-card-border);
  box-shadow: 0 4px 20px rgba(15, 23, 42, 0.08);
}

:global(.theme-light) .brand-title {
  color: var(--fs-text-main);
}

:global(.theme-light) .brand-sub {
  color: var(--fs-text-sub);
}

:global(.theme-light) .form-label {
  color: var(--fs-text-sub);
}

:global(.theme-light) .flat-input {
  background: #F8FAFC;
  border-color: #CBD5E1;
  color: #0F172A;
}

:global(.theme-light) .flat-input:focus {
  border-color: var(--fs-primary);
}

:global(.theme-light) .quick-link {
  color: var(--fs-primary);
}

:global(.theme-light) .btn-submit {
  background: var(--fs-primary);
  color: #FFFFFF;
}

:global(.theme-light) .login-foot {
  color: var(--fs-text-dim);
}
</style>