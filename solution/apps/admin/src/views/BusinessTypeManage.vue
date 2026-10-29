<template>
  <div class="business-type-manage-container">
    <el-card shadow="never" class="table-card">
      <template #header>
        <div class="card-header">
          <div class="header-title">
            <span class="title-text">业务模型与感知类型配置</span>
            <span class="subtitle-text">管理多租户 SaaS 平台支持的边缘 AI 感知业务要求与特征属性</span>
          </div>
          <div class="header-actions">
            <el-button type="primary" :icon="Plus" @click="openCreateDialog">
              新增业务类型
            </el-button>
            <el-button :icon="Refresh" @click="fetchBusinessTypes">
              刷新
            </el-button>
          </div>
        </div>
      </template>

      <!-- 架构协同提示条 -->
      <div class="guide-banner">
        <el-alert
          type="info"
          :closable="false"
          show-icon
        >
          <template #title>
            <span style="font-weight: bold;">业务类型与边缘采集协同规范：</span>
          </template>
          <div style="font-size: 13px; line-height: 1.6; margin-top: 4px;">
            每一个业务模式对应现场边缘端（客流智能统计系统客户端）的算法模型与数据流水协议（如<b>客流检测分析、车流与车牌识别</b>）。
            在为商户分配并创建点位之前，如果需要全新的感知业务，必须在此完成业务模型定义，且前提是前端采集端已经具备相应的视觉分析与流水上报实现。
          </div>
        </el-alert>
      </div>

      <!-- 检索工具栏 -->
      <div class="filter-bar">
        <el-input
          v-model="keyword"
          placeholder="搜索业务编码 / 业务名称 / 场景要求描述..."
          clearable
          style="width: 320px;"
          @keyup.enter="fetchBusinessTypes"
          @clear="fetchBusinessTypes"
        >
          <template #prefix>
            <el-icon><Search /></el-icon>
          </template>
        </el-input>
        <el-button type="primary" plain @click="fetchBusinessTypes">查询</el-button>
      </div>

      <!-- 数据表格 -->
      <el-table
        v-loading="loading"
        :data="businessTypes"
        border
        stripe
        style="width: 100%; margin-top: 16px;"
      >
        <el-table-column prop="bizCode" label="业务模式编码 (biz_type)" width="180">
          <template #default="{ row }">
            <div style="display: flex; align-items: center; gap: 6px;">
              <el-tag
                size="small"
                effect="plain"
                style="font-family: Consolas, monospace; font-weight: bold; letter-spacing: 0.5px;"
              >
                {{ row.bizCode }}
              </el-tag>
              <el-button
                size="small"
                link
                type="primary"
                title="复制编码"
                @click="copyCode(row.bizCode)"
              >
                <el-icon><CopyDocument /></el-icon>
              </el-button>
            </div>
          </template>
        </el-table-column>

        <el-table-column label="业务模式名称" width="200">
          <template #default="{ row }">
            <div style="display: flex; align-items: center; gap: 8px;">
              <span
                class="biz-dot"
                :style="{ backgroundColor: row.colorTag || '#388bfd' }"
              />
              <el-icon :size="16" :style="{ color: row.colorTag || '#388bfd' }">
                <component :is="getIconComponent(row.icon)" />
              </el-icon>
              <span style="font-weight: 600; color: #24292f;">{{ row.bizName }}</span>
            </div>
          </template>
        </el-table-column>

        <el-table-column prop="description" label="场景定位与业务要求描述" min-width="240" show-overflow-tooltip />

        <el-table-column label="支持的 AI 特征属性" min-width="260">
          <template #default="{ row }">
            <div class="features-cell">
              <el-tag
                v-for="feat in (row.featureList || [])"
                :key="feat"
                size="small"
                effect="light"
                class="feature-tag"
              >
                {{ feat }}
              </el-tag>
              <span v-if="!row.featureList || row.featureList.length === 0" style="color: #909399; font-size: 12px;">
                未指定属性
              </span>
            </div>
          </template>
        </el-table-column>

        <el-table-column prop="deviceReq" label="采集端要求建议" min-width="200" show-overflow-tooltip>
          <template #default="{ row }">
            <span style="color: #57606a; font-size: 13px;">{{ row.deviceReq || '无特定限制' }}</span>
          </template>
        </el-table-column>

        <el-table-column label="关联在用点位" width="130" align="center">
          <template #default="{ row }">
            <el-tag
              :type="row.pointCount > 0 ? 'success' : 'info'"
              size="small"
              style="font-weight: bold;"
            >
              {{ row.pointCount }} 个点位
            </el-tag>
          </template>
        </el-table-column>

        <el-table-column label="状态" width="90" align="center">
          <template #default="{ row }">
            <el-switch
              v-model="row.status"
              :active-value="1"
              :inactive-value="0"
              @change="handleStatusChange(row)"
            />
          </template>
        </el-table-column>

        <el-table-column label="类型" width="100" align="center">
          <template #default="{ row }">
            <el-tag :type="row.isSystem === 1 ? 'info' : 'warning'" size="small">
              {{ row.isSystem === 1 ? '系统内置' : '自定义' }}
            </el-tag>
          </template>
        </el-table-column>

        <el-table-column label="操作" width="150" fixed="right" align="center">
          <template #default="{ row }">
            <el-button size="small" link type="primary" @click="openEditDialog(row)">
              编辑
            </el-button>
            <el-tooltip
              v-if="row.isSystem === 1"
              content="系统内置原生核心业务禁止删除"
              placement="top"
            >
              <span>
                <el-button size="small" link type="danger" disabled>删除</el-button>
              </span>
            </el-tooltip>
            <el-tooltip
              v-else-if="row.pointCount > 0"
              :content="`尚有 ${row.pointCount} 个点位正在使用该业务，禁止直接删除`"
              placement="top"
            >
              <span>
                <el-button size="small" link type="danger" disabled>删除</el-button>
              </span>
            </el-tooltip>
            <el-button
              v-else
              size="small"
              link
              type="danger"
              @click="handleDelete(row)"
            >
              删除
            </el-button>
          </template>
        </el-table-column>
      </el-table>

      <!-- 分页栏 -->
      <div class="pagination-bar">
        <el-pagination
          v-model:current-page="page"
          v-model:page-size="size"
          :total="total"
          layout="total, prev, pager, next"
          @current-change="fetchBusinessTypes"
        />
      </div>
    </el-card>

    <!-- 新增 / 编辑业务类型弹窗 -->
    <el-dialog
      v-model="dialogVisible"
      :title="isEdit ? '编辑感知业务类型' : '新建感知业务类型'"
      width="640px"
      :close-on-click-modal="false"
    >
      <el-form
        ref="formRef"
        :model="form"
        :rules="rules"
        label-width="120px"
      >
        <el-form-item label="业务编码" prop="bizCode">
          <el-input
            v-model="form.bizCode"
            placeholder="例如: PEDESTRIAN / VEHICLE / FIRE_SMOKE"
            :disabled="isEdit && form.isSystem === 1"
            @input="val => form.bizCode = (val || '').toUpperCase()"
          />
          <div class="form-tip">
            全系统唯一大写英文代号，与边缘采集端算法协议及点位 <code>biz_type</code> 字段严格对齐
          </div>
        </el-form-item>

        <el-form-item label="业务模式名称" prop="bizName">
          <el-input
            v-model="form.bizName"
            placeholder="例如: 客流检测与客群分析 / 车流检测与车辆分析"
          />
        </el-form-item>

        <el-form-item label="场景与要求描述" prop="description">
          <el-input
            v-model="form.description"
            type="textarea"
            :rows="3"
            placeholder="简要描述该业务适用的场景、检测目标要求以及现场布设标准..."
          />
        </el-form-item>

        <el-form-item label="支持的 AI 特征">
          <div class="features-editor">
            <div class="selected-tags">
              <el-tag
                v-for="(tag, idx) in form.featureList"
                :key="tag"
                closable
                class="edit-tag"
                @close="removeFeatureTag(idx)"
              >
                {{ tag }}
              </el-tag>
              <el-input
                v-if="inputTagVisible"
                ref="tagInputRef"
                v-model="inputTagValue"
                size="small"
                style="width: 110px;"
                placeholder="回车确认"
                @keyup.enter="confirmAddTag"
                @blur="confirmAddTag"
              />
              <el-button
                v-else
                size="small"
                class="button-new-tag"
                @click="showTagInput"
              >
                + 添加特征
              </el-button>
            </div>

            <!-- 常用特征预置候选区 -->
            <div class="preset-section">
              <span class="preset-label">快捷预选候选属性：</span>
              <div class="preset-group">
                <span class="group-title">客流类:</span>
                <span
                  v-for="p in presetPedestrianFeatures"
                  :key="p"
                  class="preset-pill"
                  :class="{ active: form.featureList.includes(p) }"
                  @click="toggleFeature(p)"
                >
                  {{ p }}
                </span>
              </div>
              <div class="preset-group">
                <span class="group-title">车流类:</span>
                <span
                  v-for="p in presetVehicleFeatures"
                  :key="p"
                  class="preset-pill"
                  :class="{ active: form.featureList.includes(p) }"
                  @click="toggleFeature(p)"
                >
                  {{ p }}
                </span>
              </div>
              <div class="preset-group">
                <span class="group-title">安防类:</span>
                <span
                  v-for="p in presetSecurityFeatures"
                  :key="p"
                  class="preset-pill"
                  :class="{ active: form.featureList.includes(p) }"
                  @click="toggleFeature(p)"
                >
                  {{ p }}
                </span>
              </div>
            </div>
          </div>
        </el-form-item>

        <el-form-item label="采集与算力建议">
          <el-input
            v-model="form.deviceReq"
            type="textarea"
            :rows="2"
            placeholder="例如: 推荐1080P/720P，帧率15-25fps，架高2.5m~3.5m下视角..."
          />
        </el-form-item>

        <el-form-item label="UI 图标与色彩">
          <div style="display: flex; align-items: center; gap: 16px;">
            <el-select v-model="form.icon" style="width: 140px;">
              <el-option label="👤 用户客流 (User)" value="User" />
              <el-option label="🚗 车辆交通 (Van)" value="Van" />
              <el-option label="📹 摄像监控 (VideoCamera)" value="VideoCamera" />
              <el-option label="📷 快照视觉 (Camera)" value="Camera" />
              <el-option label="🖥️ 工控终端 (Monitor)" value="Monitor" />
              <el-option label="⚙️ 核心业务 (Operation)" value="Operation" />
            </el-select>

            <div class="color-picker-box">
              <span
                v-for="c in presetColors"
                :key="c.color"
                class="color-pill"
                :style="{ backgroundColor: c.color }"
                :class="{ selected: form.colorTag === c.color }"
                :title="c.label"
                @click="form.colorTag = c.color"
              />
            </div>
          </div>
        </el-form-item>

        <el-form-item label="显示排序">
          <el-input-number v-model="form.sortOrder" :min="0" :max="999" />
          <span style="font-size: 12px; color: #909399; margin-left: 10px;">数字越小越优先显示</span>
        </el-form-item>

        <el-form-item label="启用状态">
          <el-switch
            v-model="form.status"
            :active-value="1"
            :inactive-value="0"
            active-text="启用中"
            inactive-text="已停用"
          />
        </el-form-item>
      </el-form>

      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button type="primary" :loading="submitLoading" @click="submitForm">
          确认保存
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, nextTick, onMounted } from 'vue';
import { ElMessage, ElMessageBox, FormInstance } from 'element-plus';
import { Plus, Refresh, Search, CopyDocument } from '@element-plus/icons-vue';
import { adminApi } from '../api';

const loading = ref(false);
const submitLoading = ref(false);
const keyword = ref('');
const page = ref(1);
const size = ref(10);
const total = ref(0);
const businessTypes = ref<any[]>([]);

const dialogVisible = ref(false);
const isEdit = ref(false);
const editId = ref<number | null>(null);
const formRef = ref<FormInstance>();

const inputTagVisible = ref(false);
const inputTagValue = ref('');
const tagInputRef = ref();

const presetPedestrianFeatures = [
  '人体检测', '人脸检测', '轨迹跟踪', '进出防区计数', '性别分析', '年龄段识别', '客流密度', '打伞背包'
];
const presetVehicleFeatures = [
  '车辆检测', '车辆跟踪', '车牌识别', '号牌颜色识别', '车型分类', '车身颜色分析', '车位占用'
];
const presetSecurityFeatures = [
  '安全帽识别', '反光背心检测', '区域入侵警戒', '明火烟雾', '摔倒报警', '离岗抽烟'
];

const presetColors = [
  { color: '#388bfd', label: '科技蓝 (客流推荐)' },
  { color: '#2ea043', label: '生态绿' },
  { color: '#e3b341', label: '金琥珀 (车流推荐)' },
  { color: '#f85149', label: '警示红' },
  { color: '#8957e5', label: '智能紫' }
];

const form = reactive({
  bizCode: '',
  bizName: '',
  description: '',
  featureList: [] as string[],
  deviceReq: '',
  icon: 'User',
  colorTag: '#388bfd',
  status: 1,
  sortOrder: 0,
  isSystem: 0
});

const rules = {
  bizCode: [
    { required: true, message: '请输入业务英文标识编码', trigger: 'blur' },
    { pattern: /^[A-Z0-9_]{2,32}$/, message: '必须为2-32位大写字母、数字或下划线', trigger: 'blur' }
  ],
  bizName: [
    { required: true, message: '请输入业务模式名称', trigger: 'blur' },
    { min: 2, max: 32, message: '长度在 2 到 32 个字符', trigger: 'blur' }
  ]
};

const getIconComponent = (iconName: string) => {
  return iconName || 'Operation';
};

const copyCode = (code: string) => {
  if (navigator.clipboard) {
    navigator.clipboard.writeText(code);
    ElMessage.success(`已复制业务编码: ${code}`);
  }
};

const fetchBusinessTypes = async () => {
  loading.value = true;
  try {
    const res: any = await adminApi.getBusinessTypes(page.value, size.value, keyword.value);
    businessTypes.value = res.records || [];
    total.value = res.total || 0;
  } catch (error) {
    // 错误处理由全局 axios 拦截器提示
  } finally {
    loading.value = false;
  }
};

const openCreateDialog = () => {
  isEdit.value = false;
  editId.value = null;
  Object.assign(form, {
    bizCode: '',
    bizName: '',
    description: '',
    featureList: ['人体检测', '轨迹跟踪', '进出防区计数'],
    deviceReq: '推荐1080P/720P，帧率15-25fps',
    icon: 'User',
    colorTag: '#388bfd',
    status: 1,
    sortOrder: 0,
    isSystem: 0
  });
  dialogVisible.value = true;
  nextTick(() => {
    formRef.value?.clearValidate();
  });
};

const openEditDialog = (row: any) => {
  isEdit.value = true;
  editId.value = row.id;
  Object.assign(form, {
    bizCode: row.bizCode,
    bizName: row.bizName,
    description: row.description || '',
    featureList: [...(row.featureList || [])],
    deviceReq: row.deviceReq || '',
    icon: row.icon || 'User',
    colorTag: row.colorTag || '#388bfd',
    status: row.status ?? 1,
    sortOrder: row.sortOrder ?? 0,
    isSystem: row.isSystem ?? 0
  });
  dialogVisible.value = true;
  nextTick(() => {
    formRef.value?.clearValidate();
  });
};

const toggleFeature = (feat: string) => {
  const idx = form.featureList.indexOf(feat);
  if (idx >= 0) {
    form.featureList.splice(idx, 1);
  } else {
    form.featureList.push(feat);
  }
};

const removeFeatureTag = (index: number) => {
  form.featureList.splice(index, 1);
};

const showTagInput = () => {
  inputTagVisible.value = true;
  nextTick(() => {
    tagInputRef.value?.input?.focus();
  });
};

const confirmAddTag = () => {
  const val = inputTagValue.value.trim();
  if (val && !form.featureList.includes(val)) {
    form.featureList.push(val);
  }
  inputTagVisible.value = false;
  inputTagValue.value = '';
};

const handleStatusChange = async (row: any) => {
  try {
    await adminApi.toggleBusinessTypeStatus(row.id, row.status);
    ElMessage.success(`业务 [${row.bizName}] 状态已${row.status === 1 ? '启用' : '停用'}`);
  } catch (error) {
    // 还原状态
    row.status = row.status === 1 ? 0 : 1;
  }
};

const handleDelete = (row: any) => {
  ElMessageBox.confirm(
    `确认删除业务类型 [${row.bizName}] (${row.bizCode}) 吗？`,
    '删除确认',
    {
      confirmButtonText: '确定删除',
      cancelButtonText: '取消',
      type: 'warning'
    }
  ).then(async () => {
    try {
      await adminApi.deleteBusinessType(row.id);
      ElMessage.success('业务类型已成功删除');
      fetchBusinessTypes();
    } catch (error) {
      // 错误由 axios 拦截器捕获提示
    }
  });
};

const submitForm = async () => {
  if (!formRef.value) return;
  await formRef.value.validate(async (valid) => {
    if (!valid) return;
    submitLoading.value = true;
    try {
      const payload = {
        bizCode: form.bizCode,
        bizName: form.bizName,
        description: form.description,
        featureList: form.featureList,
        deviceReq: form.deviceReq,
        icon: form.icon,
        colorTag: form.colorTag,
        status: form.status,
        sortOrder: form.sortOrder
      };
      if (isEdit.value && editId.value) {
        await adminApi.updateBusinessType(editId.value, payload);
        ElMessage.success('业务类型更新成功');
      } else {
        await adminApi.createBusinessType(payload);
        ElMessage.success('业务类型创建成功');
      }
      dialogVisible.value = false;
      fetchBusinessTypes();
    } catch (error) {
      // 错误提示
    } finally {
      submitLoading.value = false;
    }
  });
};

onMounted(() => {
  fetchBusinessTypes();
});
</script>

<style scoped>
.business-type-manage-container {
  padding: 10px;
}

.table-card {
  border-radius: 8px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-title {
  display: flex;
  flex-direction: column;
}

.title-text {
  font-size: 16px;
  font-weight: 700;
  color: #1f2328;
}

.subtitle-text {
  font-size: 12px;
  color: #656d76;
  margin-top: 4px;
}

.guide-banner {
  margin-bottom: 16px;
}

.filter-bar {
  display: flex;
  align-items: center;
  gap: 12px;
}

.biz-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}

.features-cell {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
}

.feature-tag {
  border-radius: 3px;
  font-size: 11px;
}

.pagination-bar {
  display: flex;
  justify-content: flex-end;
  margin-top: 20px;
}

.form-tip {
  font-size: 12px;
  color: #8c959f;
  margin-top: 4px;
  line-height: 1.4;
}

.form-tip code {
  background: #f6f8fa;
  padding: 2px 5px;
  border-radius: 3px;
  color: #0969da;
}

.features-editor {
  display: flex;
  flex-direction: column;
  gap: 10px;
  width: 100%;
}

.selected-tags {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 6px;
  padding: 6px 10px;
  background-color: #f6f8fa;
  border: 1px dashed #d0d7de;
  border-radius: 6px;
  min-height: 40px;
}

.edit-tag {
  font-size: 12px;
}

.preset-section {
  display: flex;
  flex-direction: column;
  gap: 6px;
  background-color: #ffffff;
  padding: 8px 12px;
  border: 1px solid #e1e4e8;
  border-radius: 6px;
}

.preset-label {
  font-size: 12px;
  font-weight: bold;
  color: #57606a;
}

.preset-group {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 6px;
}

.group-title {
  font-size: 11px;
  color: #8c959f;
  width: 50px;
}

.preset-pill {
  font-size: 11px;
  padding: 2px 8px;
  background-color: #f3f4f6;
  border: 1px solid #e5e7eb;
  border-radius: 12px;
  color: #374151;
  cursor: pointer;
  user-select: none;
  transition: all 0.15s ease;
}

.preset-pill:hover {
  background-color: #e5e7eb;
  color: #111827;
}

.preset-pill.active {
  background-color: #388bfd;
  border-color: #388bfd;
  color: #ffffff;
  font-weight: bold;
}

.color-picker-box {
  display: flex;
  align-items: center;
  gap: 8px;
}

.color-pill {
  width: 22px;
  height: 22px;
  border-radius: 50%;
  cursor: pointer;
  border: 2px solid transparent;
  transition: transform 0.15s ease;
}

.color-pill:hover {
  transform: scale(1.15);
}

.color-pill.selected {
  border-color: #24292f;
  transform: scale(1.15);
  box-shadow: 0 0 0 2px rgba(9, 105, 218, 0.4);
}
</style>
