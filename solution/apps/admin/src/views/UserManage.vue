<template>
  <div class="user-manage-container">
    <el-card shadow="never">
      <div class="toolbar">
        <el-input
          v-model="keyword"
          placeholder="搜索手机号 / 企业客户全称 / 联系人"
          style="width: 320px;"
          clearable
          @clear="fetchUsers"
          @keyup.enter="fetchUsers"
        >
          <template #append>
            <el-button icon="Search" @click="fetchUsers" />
          </template>
        </el-input>

        <el-button type="primary" icon="Plus" @click="openCreateDialog">
          添加商户客户
        </el-button>
      </div>

      <el-table :data="tableData" v-loading="loading" stripe style="width: 100%; margin-top: 16px;">
        <el-table-column prop="id" label="客户ID" width="80" />
        <el-table-column prop="phone" label="登录账号 (手机号)" width="150">
          <template #default="{ row }">
            <span style="font-weight: 600; color: #1989fa;">{{ row.phone }}</span>
          </template>
        </el-table-column>
        <el-table-column label="企业全称 / 商户名称" min-width="180">
          <template #default="{ row }">
            <div style="font-weight: 600; font-size: 14px; color: #303133;">
              {{ row.companyName || row.nickname || '未设定企业名' }}
            </div>
            <div v-if="row.nickname && row.nickname !== row.companyName" style="color: #909399; font-size: 12px;">
              简称: {{ row.nickname }}
            </div>
          </template>
        </el-table-column>
        <el-table-column label="责任联系人" width="130">
          <template #default="{ row }">
            <span>{{ row.contactPerson || '未登记' }}</span>
          </template>
        </el-table-column>
        <el-table-column label="点位配额占用" width="170">
          <template #default="{ row }">
            <el-tag
              style="cursor: pointer;"
              :type="row.pointCount >= (row.pointQuota || 5) ? 'danger' : 'primary'"
              @click="jumpToPoints(row)"
            >
              📹 {{ row.pointCount }} / {{ row.pointQuota || 5 }} 点位 
              {{ row.pointCount >= (row.pointQuota || 5) ? '(满额)' : '' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="SaaS订阅有效期" width="170">
          <template #default="{ row }">
            <span v-if="row.expireTime" :style="{ color: isExpired(row.expireTime) ? '#f56c6c' : '#606266' }">
              {{ formatTime(row.expireTime) }}
              <el-tag v-if="isExpired(row.expireTime)" type="danger" size="small" style="margin-left: 4px;">已过期</el-tag>
            </span>
            <span v-else style="color: #909399;">永久有效</span>
          </template>
        </el-table-column>
        <el-table-column prop="status" label="状态" width="90">
          <template #default="{ row }">
            <el-tag :type="row.status === 1 ? 'success' : 'danger'" size="small">
              {{ row.status === 1 ? '正常' : '禁用' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="280" fixed="right">
          <template #default="{ row }">
            <el-button size="small" link type="success" @click="handleOpenScreen(row)">
              🖥️ 专属大屏
            </el-button>
            <el-button size="small" link type="primary" @click="jumpToPoints(row)">
              点位管理
            </el-button>
            <el-button size="small" link type="warning" @click="openEditDialog(row)">
              编辑商户
            </el-button>
            <el-button size="small" link type="danger" @click="handleDelete(row)">
              删除账号
            </el-button>
          </template>
        </el-table-column>
      </el-table>

      <div class="pagination-bar">
        <el-pagination
          v-model:current-page="page"
          v-model:page-size="size"
          :total="total"
          layout="total, prev, pager, next"
          @current-change="fetchUsers"
        />
      </div>
    </el-card>

    <!-- 添加商户客户对话框 -->
    <el-dialog v-model="createVisible" title="开通商户客户账号 (含点位配额与初始点位)" width="520px">
      <el-form :model="createForm" :rules="createRules" ref="createFormRef" label-width="120px">
        <el-form-item label="登录手机号" prop="phone">
          <el-input v-model="createForm.phone" placeholder="11位手机号 (唯一凭证)" />
        </el-form-item>
        <el-form-item label="企业客户全称" prop="companyName">
          <el-input v-model="createForm.companyName" placeholder="例如: 杭州栩文科技有限公司" />
        </el-form-item>
        <el-form-item label="商户项目简称">
          <el-input v-model="createForm.nickname" placeholder="例如: 杭州总部项目" />
        </el-form-item>
        <el-form-item label="责任联系人">
          <el-input v-model="createForm.contactPerson" placeholder="例如: 刘经理 / 张主任" />
        </el-form-item>
        <el-form-item label="允许点位配额">
          <el-input-number v-model="createForm.pointQuota" :min="1" :max="100" />
          <span style="margin-left: 10px; color: #909399; font-size: 12px;">上限点位数</span>
        </el-form-item>
        <el-form-item label="SaaS订阅到期">
          <el-date-picker
            v-model="createForm.expireTime"
            type="datetime"
            placeholder="选择授权到期时间"
            value-format="YYYY-MM-DD HH:mm:ss"
            style="width: 100%;"
          />
        </el-form-item>
        <el-form-item label="登录初始密码">
          <el-input v-model="createForm.password" placeholder="留空则默认与手机号同号" />
        </el-form-item>
        <el-form-item label="初始首个点位">
          <el-input v-model="createForm.initialPointName" placeholder="例如: 东门主通道 (可选自动创建)" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="createVisible = false">取消</el-button>
        <el-button type="primary" :loading="submitLoading" @click="submitCreate">确认开通</el-button>
      </template>
    </el-dialog>

    <!-- 修改商户客户对话框 -->
    <el-dialog v-model="editVisible" title="修改商户客户信息与配额" width="500px">
      <el-form :model="editForm" label-width="120px">
        <el-form-item label="企业客户全称">
          <el-input v-model="editForm.companyName" />
        </el-form-item>
        <el-form-item label="商户项目简称">
          <el-input v-model="editForm.nickname" />
        </el-form-item>
        <el-form-item label="责任联系人">
          <el-input v-model="editForm.contactPerson" />
        </el-form-item>
        <el-form-item label="允许点位配额">
          <el-input-number v-model="editForm.pointQuota" :min="1" :max="100" />
        </el-form-item>
        <el-form-item label="SaaS订阅到期">
          <el-date-picker
            v-model="editForm.expireTime"
            type="datetime"
            placeholder="选择授权到期时间"
            value-format="YYYY-MM-DD HH:mm:ss"
            style="width: 100%;"
          />
        </el-form-item>
        <el-form-item label="重置密码">
          <el-input v-model="editForm.password" placeholder="留空表示不修改原密码" />
        </el-form-item>
        <el-form-item label="账号状态">
          <el-radio-group v-model="editForm.status">
            <el-radio :label="1">正常开通</el-radio>
            <el-radio :label="0">停用禁用</el-radio>
          </el-radio-group>
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="editVisible = false">取消</el-button>
        <el-button type="primary" :loading="submitLoading" @click="submitEdit">保存修改</el-button>
      </template>
    </el-dialog>

    <!-- 商户专属数字大屏投屏直达配置对话框 -->
    <el-dialog v-model="screenDialogVisible" title="🖥️ 商户专属数字大屏投屏配置 (多租户隔离)" width="600px">
      <div v-loading="screenLoading">
        <el-alert
          type="success"
          :closable="false"
          show-icon
          title="多租户数据严格隔离保障"
          description="该专属链接附带该商户专属的长效免密投屏凭据（180天有效），大屏仅展示该商户名下的通道画面、客流/车流指标与最新抓拍流水，绝不跨商户串数据。"
          style="margin-bottom: 16px;"
        />
        <el-form label-width="120px" size="default">
          <el-form-item label="商户企业全称:">
            <span style="font-weight: 700; color: #303133; font-size: 15px;">{{ currentScreenInfo.companyName }}</span>
          </el-form-item>
          <el-form-item label="商户客户ID:">
            <el-tag type="info">ID: {{ currentScreenInfo.userId }}</el-tag>
          </el-form-item>
          <el-form-item label="大屏直达地址:">
            <el-input v-model="currentScreenInfo.fullScreenUrl" readonly type="textarea" :rows="3" />
          </el-form-item>
        </el-form>
      </div>
      <template #footer>
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <el-button @click="copyScreenUrl" type="warning" plain>📋 复制大屏直达链接</el-button>
          <div>
            <el-button @click="screenDialogVisible = false">关闭</el-button>
            <el-button type="primary" @click="openScreenInNewTab">🚀 立即在新窗口打开大屏</el-button>
          </div>
        </div>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue';
import { useRouter } from 'vue-router';
import { ElMessage, ElMessageBox } from 'element-plus';
import { adminApi } from '../api';

const router = useRouter();
const loading = ref(false);
const submitLoading = ref(false);
const keyword = ref('');
const page = ref(1);
const size = ref(10);
const total = ref(0);
const tableData = ref<any[]>([]);

const createVisible = ref(false);
const screenDialogVisible = ref(false);
const screenLoading = ref(false);
const currentScreenInfo = ref<any>({
  userId: 0,
  companyName: '',
  fullScreenUrl: ''
});

const handleOpenScreen = async (row: any) => {
  screenLoading.value = true;
  screenDialogVisible.value = true;
  try {
    const res: any = await adminApi.getMerchantScreenUrl(row.id);
    const host = window.location.origin + window.location.pathname;
    const fullUrl = host + res.screenUrl;
    currentScreenInfo.value = {
      userId: res.userId,
      companyName: res.companyName || row.companyName || row.phone,
      fullScreenUrl: fullUrl
    };
  } catch (e) {
    ElMessage.error('获取大屏投屏凭证失败');
  } finally {
    screenLoading.value = false;
  }
};

const copyScreenUrl = () => {
  navigator.clipboard.writeText(currentScreenInfo.value.fullScreenUrl).then(() => {
    ElMessage.success('商户大屏专属直达链接已成功复制到剪贴板！');
  }).catch(() => {
    ElMessage.error('复制失败，请手动全选复制');
  });
};

const openScreenInNewTab = () => {
  window.open(currentScreenInfo.value.fullScreenUrl, '_blank');
};

const createFormRef = ref();
const createForm = reactive({
  phone: '',
  password: '',
  companyName: '',
  nickname: '',
  contactPerson: '',
  pointQuota: 5,
  expireTime: '',
  initialPointName: '正门主通道'
});

const createRules = {
  phone: [{ required: true, message: '请输入11位手机号', trigger: 'blur' }],
  companyName: [{ required: true, message: '请输入企业客户全称', trigger: 'blur' }]
};

const editVisible = ref(false);
const currentEditId = ref<number | null>(null);
const editForm = reactive({
  companyName: '',
  nickname: '',
  contactPerson: '',
  pointQuota: 5,
  expireTime: '',
  password: '',
  status: 1
});

const formatTime = (timeStr: string) => {
  if (!timeStr) return '';
  return timeStr.replace('T', ' ').substring(0, 19);
};

const isExpired = (timeStr: string) => {
  if (!timeStr) return false;
  return new Date(timeStr).getTime() < Date.now();
};

const fetchUsers = async () => {
  loading.value = true;
  try {
    const res: any = await adminApi.getUsers(page.value, size.value, keyword.value);
    tableData.value = res.records || [];
    total.value = res.total || 0;
  } catch (e) {
    //
  } finally {
    loading.value = false;
  }
};

const jumpToPoints = (row: any) => {
  router.push({
    path: '/points',
    query: { userId: row.id, phone: row.phone }
  });
};

const openCreateDialog = () => {
  createForm.phone = '';
  createForm.password = '';
  createForm.companyName = '';
  createForm.nickname = '';
  createForm.contactPerson = '';
  createForm.pointQuota = 5;
  // 默认一年后到期
  const nextYear = new Date();
  nextYear.setFullYear(nextYear.getFullYear() + 1);
  createForm.expireTime = nextYear.toISOString().slice(0, 19).replace('T', ' ');
  createForm.initialPointName = '正门主通道';
  createVisible.value = true;
};

const submitCreate = async () => {
  if (!createFormRef.value) return;
  await createFormRef.value.validate(async (valid: boolean) => {
    if (!valid) return;
    submitLoading.value = true;
    try {
      await adminApi.createUser(createForm);
      ElMessage.success('商户客户账号开通成功');
      createVisible.value = false;
      fetchUsers();
    } catch (e) {
      //
    } finally {
      submitLoading.value = false;
    }
  });
};

const openEditDialog = (row: any) => {
  currentEditId.value = row.id;
  editForm.companyName = row.companyName || '';
  editForm.nickname = row.nickname || '';
  editForm.contactPerson = row.contactPerson || '';
  editForm.pointQuota = row.pointQuota || 5;
  editForm.expireTime = row.expireTime ? row.expireTime.replace('T', ' ').substring(0, 19) : '';
  editForm.password = '';
  editForm.status = row.status;
  editVisible.value = true;
};

const submitEdit = async () => {
  if (!currentEditId.value) return;
  submitLoading.value = true;
  try {
    await adminApi.updateUser(currentEditId.value, editForm);
    ElMessage.success('商户客户信息已更新');
    editVisible.value = false;
    fetchUsers();
  } catch (e) {
    //
  } finally {
    submitLoading.value = false;
  }
};

const handleDelete = (row: any) => {
  ElMessageBox.confirm(
    `确定删除商户【${row.companyName || row.nickname || row.phone}】吗？删除后将级联清空其名下所有点位及云端切片视频（本地客户端数据不删，手机端彻底无法再看）。`,
    '高危警告: 云端级联删除',
    {
      confirmButtonText: '确定删除',
      cancelButtonText: '取消',
      type: 'warning'
    }
  ).then(async () => {
    try {
      await adminApi.deleteUser(row.id);
      ElMessage.success('用户及云端关联数据已成功销毁');
      fetchUsers();
    } catch (e) {
      //
    }
  });
};

onMounted(() => {
  fetchUsers();
});
</script>

<style scoped>
.toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.pagination-bar {
  display: flex;
  justify-content: flex-end;
  margin-top: 16px;
}
</style>
