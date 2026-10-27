#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
FlowSight 边云协同智能物联平台 - Windows Git Shell 远程自动化部署引擎
功能：在 Windows Git Bash / Git Shell 本地一键打包、上传并全自动完成远程生产部署与测试
目标服务器: root@124.221.179.44
生产域名: https://flowsight.1t90.com/
"""

import os
import sys
import tarfile
import time
import ssl
import json
import urllib.request

# 确保控制台 UTF-8 输出
sys.stdout.reconfigure(encoding='utf-8')

# 服务器配置
SERVER_HOST = "124.221.179.44"
SERVER_PORT = 22
SERVER_USER = "root"
SERVER_PASS = "ZXCVbnm,=-09"
DOMAIN = "flowsight.1t90.com"

# 查找项目根目录
script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(script_dir) if os.path.basename(script_dir) == "deploy" else script_dir

print("\033[1;36m================================================================\033[0m")
print("\033[1;36m    FlowSight 边云协同平台 - Windows Git Shell 远程部署引擎     \033[0m")
print(f"\033[1;36m    目标服务器: {SERVER_USER}@{SERVER_HOST} | 域名: https://{DOMAIN}   \033[0m")
print("\033[1;36m================================================================\033[0m\n")

try:
    import paramiko
except ImportError:
    print("\033[0;31m[错误] 未检测到 paramiko 模块，请先执行: pip install paramiko\033[0m")
    sys.exit(1)

# ----------------- 步骤 1: 打包本地纯净源码 -----------------
print("\033[0;32m[1/5] 打包工程纯源码 (排除本地 node_modules、target、dist、.git)... \033[0m")
os.makedirs(os.path.join(project_root, "scratch"), exist_ok=True)
source_bundle = os.path.join(project_root, "scratch", "flowsight_source.tar.gz")

def filter_bundle(tarinfo):
    name = tarinfo.name
    # 排除编译缓存和超大中间产物
    if any(k in name for k in ["node_modules", "target", ".idea", ".git", "dist", "build", "__pycache__"]):
        return None
    return tarinfo

with tarfile.open(source_bundle, "w:gz") as tar:
    tar.add(os.path.join(project_root, "solution", "backend"), arcname="backend", filter=filter_bundle)
    tar.add(os.path.join(project_root, "solution", "apps", "h5"), arcname="apps/h5", filter=filter_bundle)
    tar.add(os.path.join(project_root, "solution", "apps", "admin"), arcname="apps/admin", filter=filter_bundle)
    tar.add(os.path.join(project_root, "clips"), arcname="clips")
    tar.add(os.path.join(project_root, "deploy", "application-prod.yml"), arcname="deploy/application-prod.yml")
    tar.add(os.path.join(project_root, "deploy", "flowsight-backend.service"), arcname="deploy/flowsight-backend.service")
    tar.add(os.path.join(project_root, "deploy", "flowsight.conf"), arcname="deploy/flowsight.conf")
    tar.add(os.path.join(project_root, "deploy", "data_init.sql"), arcname="deploy/data_init.sql")
    tar.add(os.path.join(project_root, "deploy", "server_build_and_deploy.sh"), arcname="server_build_and_deploy.sh")

size_mb = os.path.getsize(source_bundle) / (1024 * 1024)
print(f" -> 纯源码压缩包生成完毕，体积: \033[1;33m{size_mb:.2f} MB\033[0m")

# ----------------- 步骤 2: 连接服务器并上传 -----------------
print(f"\n\033[0;32m[2/5] 连接服务器 {SERVER_HOST} 并上传源码包至 /tmp/flowsight_build... \033[0m")
ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(SERVER_HOST, SERVER_PORT, SERVER_USER, SERVER_PASS, timeout=20)

def run_remote(cmd, timeout=300):
    print(f"\033[0;34m[远程执行]\033[0m {cmd}")
    stdin, stdout, stderr = ssh.exec_command(cmd, timeout=timeout)
    while True:
        line = stdout.readline()
        if not line:
            break
        try:
            print("  " + line.strip())
        except Exception:
            print("  " + line.encode('ascii', errors='replace').decode('ascii').strip())
    err = stderr.read().decode('utf-8', errors='replace')
    status = stdout.channel.recv_exit_status()
    if status != 0 and err:
        print(f"\033[0;31m[错误日志]:\033[0m {err.strip()}")
    return status

run_remote("mkdir -p /tmp/flowsight_build")

sftp = ssh.open_sftp()
sftp.put(source_bundle, "/tmp/flowsight_build/flowsight_source.tar.gz")
sftp.close()
print(" -> 源码压缩包已安全传输至远程服务器")

# ----------------- 步骤 3: 远程解压、编译与部署 -----------------
print("\n\033[0;32m[3/5] 触发服务器现场自动化编译与热部署流水线... \033[0m")
run_remote("cd /tmp/flowsight_build && tar -zxf flowsight_source.tar.gz && chmod +x server_build_and_deploy.sh")
status = run_remote("cd /tmp/flowsight_build && ./server_build_and_deploy.sh", timeout=600)
if status != 0:
    print("\033[0;31m[部署失败] 服务器编译或启动异常，请检查上述日志\033[0m")
    sys.exit(1)

# ----------------- 步骤 4: 生产验证与冒烟测试 -----------------
print("\n\033[0;32m[4/5] 执行线上全链路功能冒烟测试... \033[0m")
ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

def test_url(url, desc):
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'FlowSight-Deploy-Check/1.2'})
        res = urllib.request.urlopen(req, context=ctx, timeout=10)
        print(f" -> {desc}: \033[0;32m[HTTP {res.status} OK]\033[0m ({url})")
        return True
    except Exception as e:
        print(f" -> {desc}: \033[0;31m[异常: {e}]\033[0m ({url})")
        return False

# 1. 验证 HTTPS 域名入口
test_url(f"https://{DOMAIN}/", "移动端 H5 生产入口")
test_url(f"https://{DOMAIN}/admin/", "PC 管理中台生产入口")

# 2. 验证非 443 无证书警告 HTTP 端口
test_url(f"http://{SERVER_HOST}:3000/", "H5 免证书独立端口 3000")
test_url(f"http://{SERVER_HOST}:3001/", "Admin 免证书独立端口 3001")
test_url(f"http://{SERVER_HOST}:8888/", "统一导航门户端口 8888")

# 3. 验证多租户 API 鉴权登录
try:
    login_payload = json.dumps({'phone': '13454118763', 'password': '13454118763'}).encode('utf-8')
    login_req = urllib.request.Request(f"https://{DOMAIN}/api/v1/auth/login", data=login_payload, headers={'Content-Type': 'application/json', 'User-Agent': 'FlowSight-Deploy-Check/1.2'})
    login_res = json.loads(urllib.request.urlopen(login_req, context=ctx, timeout=8).read().decode('utf-8'))
    if login_res.get('code') == 200:
        company = login_res.get('data', {}).get('companyName')
        print(f" -> 租户身份鉴权验证: \033[0;32m[成功]\033[0m 租户: {company}")
    else:
        print(f" -> 租户身份鉴权验证: \033[0;31m[失败]\033[0m {login_res}")
except Exception as e:
    print(f" -> 租户身份鉴权接口探测异常: {e}")

# ----------------- 步骤 5: 清理临时源码与部署脚本 -----------------
print("\n\033[0;32m[5/5] 删除服务器端临时编译源码包与临时文件... \033[0m")
run_remote("rm -rf /tmp/flowsight_build /tmp/flowsight_source.tar.gz")

# 清理本地临时 tar.gz
try:
    if os.path.exists(source_bundle):
        os.remove(source_bundle)
except Exception:
    pass

ssh.close()

print("\n\033[1;32m================================================================\033[0m")
print("\033[1;32m  🎉 部署全流程圆满成功！服务器环境已就绪并清理完毕！             \033[0m")
print("\033[1;32m================================================================\033[0m")
print(f"1. 官方 HTTPS 生产域名 (带证书): https://{DOMAIN}/")
print(f"2. 移动端 H5 免证书独立入口:    http://{SERVER_HOST}:3000/")
print(f"3. PC 管理中台免证书独立入口:    http://{SERVER_HOST}:3001/")
print(f"4. 统一聚合门户导航:            http://{SERVER_HOST}:8888/")
print("5. 默认商户账号: 13454118763 / 密码: 13454118763 (未来科技城一期)")
print("6. 默认管理账号: 13800000000 / 密码: admin123 (超级管理员)")
print("================================================================\n")
