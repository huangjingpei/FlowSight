#!/bin/bash
# ==============================================================================
# FlowSight 边云协同智能物联与视觉多租户平台 - 一键自动化部署脚本
# 适用环境: Ubuntu 22.04 LTS / Debian 11+
# 域名: https://flowsight.1t90.com
# ==============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[FlowSight-INFO]${NC} $1"
}
log_warn() {
    echo -e "${YELLOW}[FlowSight-WARN]${NC} $1"
}
log_error() {
    echo -e "${RED}[FlowSight-ERROR]${NC} $1"
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_DIR="/opt/flowsight"
MYSQL_USER="flowsight"
MYSQL_PASS="ZXCVbnm,=-09"
MYSQL_DB="flowsight_cloud"
BACKEND_PORT=8088

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}    FlowSight 边-云-端协同智能物联平台 自动化生产部署引擎       ${NC}"
echo -e "${CYAN}================================================================${NC}"

# 1. 检查 root 执行权限
if [ "$EUID" -ne 0 ]; then
    log_error "请使用 root 用户或 sudo 执行本部署脚本！"
    exit 1
fi

# 2. 检查并安装 Java 17 运行时
log_info "1/7 检查 Java 17 运行时环境..."
if [ ! -f "/usr/lib/jvm/java-17-openjdk-amd64/bin/java" ]; then
    log_warn "未检测到 Java 17，正在通过 apt 安装 openjdk-17-jre-headless..."
    apt-get update -y
    apt-get install -y openjdk-17-jre-headless
fi
JAVA_BIN="/usr/lib/jvm/java-17-openjdk-amd64/bin/java"
log_info "Java 17 运行时就绪: $($JAVA_BIN -version 2>&1 | head -n 1)"

# 3. 创建生产目录骨架
log_info "2/7 初始化运行目录: ${TARGET_DIR}..."
mkdir -p "${TARGET_DIR}/h5"
mkdir -p "${TARGET_DIR}/admin"
mkdir -p "${TARGET_DIR}/clips"
mkdir -p "${TARGET_DIR}/logs"
mkdir -p /etc/ssl/1t90.com

# 4. 初始化 MySQL 数据库与数据字典
log_info "3/7 初始化 MySQL 业务数据库与种子数据..."
if [ -f "/etc/mysql/debian.cnf" ]; then
    mysql --defaults-file=/etc/mysql/debian.cnf -e "CREATE DATABASE IF NOT EXISTS ${MYSQL_DB} CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
    mysql --defaults-file=/etc/mysql/debian.cnf -e "CREATE USER IF NOT EXISTS '${MYSQL_USER}'@'localhost' IDENTIFIED BY '${MYSQL_PASS}';"
    mysql --defaults-file=/etc/mysql/debian.cnf -e "CREATE USER IF NOT EXISTS '${MYSQL_USER}'@'%' IDENTIFIED BY '${MYSQL_PASS}';"
    mysql --defaults-file=/etc/mysql/debian.cnf -e "GRANT ALL PRIVILEGES ON ${MYSQL_DB}.* TO '${MYSQL_USER}'@'localhost';"
    mysql --defaults-file=/etc/mysql/debian.cnf -e "GRANT ALL PRIVILEGES ON ${MYSQL_DB}.* TO '${MYSQL_USER}'@'%';"
    mysql --defaults-file=/etc/mysql/debian.cnf -e "FLUSH PRIVILEGES;"
    
    # 导入 Schema 与种子测试数据
    if [ -f "${SCRIPT_DIR}/schema.sql" ]; then
        mysql --defaults-file=/etc/mysql/debian.cnf ${MYSQL_DB} < "${SCRIPT_DIR}/schema.sql"
        log_info "表结构架构 schema.sql 导入完成"
    fi
    if [ -f "${SCRIPT_DIR}/data_init.sql" ]; then
        mysql --defaults-file=/etc/mysql/debian.cnf ${MYSQL_DB} < "${SCRIPT_DIR}/data_init.sql"
        log_info "预置多租户种子数据 data_init.sql 导入完成"
    fi
else
    log_warn "未找到 debian.cnf，使用标准 mysql 用户验证..."
    mysql -u${MYSQL_USER} -p${MYSQL_PASS} -e "CREATE DATABASE IF NOT EXISTS ${MYSQL_DB} CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;" || true
fi

# 5. 部署前端静态资源与现场切片
log_info "4/7 部署前端静态产物与音视频资源..."
if [ -d "${SCRIPT_DIR}/h5" ]; then
    cp -rf "${SCRIPT_DIR}/h5/"* "${TARGET_DIR}/h5/"
    log_info "H5 / 数字大屏静态包更新完成 (${TARGET_DIR}/h5)"
fi
if [ -d "${SCRIPT_DIR}/admin" ]; then
    cp -rf "${SCRIPT_DIR}/admin/"* "${TARGET_DIR}/admin/"
    log_info "PC 管理中台静态包更新完成 (${TARGET_DIR}/admin)"
fi
if [ -d "${SCRIPT_DIR}/clips" ]; then
    cp -rf "${SCRIPT_DIR}/clips/"* "${TARGET_DIR}/clips/"
    log_info "事件抓拍短视频回放池更新完成 (${TARGET_DIR}/clips)"
fi

# 6. 配置 SSL 证书与 Nginx 反向代理
log_info "5/7 配置 SSL 证书与 Nginx 路由矩阵..."
CERT_FILE="/etc/ssl/1t90.com/flowsight.1t90.com_bundle.pem"
KEY_FILE="/etc/ssl/1t90.com/flowsight.1t90.com.key"

if [ ! -f "${CERT_FILE}" ] || [ ! -f "${KEY_FILE}" ]; then
    log_warn "未检测到已存在的 flowsight.1t90.com SSL 证书，正在自动签发自签名证书保障 HTTPS 握手..."
    openssl req -x509 -nodes -days 3650 -newkey rsa:2048 \
        -keyout "${KEY_FILE}" \
        -out "${CERT_FILE}" \
        -subj "/C=CN/ST=Zhejiang/L=Hangzhou/O=FlowSight/OU=Cloud/CN=flowsight.1t90.com" \
        -addext "subjectAltName=DNS:flowsight.1t90.com,DNS:localhost,IP:124.221.179.44"
fi

if [ -f "${SCRIPT_DIR}/flowsight.conf" ]; then
    cp -f "${SCRIPT_DIR}/flowsight.conf" /etc/nginx/sites-available/flowsight.conf
    ln -sf /etc/nginx/sites-available/flowsight.conf /etc/nginx/sites-enabled/flowsight.conf
fi

nginx -t
systemctl reload nginx
log_info "Nginx 路由与证书重载就绪！"

# 7. 部署 Java 后端微服务与 Systemd 守护进程
log_info "6/7 部署云端 Java 后端微服务..."
if [ -f "${SCRIPT_DIR}/flowsight-backend.jar" ]; then
    cp -f "${SCRIPT_DIR}/flowsight-backend.jar" "${TARGET_DIR}/flowsight-backend.jar"
fi
if [ -f "${SCRIPT_DIR}/application-prod.yml" ]; then
    cp -f "${SCRIPT_DIR}/application-prod.yml" "${TARGET_DIR}/application-prod.yml"
fi

if [ -f "${SCRIPT_DIR}/flowsight-backend.service" ]; then
    cp -f "${SCRIPT_DIR}/flowsight-backend.service" /etc/systemd/system/flowsight-backend.service
    systemctl daemon-reload
    systemctl enable flowsight-backend
    systemctl restart flowsight-backend
    log_info "FlowSight 后端 systemd 守护进程已启动！"
fi

# 8. 服务健康探测与冒烟测试
log_info "7/7 执行服务健康探测与端口核验..."
SUCCESS=0
for i in {1..30}; do
    if curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:${BACKEND_PORT}/api/v1/auth/login | grep -qE "200|400|405"; then
        SUCCESS=1
        break
    fi
    echo -n "."
    sleep 2
done
echo ""

if [ $SUCCESS -eq 1 ]; then
    log_info "✅ FlowSight 云端后端服务已成功启动并就绪 (Port: ${BACKEND_PORT})"
else
    log_error "❌ 后端启动检测超时，请检查日志: tail -n 50 /opt/flowsight/logs/backend.error.log"
    exit 1
fi

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}       🎉 FlowSight 边云协同物联网 SaaS 平台部署成功！          ${NC}"
echo -e "${GREEN}================================================================${NC}"
echo -e "访问地址列表:"
echo -e "  🌐 官方生产 HTTPS 门户:  ${CYAN}https://flowsight.1t90.com/${NC}"
echo -e "  💻 PC 运营管理中台:      ${CYAN}https://flowsight.1t90.com/admin/${NC}"
echo -e "  📱 H5 移动决策与数字大屏: ${CYAN}https://flowsight.1t90.com/#/screen${NC}"
echo -e "  ⚡ 独立端口快速通道:"
echo -e "     - H5 / 数字大屏通道:  ${BLUE}http://124.221.179.44:3000/${NC}"
echo -e "     - PC 管理后台通道:    ${BLUE}http://124.221.179.44:3001/${NC}"
echo -e "     - 聚合调度网关:       ${BLUE}http://124.221.179.44:8888/${NC}"
echo -e "----------------------------------------------------------------"
echo -e "预置系统账号:"
echo -e "  - 平台超级管理员: 手机号 ${YELLOW}13800000000${NC} / 密码 ${YELLOW}13800000000${NC}"
echo -e "  - 商户 1 (未来科技城): 手机号 ${YELLOW}13454118763${NC} / 密码 ${YELLOW}13454118763${NC}"
echo -e "  - 商户 2 (杭州栩文科技): 手机号 ${YELLOW}13454118762${NC} / 密码 ${YELLOW}13454118762${NC}"
echo -e "${GREEN}================================================================${NC}"
