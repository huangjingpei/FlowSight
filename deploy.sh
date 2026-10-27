#!/bin/bash
# ==============================================================================
# FlowSight 根目录快捷部署入口
# 执行方式: ./deploy.sh 或 bash deploy.sh
# ==============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec bash "${SCRIPT_DIR}/deploy/deploy.sh" "$@"
