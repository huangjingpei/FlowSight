#!/bin/bash
# ==============================================================================
# FlowSight 边云协同智能物联与视觉多租户平台 - 自适应一键部署脚本 (Universal Deployer)
# 支持环境:
#   1. Windows Git Shell / Git Bash (本地一键打包、SFTP上传、远程构建与验证)
#   2. Linux Server (Ubuntu 22.04 LTS / Debian 11+ 服务器端本地部署)
# 生产域名: https://flowsight.1t90.com
# 目标服务器: 124.221.179.44
# ==============================================================================

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OS_TYPE="$(uname -s)"

# ==============================================================================
# 模式 A: Windows Git Shell / Git Bash 本地执行模式
# ==============================================================================
case "${OS_TYPE}" in
    MINGW*|MSYS*|CYGWIN*|Windows_NT*)
        echo -e "${CYAN}================================================================${NC}"
        echo -e "${CYAN}  FlowSight 远程自动化部署流水线 (Windows Git Shell 本地模式)   ${NC}"
        echo -e "${CYAN}  目标服务器: root@124.221.179.44 | 域名: https://flowsight.1t90.com  ${NC}"
        echo -e "${CYAN}================================================================${NC}"
        
        # 寻找本地 Python 解释器
        PYTHON_BIN=""
        if command -v python >/dev/null 2>&1; then
            PYTHON_BIN="python"
        elif command -v python3 >/dev/null 2>&1; then
            PYTHON_BIN="python3"
        elif [ -f "/c/Users/Administrator/AppData/Local/Programs/Python/Python312/python.exe" ]; then
            PYTHON_BIN="/c/Users/Administrator/AppData/Local/Programs/Python/Python312/python.exe"
        fi

        if [ -n "${PYTHON_BIN}" ]; then
            # 检查 paramiko
            if ! ${PYTHON_BIN} -c "import paramiko" >/dev/null 2>&1; then
                echo -e "${YELLOW}[提示] 正在为本地 Python 安装 paramiko 远程通信组件...${NC}"
                ${PYTHON_BIN} -m pip install paramiko
            fi
            
            REMOTE_SCRIPT="${SCRIPT_DIR}/remote_deploy.py"
            if [ ! -f "${REMOTE_SCRIPT}" ]; then
                REMOTE_SCRIPT="${SCRIPT_DIR}/deploy/remote_deploy.py"
            fi
            
            exec ${PYTHON_BIN} "${REMOTE_SCRIPT}" "$@"
        else
            echo -e "${RED}[错误] 未在系统 PATH 中检测到 Python。请安装 Python 3 或将其添加到环境变量。${NC}"
            exit 1
        fi
        ;;
esac

# ==============================================================================
# 模式 B: Linux 远程服务器本地执行模式
# ==============================================================================
log_info() {
    echo -e "${GREEN}[FlowSight-INFO]${NC} $1"
}
log_warn() {
    echo -e "${YELLOW}[FlowSight-WARN]${NC} $1"
}
log_error() {
    echo -e "${RED}[FlowSight-ERROR]${NC} $1"
}

TARGET_DIR="/opt/flowsight"
MYSQL_USER="flowsight"
MYSQL_PASS="ZXCVbnm,=-09"
MYSQL_DB="flowsight_cloud"
BACKEND_PORT=8088

echo -e "${CYAN}================================================================${NC}"
echo -e "${CYAN}    FlowSight 边-云-端协同智能物联平台 自动化生产部署引擎       ${NC}"
echo -e "${CYAN}================================================================${NC}"

# 1. 检查并自适应提权 (非强制硬报错退出，普通用户执行时自动提权)
if [ "$EUID" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        log_info "检测到当前为普通用户，自动调用 sudo 提权执行..."
        exec sudo -E bash "$0" "$@"
    else
        log_warn "当前为非 root 用户且系统未安装 sudo，将尝试以当前权限继续执行..."
    fi
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
    
    # 导入 Schema (采用 CREATE TABLE IF NOT EXISTS，绝不删除既有数据)
    if [ -f "${SCRIPT_DIR}/schema.sql" ]; then
        mysql --defaults-file=/etc/mysql/debian.cnf ${MYSQL_DB} < "${SCRIPT_DIR}/schema.sql"
        log_info "表结构架构 schema.sql 检查完成"
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
    log_info "视频切片存证流初始化完成 (${TARGET_DIR}/clips)"
fi

# 6. 部署后端 Spring Boot 生产 Jar
log_info "5/7 部署 Java 后端服务与生产配置..."
if [ -f "${SCRIPT_DIR}/flowsight-backend.jar" ]; then
    cp -f "${SCRIPT_DIR}/flowsight-backend.jar" "${TARGET_DIR}/flowsight-backend.jar"
fi
if [ -f "${SCRIPT_DIR}/application-prod.yml" ]; then
    cp -f "${SCRIPT_DIR}/application-prod.yml" "${TARGET_DIR}/application-prod.yml"
fi

# 7. 配置 Nginx 反向代理与 SSL
log_info "6/7 配置 Nginx 站点与 SSL 反向代理..."
if [ -f "${SCRIPT_DIR}/flowsight.conf" ]; then
    cp -f "${SCRIPT_DIR}/flowsight.conf" /etc/nginx/sites-available/flowsight.conf
    ln -sf /etc/nginx/sites-available/flowsight.conf /etc/nginx/sites-enabled/flowsight.conf
    nginx -t
    systemctl reload nginx
    log_info "Nginx 配置重载成功"
fi

# 8. 配置并重启 Systemd 后台服务
log_info "7/7 配置 Systemd 守护进程并启动服务..."
if [ -f "${SCRIPT_DIR}/flowsight-backend.service" ]; then
    cp -f "${SCRIPT_DIR}/flowsight-backend.service" /etc/systemd/system/flowsight-backend.service
    systemctl daemon-reload
    systemctl enable flowsight-backend
    systemctl restart flowsight-backend
    log_info "flowsight-backend 守护服务已重启"
fi

# 9. 服务健康自检探活
log_info "正在进行服务健康探活检测..."
SUCCESS=0
for i in {1..30}; do
    if curl -s -o /dev/null -w "%{http_code}" http://127.0.0.1:${BACKEND_PORT}/api/v1/auth/login | grep -qE "200|400|405"; then
        SUCCESS=1
        echo ""
        log_info "✅ 后端接口与网关探活响应正常 (HTTP 200/400)！"
        break
    fi
    echo -n "."
    sleep 2
done

if [ $SUCCESS -ne 1 ]; then
    log_error "后端服务在 60 秒内未就绪，请使用 'journalctl -u flowsight-backend -n 50' 排查日志"
    exit 1
fi

echo -e "${GREEN}================================================================${NC}"
echo -e "${GREEN}  🎉 FlowSight 平台部署完成！服务运行状态良好                   ${NC}"
echo -e "${GREEN}================================================================${NC}"
echo -e "  • 官方 HTTPS 生产域名: https://flowsight.1t90.com/"
echo -e "  • 移动端 H5 免证书端口: http://124.221.179.44:3000/"
echo -e "  • PC 管理后台免证书端口: http://124.221.179.44:3001/"
echo -e "  • 统一导航门户端口:     http://124.221.179.44:8888/"
echo -e "${GREEN}================================================================${NC}"
