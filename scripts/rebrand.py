#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
FlowSight 边云协同智能物联平台 - OEM 品牌定制与重命名脚本
魏总定制分支专属工具 (Branch: wei)

功能说明:
1. 一键安全替换所有前端 (Admin / H5 / 数字大屏)、桌面端 (C++ Qt) 及初始数据中的品牌名称
2. 严格保护底层网络协议、API 接口、LocalStorage Token 存储键、生产域名 flowsight.1t90.com
3. 支持自定义品牌名称，例如:
     python scripts/rebrand.py "客流智能统计系统"
     python scripts/rebrand.py "AI客流感知大脑"
     python scripts/rebrand.py --restore       (一键还原为官方默认 FlowSight 品牌)
"""

import sys
import os
import re

# 统一编码设置
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')

# 切换到工程根目录
script_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.dirname(script_dir) if os.path.basename(script_dir) == "scripts" else script_dir
os.chdir(project_root)

# 获取命令行参数
target_brand = sys.argv[1].strip() if len(sys.argv) > 1 else "客流智能统计系统"
restore_mode = (target_brand in ["--restore", "-r", "FlowSight", "flowsight"])

if restore_mode:
    target_brand = "FlowSight"
    print("\n🔄 触发还原模式: 正在将品牌名称还原为官方原始标识 [ FlowSight ]...\n")
else:
    print(f"\n🚀 触发定制模式: 正在将品牌名称切换为 [ {target_brand} ]...\n")

def replace_in_file(file_path, replacements):
    full_path = os.path.join(project_root, file_path)
    if not os.path.exists(full_path):
        print(f"  ⚠️ 文件不存在，跳过: {file_path}")
        return False
    
    with open(full_path, 'r', encoding='utf-8') as f:
        content = f.read()

    original_content = content
    for pattern, repl in replacements:
        content = re.sub(pattern, repl, content)

    if content != original_content:
        with open(full_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"  ✅ [已更新] {file_path}")
        return True
    else:
        print(f"  ⏭️ [无需修改] {file_path}")
        return False

# ----------------- 1. Admin 运营管理平台 -----------------
print("📦 [1/4] 配置 PC 云端运营管理平台 (Admin Web)...")
admin_index = "solution/apps/admin/index.html"
admin_title = "FlowSight 智能客流云端运营管理平台" if restore_mode else f"{target_brand} · 云端运营管理平台"
replace_in_file(admin_index, [
    (r"<title>.*?</title>", f"<title>{admin_title}</title>")
])

admin_layout = "solution/apps/admin/src/views/Layout.vue"
admin_logo = "FlowSight Cloud" if restore_mode else target_brand
replace_in_file(admin_layout, [
    (r'<span class="logo-text">.*?</span>', f'<span class="logo-text">{admin_logo}</span>')
])

admin_login = "solution/apps/admin/src/views/Login.vue"
admin_login_h2 = "FlowSight 运营管理平台" if restore_mode else f"{target_brand} 运营管理平台"
replace_in_file(admin_login, [
    (r'<h2>.*?运营管理平台</h2>', f'<h2>{admin_login_h2}</h2>')
])

admin_biz = "solution/apps/admin/src/views/BusinessTypeManage.vue"
admin_biz_repl = "现场边缘端（FlowSight.exe）的算法模型" if restore_mode else f"现场边缘端（{target_brand}客户端）的算法模型"
replace_in_file(admin_biz, [
    (r'现场边缘端（.*?）的算法模型', admin_biz_repl)
])

admin_point = "solution/apps/admin/src/views/PointManage.vue"
admin_point_repl = "现场绑定的 FlowSight.exe 客户端在同步配置时" if restore_mode else f"现场绑定的边缘采集客户端（{target_brand}）在同步配置时"
replace_in_file(admin_point, [
    (r'现场绑定的.*?在同步配置时', admin_point_repl)
])

# ----------------- 2. H5 移动决策看板与数字大屏 -----------------
print("\n📱 [2/4] 配置 H5 移动决策中台与数字大屏...")
h5_index = "solution/apps/h5/index.html"
h5_title = "FlowSight 移动决策看板" if restore_mode else f"{target_brand} · 移动决策看板"
replace_in_file(h5_index, [
    (r"<title>.*?</title>", f"<title>{h5_title}</title>")
])

h5_router = "solution/apps/h5/src/router/index.ts"
h5_screen_title = "FlowSight 智能视觉数字全景大屏" if restore_mode else f"{target_brand} · 智能视觉数字全景大屏"
replace_in_file(h5_router, [
    (r"title:\s*['\"].*?智能视觉数字全景大屏['\"]", f"title: '{h5_screen_title}'")
])

h5_login = "solution/apps/h5/src/views/Login.vue"
h5_login_h1 = "FlowSight · 移动决策中台" if restore_mode else f"{target_brand} · 移动决策中台"
h5_login_foot = "FlowSight v1.2 · 边云协同视觉计算" if restore_mode else f"{target_brand} v1.2 · 边云协同视觉计算"
replace_in_file(h5_login, [
    (r'<h1>.*?· 移动决策中台</h1>', f'<h1>{h5_login_h1}</h1>'),
    (r'<span>.*? v1\.2 · 边云协同视觉计算</span>', f'<span>{h5_login_foot}</span>')
])

h5_dash = "solution/apps/h5/src/views/Dashboard.vue"
dash_company = "FlowSight 实时监控" if restore_mode else f"{target_brand} 实时监控"
replace_in_file(h5_dash, [
    (r"currentCompany\.value \|\| '.*?'", f"currentCompany.value || '{dash_company}'"),
    (r"summary\.companyName \|\| '.*?'", f"summary.companyName || '{dash_company}'")
])

h5_screen = "solution/apps/h5/src/views/DigitalScreen.vue"
screen_opt = "🌐 FlowSight · 全域指挥中心" if restore_mode else f"🌐 {target_brand} · 全域指挥中心"
screen_title_main = "FlowSight · 全域智能视觉大屏" if restore_mode else f"{target_brand} · 全域智能视觉大屏"
replace_in_file(h5_screen, [
    (r'🌐 .*? · 全域指挥中心', screen_opt),
    (r"screenTitle\.value = .*? \|\| '.*? · 全域智能视觉大屏'", f"screenTitle.value = route.query.title as string || '{screen_title_main}'"),
    (r"screenTitle\.value = .*? \|\| '.*? · 视觉分析调度中心'", f"screenTitle.value = route.query.title as string || '{target_brand} · 视觉分析调度中心'")
])

h5_settings = "solution/apps/h5/src/views/Settings.vue"
settings_node = "FlowSight-Edge-Node-01" if restore_mode else "客流采集节点-01"
settings_logout = "确定要退出当前 FlowSight 决策账号吗？" if restore_mode else f"确定要退出当前{target_brand}账号吗？"
settings_guide = "选择每次登录或重新打开 FlowSight 时优先默认进入的业务通道。" if restore_mode else "选择每次登录或重新打开系统时优先默认进入的业务通道。"
replace_in_file(h5_settings, [
    (r"summary\.clientName \|\| '.*?'", f"summary.clientName || '{settings_node}'"),
    (r"message:\s*['\"]确定要退出当前.*?账号吗？['\"]", f"message: '{settings_logout}'"),
    (r"选择每次登录或重新打开.*?时优先默认进入的业务通道。", settings_guide)
])

# ----------------- 3. C++ 桌面客户端 -----------------
print("\n🖥️ [3/4] 配置 Windows C++ 桌面客户端...")
win_cpp = "src/ui/MainWindow.cpp"
win_title = "FlowSight 智能客流分析系统 v1.2" if restore_mode else f"{target_brand} v1.2"
win_brand = "FLOWSIGHT" if restore_mode else target_brand
replace_in_file(win_cpp, [
    (r'setWindowTitle\(["\'].*? v1\.2["\']\);', f'setWindowTitle("{win_title}");'),
    (r'brand_title = new QLabel\(["\'].*?["\'], brand_widget\);', f'brand_title = new QLabel("{win_brand}", brand_widget);')
])

canvas_cpp = "src/ui/VideoCanvas.cpp"
canvas_watermark = "FlowSight AI 智能客流感知系统" if restore_mode else target_brand
replace_in_file(canvas_cpp, [
    (r'painter\.drawText\(content_x, content_y \+ 20, ["\'].*?["\']\);', f'painter.drawText(content_x, content_y + 20, "{canvas_watermark}");')
])

# ----------------- 4. 数据库初始数据 -----------------
print("\n🗄️ [4/4] 配置数据库初始化种子数据...")
db_schema = "solution/backend/src/main/resources/schema.sql"
db_admin_corp = "FlowSight SaaS 运营指挥中心" if restore_mode else f"{target_brand} 运营指挥中心"
replace_in_file(db_schema, [
    (r"'系统管理员',\s*'FlowSight SaaS 运营指挥中心'", f"'系统管理员', '{db_admin_corp}'"),
    (r"'系统管理员',\s*'.*? 运营指挥中心'", f"'系统管理员', '{db_admin_corp}'")
])

db_data = "solution/backend/src/main/resources/data_init.sql"
replace_in_file(db_data, [
    (r"'系统管理员','FlowSight SaaS 运营指挥中心'", f"'系统管理员','{db_admin_corp}'"),
    (r"'系统管理员','.*? 运营指挥中心'", f"'系统管理员','{db_admin_corp}'")
])

print(f"\n🎉 品牌定制处理完成！当前品牌: [ {target_brand} ]")
print("提示: 如需重新构建并发布前端，请执行:")
print("  cd solution/apps/admin && npm run build")
print("  cd solution/apps/h5 && npm run build")
