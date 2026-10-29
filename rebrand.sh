#!/usr/bin/env bash
# ==============================================================================
# FlowSight 边云协同智能物联平台 - OEM 品牌定制与重命名脚本 (rebrand.sh)
# 魏总定制分支专属工具 (Branch: wei)
#
# 用法示例:
#   bash rebrand.sh                      # 默认应用 "客流智能统计系统"
#   bash rebrand.sh "AI智能感知大脑"     # 自定义为其他品牌
#   bash rebrand.sh --restore            # 一键还原为官方默认 FlowSight 品牌
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if command -v python &>/dev/null; then
    python scripts/rebrand.py "$@"
elif command -v python3 &>/dev/null; then
    python3 scripts/rebrand.py "$@"
else
    echo "❌ [错误] 未检测到 Python 运行环境，请确保已安装 Python 并加入系统 PATH。"
    exit 1
fi
