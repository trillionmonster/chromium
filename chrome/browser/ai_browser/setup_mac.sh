#!/bin/bash
# =========================================================
# setup_mac.sh — Mac 上一键配置和编译 AI Browser
# =========================================================
# 用法: bash setup_mac.sh
# 前置条件: Xcode 已安装, 100GB+ 可用磁盘
# =========================================================
set -e

echo "============================================"
echo "  AI Browser — macOS 编译环境搭建"
echo "============================================"
echo ""

# --- Step 1: depot_tools ---
if command -v gclient &>/dev/null; then
  echo "[1/6] depot_tools 已安装 ✓"
else
  echo "[1/6] 安装 depot_tools ..."
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools
  export PATH="$HOME/depot_tools:$PATH"
  echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.zshrc
  echo "  → 已安装并写入 ~/.zshrc"
fi
export PATH="$HOME/depot_tools:$PATH"

# --- Step 2: 获取 Chromium 源码 ---
CHROMIUM_DIR="$HOME/chromium"
if [ -f "$CHROMIUM_DIR/src/BUILD.gn" ]; then
  echo "[2/6] Chromium 源码已存在 ✓"
else
  echo "[2/6] 获取 Chromium 源码 (约30GB，需要较长时间) ..."
  mkdir -p "$CHROMIUM_DIR"
  cd "$CHROMIUM_DIR"
  fetch --nohooks --no-history chromium
  echo "  → 源码下载完成"
fi

# --- Step 3: 安装依赖和 hooks ---
cd "$CHROMIUM_DIR/src"
echo "[3/6] 运行 gclient hooks ..."
gclient runhooks
echo "  → hooks 完成"

# --- Step 4: 复制 ai_browser 模块 ---
echo "[4/6] 安装 ai_browser 模块 ..."
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -d "$SCRIPT_DIR/chrome/browser/ai_browser" ]; then
  # 从解压的 tarball 目录复制
  cp -r "$SCRIPT_DIR/chrome/browser/ai_browser" "$CHROMIUM_DIR/src/chrome/browser/ai_browser"
elif [ -d "$SCRIPT_DIR/../ai_browser" ]; then
  # 从模块目录的父级复制
  cp -r "$SCRIPT_DIR/../ai_browser" "$CHROMIUM_DIR/src/chrome/browser/ai_browser"
else
  echo "  → ai_browser 模块已在源码树中或需要手动复制"
fi
echo "  → 模块安装完成"

# --- Step 5: 应用集成补丁 ---
echo "[5/6] 应用集成补丁 ..."
cd "$CHROMIUM_DIR/src"
python3 chrome/browser/ai_browser/apply_patches.py
echo "  → 补丁完成"

# --- Step 6: 配置并编译 ---
echo "[6/6] 配置构建 ..."
ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
  CPU="arm64"
else
  CPU="x64"
fi

gn gen out/Release --args="
  is_debug = false
  is_official_build = false
  target_cpu = \"$CPU\"
  symbol_level = 0
  enable_nacl = false
  blink_symbol_level = 0
  v8_symbol_level = 0
"

echo ""
echo "============================================"
echo "  配置完成! 开始编译:"
echo ""
echo "  cd $CHROMIUM_DIR/src"
echo "  autoninja -C out/Release chrome"
echo ""
echo "  编译完成后运行:"
echo "  open out/Release/Chromium.app"
echo "  # 地址栏输入: chrome://ai-settings"
echo "============================================"
