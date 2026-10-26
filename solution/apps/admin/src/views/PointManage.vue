<template>
  <div class="point-manage-container">
    <el-card shadow="never">
      <div class="filter-header" v-if="filteredPhone">
        <el-alert
          :title="`正在查看商户客户【${filteredPhone}】名下的点位列表`"
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
            placeholder="搜索点位名称 / 物理地址 / 点位编码"
            style="width: 280px;"
            clearable
            @clear="fetchPoints"
            @keyup.enter="fetchPoints"
          >
            <template #append>
              <el-button icon="Search" @click="fetchPoints" />
            </template>
          </el-input>

          <el-select
            v-model="filterBizType"
            placeholder="业务感知模式"
            style="width: 150px; margin-left: 12px;"
            clearable
            @change="fetchPoints"
          >
            <el-option label="全部感知模式" value="" />
            <el-option label="🚶 客流感知" value="PEDESTRIAN" />
            <el-option label="🚗 车流感知" value="VEHICLE" />
          </el-select>
        </div>

        <el-button type="primary" icon="Plus" @click="openCreateDialog">
          新增分析点位
        </el-button>
      </div>

      <el-table :data="tableData" v-loading="loading" stripe style="width: 100%; margin-top: 16px;">
        <el-table-column prop="id" label="点位ID" width="80" />
        <el-table-column prop="pointName" label="点位名称" width="160">
          <template #default="{ row }">
            <span style="font-weight: 600; color: #303133;">{{ row.pointName }}</span>
          </template>
        </el-table-column>
        <el-table-column prop="pointCode" label="点位编码 (PointCode)" width="180">
          <template #default="{ row }">
            <el-tag type="warning" size="small" style="font-family: monospace; font-weight: bold;">
              {{ row.pointCode }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column prop="bizType" label="感知业务模式" width="130" align="center">
          <template #default="{ row }">
            <el-tag
              :type="row.bizType === 'VEHICLE' ? 'warning' : 'success'"
              size="small"
              style="font-weight: bold; border-radius: 4px;"
            >
              {{ row.bizType === 'VEHICLE' ? '🚗 车流感知' : '🚶 客流感知' }}
            </el-tag>
          </template>
        </el-table-column>
        <el-table-column label="归属客户 / 商户" width="180">
          <template #default="{ row }">
            <div style="font-weight: 600; color: #1989fa;">
              {{ row.companyName || row.userNickname || '商户客户' }}
            </div>
            <div style="font-size: 12px; color: #909399;">
              {{ row.userPhone }}
            </div>
          </template>
        </el-table-column>
        <el-table-column prop="address" label="物理安装地址" min-width="160" />
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
    <el-dialog v-model="createVisible" title="新增监控点位 (将受商户配额约束)" width="540px">
      <el-form :model="createForm" :rules="createRules" ref="createFormRef" label-width="120px">
        <el-form-item label="归属商户客户" prop="userId">
          <el-select v-model="createForm.userId" placeholder="选择归属商户企业客户" style="width: 100%;">
            <el-option
              v-for="u in userOptions"
              :key="u.id"
              :label="`${u.companyName ? u.companyName + ' - ' : ''}${u.phone} (${u.nickname || '无简称'}) [已用: ${u.pointCount}/${u.pointQuota || 5}]`"
              :value="u.id"
            />
          </el-select>
        </el-form-item>
        <el-form-item label="点位名称" prop="pointName">
          <el-input v-model="createForm.pointName" placeholder="例如: 正门主通道 / 收银区" />
        </el-form-item>
        <el-form-item label="业务感知模式" prop="bizType">
          <el-radio-group v-model="createForm.bizType" @change="handleBizTypeChange">
            <el-radio-button label="PEDESTRIAN">🚶 客流分析 (人数与性别年龄)</el-radio-button>
            <el-radio-button label="VEHICLE">🚗 车流分析 (机动车与车牌车型)</el-radio-button>
          </el-radio-group>
          <div style="font-size: 12px; color: #909399; margin-top: 4px;">
            {{ createForm.bizType === 'VEHICLE' ? '将调度边缘端 YOLO 机动车检测与 LPR 车牌识别算法' : '将调度边缘端 YOLO 人体检测与性别年龄推断算法' }}
          </div>
        </el-form-item>
        <el-form-item label="点位编码" prop="pointCode">
          <el-input v-model="createForm.pointCode" placeholder="系统自动分配全局唯一编码，也可手动指定">
            <template #append>
              <el-button @click="refreshSuggestedCode()">
                <el-icon><Refresh /></el-icon> 换一个
              </el-button>
            </template>
          </el-input>
          <div style="font-size: 12px; color: #909399; margin-top: 4px;">
            系统自动分配全局唯一编码（例如 PT_PED_20261026_XXXX），杜绝重复；也可手动输入自定义编码
          </div>
        </el-form-item>
        <el-form-item label="设备密码(密码2)">
          <el-input v-model="createForm.devicePassword" placeholder="边缘设备接入密码 (留空默认: Edge@123456)" />
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
        <el-form-item label="业务感知模式">
          <el-radio-group v-model="editForm.bizType">
            <el-radio-button label="PEDESTRIAN">🚶 客流分析</el-radio-button>
            <el-radio-button label="VEHICLE">🚗 车流分析</el-radio-button>
          </el-radio-group>
          <div style="font-size: 12px; color: #909399; margin-top: 4px;">
            修改感知模式后，现场绑定的 FlowSight.exe 客户端在同步配置时将自动无感热切换算法模型
          </div>
        </el-form-item>
        <el-form-item label="点位编码">
          <el-input v-model="editForm.pointCode" placeholder="例如: PT_PED_20261026_01">
            <template #append>
              <el-button @click="refreshEditSuggestedCode()">
                <el-icon><Refresh /></el-icon> 换一个
              </el-button>
            </template>
          </el-input>
          <div style="font-size: 12px; color: #909399; margin-top: 4px;">
            点位编码为全系统全局唯一凭据，后端会自动拦截重名编码
          </div>
        </el-form-item>
        <el-form-item label="重置设备密码">
          <el-input v-model="editForm.devicePassword" placeholder="留空则不修改当前设备密码" />
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
const filterBizType = ref('');
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
  pointCode: '',
  bizType: 'PEDESTRIAN',
  devicePassword: '',
  address: '',
  cameraRtsp: '',
  resolution: '1920x1080'
});

const createRules = {
  userId: [{ required: true, message: '请选择归属商户客户', trigger: 'change' }],
  pointName: [{ required: true, message: '请输入点位名称', trigger: 'blur' }]
};

const editVisible = ref(false);
const currentEditId = ref<number | null>(null);
const editForm = reactive({
  pointName: '',
  pointCode: '',
  bizType: 'PEDESTRIAN',
  devicePassword: '',
  address: '',
  cameraRtsp: '',
  resolution: ''
});

const fetchPoints = async () => {
  loading.value = true;
  try {
    const res: any = await adminApi.getPoints(page.value, size.value, filteredUserId.value, keyword.value, filterBizType.value);
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

const refreshSuggestedCode = async (bizType?: string) => {
  try {
    const type = bizType || createForm.bizType || 'PEDESTRIAN';
    const res: any = await adminApi.generatePointCode(type);
    if (res.code === 200 && res.data) {
      createForm.pointCode = res.data;
    }
  } catch (e) {
    console.error('获取推荐编码失败', e);
  }
};

const handleBizTypeChange = (val: any) => {
  createForm.bizType = val;
  // 若当前编码为空或为系统生成的默认格式，则自动匹配生成新模式前缀编码
  if (!createForm.pointCode || createForm.pointCode.startsWith('PT_')) {
    refreshSuggestedCode(val);
  }
};

const refreshEditSuggestedCode = async () => {
  try {
    const res: any = await adminApi.generatePointCode(editForm.bizType || 'PEDESTRIAN');
    if (res.code === 200 && res.data) {
      editForm.pointCode = res.data;
    }
  } catch (e) {
    console.error('获取推荐编码失败', e);
  }
};

const openCreateDialog = async () => {
  createForm.userId = filteredUserId.value;
  createForm.pointName = '';
  createForm.pointCode = '';
  createForm.bizType = 'PEDESTRIAN';
  createForm.devicePassword = '';
  createForm.address = '';
  createForm.cameraRtsp = '';
  createForm.resolution = '1920x1080';
  loadUserOptions();
  createVisible.value = true;
  await refreshSuggestedCode('PEDESTRIAN');
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
  editForm.pointCode = row.pointCode || '';
  editForm.bizType = row.bizType || 'PEDESTRIAN';
  editForm.devicePassword = '';
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
