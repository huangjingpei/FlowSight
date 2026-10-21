<template>
  <div class="user-manage-container">
    <el-card shadow="never">
      <div class="toolbar">
        <el-input
          v-model="keyword"
          placeholder="搜索手机号或商户名称"
          style="width: 260px;"
          clearable
          @clear="fetchUsers"
          @keyup.enter="fetchUsers"
        >
          <template #append>
            <el-button icon="Search" @click="fetchUsers" />
          </template>
        </el-input>

        <el-button type="primary" icon="Plus" @click="openCreateDialog">
          添加商户账号
        </el-button>
      </div>

      <el-table :data="tableData" v-loading="loading" stripe style="width: 100%; margin-top: 16px;">
        <el-table-column prop="id" label="用户ID" width="90" />
        <el-table-column prop="phone" label="手机号码 (登录账号)" width="160">
          <template #default="{ row }">
            <span style="font-weight: 600; color: #1989fa;">{{ row.phone }}</span>
          </template>
        </el-table-column>
        <el-table-column prop="nickname" label="商户名称" min-width="140" />
        <el-table-column prop="pointCount" label="拥有点位数" width="120">
          <template #default="{ row }">
            <el-tag
              style="cursor: pointer;"
              type="info"
              @click="jumpToPoints(row)"
            >
              📹 {{ row.pointCount }} 个点位 (点击查看)
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="status" label="状态" width="100">
          <template #default="{ row }">
            <el-tag :type="row.status === 1 ? 'success' : 'danger'" size="small">
              {{ row.status === 1 ? '正常' : '禁用' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="createTime" label="注册时间" width="180" />
        <el-table-column label="操作" width="220" fixed="right">
          <template #default="{ row }">
            <el-button size="small" link type="primary" @click="jumpToPoints(row)">
              点位管理
            </el-button>
            <el-button size="small" link type="warning" @click="openEditDialog(row)">
              编辑
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

    <!-- 3.1 添加用户对话框 -->
    <el-dialog v-model="createVisible" title="添加商户用户 (支持自动创建初始点位)" width="480px">
      <el-form :model="createForm" :rules="createRules" ref="createFormRef" label-width="100px">
        <el-form-item label="手机号" prop="phone">
          <el-input v-model="createForm.phone" placeholder="11位手机号(作为登录名)" />
        </el-form-item>
        <el-form-item label="登录密码">
          <el-input v-model="createForm.password" placeholder="留空则默认与手机号同号" />
        </el-form-item>
        <el-form-item label="商户名称" prop="nickname">
          <el-input v-model="createForm.nickname" placeholder="例如: 星巴克中关村店" />
        </el-form-item>
        <el-form-item label="初始点位名">
          <el-input v-model="createForm.initialPointName" placeholder="例如: 正门主通道 (可选自动建立)" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="createVisible = false">取消</el-button>
        <el-button type="primary" :loading="submitLoading" @click="submitCreate">确认添加</el-button>
      </template>
    </el-dialog>

    <!-- 3.3 修改用户对话框 -->
    <el-dialog v-model="editVisible" title="修改商户用户" width="450px">
      <el-form :model="editForm" label-width="100px">
        <el-form-item label="商户名称">
          <el-input v-model="editForm.nickname" />
        </el-form-item>
        <el-form-item label="重置密码">
          <el-input v-model="editForm.password" placeholder="留空表示不修改密码" />
        </el-form-item>
        <el-form-item label="账号状态">
          <el-radio-group v-model="editForm.status">
            <el-radio :label="1">启用</el-radio>
            <el-radio :label="0">禁用</el-radio>
          </el-radio-group>
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="editVisible = false">取消</el-button>
        <el-button type="primary" :loading="submitLoading" @click="submitEdit">保存修改</el-button>
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
const createFormRef = ref();
const createForm = reactive({
  phone: '',
  password: '',
  nickname: '',
  initialPointName: 'A门主通道'
});

const createRules = {
  phone: [{ required: true, message: '请输入手机号', trigger: 'blur' }]
};

const editVisible = ref(false);
const currentEditId = ref<number | null>(null);
const editForm = reactive({
  nickname: '',
  password: '',
  status: 1
});

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
  // 通过用户手机账号跳转到对应的点位管理
  router.push({
    path: '/points',
    query: { userId: row.id, phone: row.phone }
  });
};

const openCreateDialog = () => {
  createForm.phone = '';
  createForm.password = '';
  createForm.nickname = '';
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
      ElMessage.success('商户用户及初始点位添加成功');
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
  editForm.nickname = row.nickname;
  editForm.password = '';
  editForm.status = row.status;
  editVisible.value = true;
};

const submitEdit = async () => {
  if (!currentEditId.value) return;
  submitLoading.value = true;
  try {
    await adminApi.updateUser(currentEditId.value, editForm);
    ElMessage.success('用户信息已更新');
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
    `确定删除商户【${row.phone}】吗？删除后将级联清空其名下所有点位及云端切片视频（本地客户端数据不删，手机端彻底无法再看）。`,
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
