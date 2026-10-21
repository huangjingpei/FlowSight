<template>
  <div class="point-manage-container">
    <el-card shadow="never">
      <div class="filter-header" v-if="filteredPhone">
        <el-alert
          :title="`正在查看商户【${filteredPhone}】名下的点位列表`"
          type="info"
          show-icon
          closable
          @close="clearUserFilter"
        />
      </div>

      <div class="toolbar">
        <div class="toolbar-left">
          <el-input
            v-model="keyword"
            placeholder="搜索点位名称/地址/编码"
            style="width: 260px;"
            clearable
            @clear="fetchPoints"
            @keyup.enter="fetchPoints"
          >
            <template #append>
              <el-button icon="Search" @click="fetchPoints" />
            </template>
          </el-input>
        </div>

        <el-button type="primary" icon="Plus" @click="openCreateDialog">
          新增分析点位
        </el-button>
      </div>

      <el-table :data="tableData" v-loading="loading" stripe style="width: 100%; margin-top: 16px;">
        <el-table-column prop="id" label="点位ID" width="80" />
        <el-table-column prop="pointName" label="点位名称" width="160">
          <template #default="{ row }">
            <span style="font-weight: 600;">{{ row.pointName }}</span>
          </template>
        </el-table-column>
        <el-table-column prop="userPhone" label="归属商户手机号" width="150" />
        <el-table-column prop="address" label="物理地址" min-width="160" />
        <el-table-column prop="cameraRtsp" label="摄像头 RTSP 地址" min-width="220" show-overflow-tooltip />
        <el-table-column prop="resolution" label="分辨率" width="120" />
        <el-table-column prop="streamStatus" label="实时状态" width="100">
          <template #default="{ row }">
            <el-tag :type="row.streamStatus === 'ONLINE' ? 'success' : 'info'" size="small">
              {{ row.streamStatus === 'ONLINE' ? '在线' : '离线' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="pointToken" label="边缘采集Token" width="160">
          <template #default="{ row }">
            <el-button size="small" link type="primary" @click="copyToken(row.pointToken)">
              复制Token
            </el-button>
            <el-button size="small" link type="warning" @click="handleRefreshToken(row)">
              重置
            </el-button>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="150" fixed="right">
          <template #default="{ row }">
            <el-button size="small" link type="primary" @click="openEditDialog(row)">
              编辑
            </el-button>
            <el-button size="small" link type="danger" @click="handleDelete(row)">
              删除点位
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
          @current-change="fetchPoints"
        />
      </div>
    </el-card>

    <!-- 新增点位对话框 -->
    <el-dialog v-model="createVisible" title="新增监控点位" width="520px">
      <el-form :model="createForm" :rules="createRules" ref="createFormRef" label-width="110px">
        <el-form-item label="归属商户" prop="userId">
          <el-select v-model="createForm.userId" placeholder="选择归属商户" style="width: 100%;">
            <el-option
              v-for="u in userOptions"
              :key="u.id"
              :label="`${u.phone} (${u.nickname})`"
              :value="u.id"
            />
          </el-select>
        </el-form-item>
        <el-form-item label="点位名称" prop="pointName">
          <el-input v-model="createForm.pointName" placeholder="例如: 正门主通道 / 收银区" />
        </el-form-item>
        <el-form-item label="物理地址">
          <el-input v-model="createForm.address" placeholder="例如: 1号楼大厅西侧门" />
        </el-form-item>
        <el-form-item label="摄像头 RTSP">
          <el-input v-model="createForm.cameraRtsp" placeholder="rtsp://admin:123456@192.168.1.64:554/h264" />
        </el-form-item>
        <el-form-item label="视频分辨率">
          <el-select v-model="createForm.resolution" style="width: 100%;">
            <el-option label="1920x1080 (1080P 高清)" value="1920x1080" />
            <el-option label="1280x720 (720P 标清)" value="1280x720" />
            <el-option label="2560x1440 (2K 超清)" value="2560x1440" />
            <el-option label="3840x2160 (4K 极清)" value="3840x2160" />
          </el-select>
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="createVisible = false">取消</el-button>
        <el-button type="primary" :loading="submitLoading" @click="submitCreate">确认创建</el-button>
      </template>
    </el-dialog>

    <!-- 编辑点位对话框 -->
    <el-dialog v-model="editVisible" title="编辑点位信息" width="520px">
      <el-form :model="editForm" label-width="110px">
        <el-form-item label="点位名称">
          <el-input v-model="editForm.pointName" />
        </el-form-item>
        <el-form-item label="物理地址">
          <el-input v-model="editForm.address" />
        </el-form-item>
        <el-form-item label="摄像头 RTSP">
          <el-input v-model="editForm.cameraRtsp" />
        </el-form-item>
        <el-form-item label="视频分辨率">
          <el-select v-model="editForm.resolution" style="width: 100%;">
            <el-option label="1920x1080 (1080P)" value="1920x1080" />
            <el-option label="1280x720 (720P)" value="1280x720" />
            <el-option label="2560x1440 (2K)" value="2560x1440" />
          </el-select>
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="editVisible = false">取消</el-button>
        <el-button type="primary" :loading="submitLoading" @click="submitEdit">保存</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { ElMessage, ElMessageBox } from 'element-plus';
import { adminApi } from '../api';

const route = useRoute();
const router = useRouter();

const loading = ref(false);
const submitLoading = ref(false);
const keyword = ref('');
const page = ref(1);
const size = ref(10);
const total = ref(0);
const tableData = ref<any[]>([]);
const userOptions = ref<any[]>([]);

const filteredUserId = ref<number | undefined>(route.query.userId ? Number(route.query.userId) : undefined);
const filteredPhone = ref<string | undefined>(route.query.phone as string);

const createVisible = ref(false);
const createFormRef = ref();
const createForm = reactive({
  userId: undefined as number | undefined,
  pointName: '',
  address: '',
  cameraRtsp: '',
  resolution: '1920x1080'
});

const createRules = {
  userId: [{ required: true, message: '请选择归属商户', trigger: 'change' }],
  pointName: [{ required: true, message: '请输入点位名称', trigger: 'blur' }]
};

const editVisible = ref(false);
const currentEditId = ref<number | null>(null);
const editForm = reactive({
  pointName: '',
  address: '',
  cameraRtsp: '',
  resolution: ''
});

const fetchPoints = async () => {
  loading.value = true;
  try {
    const res: any = await adminApi.getPoints(page.value, size.value, filteredUserId.value, keyword.value);
    tableData.value = res.records || [];
    total.value = res.total || 0;
  } catch (e) {
    //
  } finally {
    loading.value = false;
  }
};

const loadUserOptions = async () => {
  try {
    const res: any = await adminApi.getUsers(1, 100);
    userOptions.value = res.records || [];
  } catch (e) {
    //
  }
};

const clearUserFilter = () => {
  filteredUserId.value = undefined;
  filteredPhone.value = undefined;
  router.replace({ path: '/points' });
  fetchPoints();
};

const openCreateDialog = () => {
  createForm.userId = filteredUserId.value;
  createForm.pointName = '';
  createForm.address = '';
  createForm.cameraRtsp = '';
  createForm.resolution = '1920x1080';
  loadUserOptions();
  createVisible.value = true;
};

const submitCreate = async () => {
  if (!createFormRef.value) return;
  await createFormRef.value.validate(async (valid: boolean) => {
    if (!valid) return;
    submitLoading.value = true;
    try {
      await adminApi.createPoint(createForm);
      ElMessage.success('点位创建成功，已自动生成对接 Token');
      createVisible.value = false;
      fetchPoints();
    } catch (e) {
      //
    } finally {
      submitLoading.value = false;
    }
  });
};

const openEditDialog = (row: any) => {
  currentEditId.value = row.id;
  editForm.pointName = row.pointName;
  editForm.address = row.address;
  editForm.cameraRtsp = row.cameraRtsp;
  editForm.resolution = row.resolution;
  editVisible.value = true;
};

const submitEdit = async () => {
  if (!currentEditId.value) return;
  submitLoading.value = true;
  try {
    await adminApi.updatePoint(currentEditId.value, editForm);
    ElMessage.success('点位配置更新成功');
    editVisible.value = false;
    fetchPoints();
  } catch (e) {
    //
  } finally {
    submitLoading.value = false;
  }
};

const handleDelete = (row: any) => {
  ElMessageBox.confirm(
    `确定删除点位【${row.pointName}】吗？删除后将清理云端流水及 MinIO 切片池文件（现场客户端本地数据不受影响，手机端无法再查看）。`,
    '警告: 云端删除点位',
    {
      confirmButtonText: '确定删除',
      cancelButtonText: '取消',
      type: 'warning'
    }
  ).then(async () => {
    try {
      await adminApi.deletePoint(row.id);
      ElMessage.success('点位及云端切片已清除');
      fetchPoints();
    } catch (e) {
      //
    }
  });
};

const copyToken = (token: string) => {
  navigator.clipboard.writeText(token);
  ElMessage.success('Token 已成功复制到剪贴板，可填入边缘客户端配置');
};

const handleRefreshToken = (row: any) => {
  ElMessageBox.confirm('重置后旧 Token 将立即失效，需在边缘采集端同步更新，确认重置吗？', '提示').then(async () => {
    try {
      const res: any = await adminApi.refreshToken(row.id);
      row.pointToken = res;
      ElMessage.success('Token 重置成功: ' + res);
    } catch (e) {
      //
    }
  });
};

onMounted(() => {
  fetchPoints();
  loadUserOptions();
});
</script>

<style scoped>
.filter-header {
  margin-bottom: 16px;
}
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
