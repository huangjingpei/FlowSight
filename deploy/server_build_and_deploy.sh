#!/bin/bash
# ==============================================================================
# FlowSight 边云协同物联网 SaaS 平台 - 源码现场编译构建与自动化部署脚本
# 流程: 编译后端 Jar -> 编译前端 H5 -> 编译前端 Admin -> 发布运行 -> 服务探活
# ==============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

BUILD_DIR="/tmp/flowsight_build"
TARGET_DIR="/opt/flowsight"

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}    FlowSight 现场编译构建与生产交付流水线                     ${NC}"
echo -e "${CYAN}================================================================${NC}"

# 1. 编译后端
echo -e "${GREEN}[1/5] 在服务器现场编译 Java 后端 (Maven)...${NC}"
cd "${BUILD_DIR}/backend"
mvn clean package -DskipTests
JAR_PATH="${BUILD_DIR}/backend/target/flowsight-cloud-backend-1.2.0.jar"
if [ ! -f "${JAR_PATH}" ]; then
    echo -e "${RED}后端编译失败，未找到 Jar 包！${NC}"
    exit 1
fi
echo -e "${GREEN}后端编译完成: ${JAR_PATH}${NC}"

# 2. 编译前端 H5
echo -e "${GREEN}[2/5] 在服务器现场编译移动端 H5 与数字大屏 (Vite)...${NC}"
cd "${BUILD_DIR}/apps/h5"
npm install --registry=https://registry.npmmirror.com
npm run build
if [ ! -d "${BUILD_DIR}/apps/h5/dist" ]; then
    echo -e "${RED}H5 编译失败！${NC}"
    exit 1
fi
echo -e "${GREEN}H5 编译完成: ${BUILD_DIR}/apps/h5/dist${NC}"

# 3. 编译前端 Admin
echo -e "${GREEN}[3/5] 在服务器现场编译 PC 管理运营中台 (Vite)...${NC}"
cd "${BUILD_DIR}/apps/admin"
npm install --registry=https://registry.npmmirror.com
npm run build
if [ ! -d "${BUILD_DIR}/apps/admin/dist" ]; then
    echo -e "${RED}Admin 编译失败！${NC}"
    exit 1
fi
echo -e "${GREEN}Admin 编译完成: ${BUILD_DIR}/apps/admin/dist${NC}"

# 4. 发布与服务热更新
echo -e "${GREEN}[4/5] 发布编译产物至生产运行目录 /opt/flowsight...${NC}"
mkdir -p "${TARGET_DIR}/h5" "${TARGET_DIR}/admin" "${TARGET_DIR}/clips" "${TARGET_DIR}/logs"
cp -f "${JAR_PATH}" "${TARGET_DIR}/flowsight-backend.jar"
cp -f "${BUILD_DIR}/deploy/application-prod.yml" "${TARGET_DIR}/application-prod.yml"

rm -rf "${TARGET_DIR}/h5/"*
cp -rf "${BUILD_DIR}/apps/h5/dist/"* "${TARGET_DIR}/h5/"

rm -rf "${TARGET_DIR}/admin/"*
cp -rf "${BUILD_DIR}/apps/admin/dist/"* "${TARGET_DIR}/admin/"

if [ -d "${BUILD_DIR}/clips" ]; then
    cp -rf "${BUILD_DIR}/clips/"* "${TARGET_DIR}/clips/"
fi

# 数据库结构检查 (严禁执行任何 DROP/TRUNCATE/data_init.sql，确保用户数据资产绝对不被清空)
if [ -f "${BUILD_DIR}/backend/src/main/resources/schema.sql" ] && [ -f "/etc/mysql/debian.cnf" ]; then
    mysql --defaults-file=/etc/mysql/debian.cnf flowsight_cloud < "${BUILD_DIR}/backend/src/main/resources/schema.sql"
fi

# 配置 Nginx 与 Systemd
cp -f "${BUILD_DIR}/deploy/flowsight.conf" /etc/nginx/sites-available/flowsight.conf
ln -sf /etc/nginx/sites-available/flowsight.conf /etc/nginx/sites-enabled/flowsight.conf
nginx -t && systemctl reload nginx

cp -f "${BUILD_DIR}/deploy/flowsight-backend.service" /etc/systemd/system/flowsight-backend.service
systemctl daemon-reload
systemctl enable flowsight-backend
systemctl restart flowsight-backend

# 5. 探活检测
echo -e "${GREEN}[5/5] 执行健康检查...${NC}"
SUCCESS=0
for i in {1..30}; do
    if curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:8088/api/v1/auth/login | grep -qE "200|400|405"; then
        SUCCESS=1
        echo -e "${GREEN}✅ 后端与前端服务已成功在生产环境运行！${NC}"
        break
    fi
    echo -n "."
    sleep 2
done
echo ""

if [ $SUCCESS -ne 1 ]; then
    echo -e "${RED}后端启动检测超时，请检查 /opt/flowsight/logs/backend.error.log${NC}"
    exit 1
fi

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}       🎉 现场编译与部署运行全部成功完成！                     ${NC}"
echo -e "${CYAN}================================================================${NC}"
