#!/bin/bash
# =========================================================
# setup_mac.sh — Mac 上编译 AI Browser
# =========================================================
# 用法: bash setup_mac.sh
# 前置条件: Xcode 已安装, 100GB+ 可用磁盘, 代理已配置
# =========================================================
set -e

# --- 代理配置（根据需要修改端口）---
export https_proxy=http://127.0.0.1:7890
export http_proxy=http://127.0.0.1:7890

echo "============================================"
echo "  AI Browser — macOS 编译环境搭建"
echo "============================================"
echo ""
echo "代理: $https_proxy"
echo ""

# --- Step 1: depot_tools ---
echo "[1/5] 检查 depot_tools ..."
export PATH="$HOME/depot_tools:$PATH"
if [ -d "$HOME/depot_tools" ] && [ -f "$HOME/depot_tools/gclient" ]; then
  echo "  ✓ 已安装"
else
  echo "  安装 depot_tools ..."
  rm -rf ~/depot_tools
  git clone https://github.com/nicedoc/depot_tools.git ~/depot_tools
  echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.zshrc
  echo "  ✓ 完成"
fi

# --- Step 2: 获取 Chromium 源码 + ai_browser ---
echo ""
echo "[2/5] 获取 Chromium 源码 ..."
CHROMIUM_DIR="$HOME/chromium"

if [ -f "$CHROMIUM_DIR/src/base/BUILD.gn" ]; then
  echo "  ✓ 源码已存在"
else
  rm -rf "$CHROMIUM_DIR"
  mkdir -p "$CHROMIUM_DIR"
  cd "$CHROMIUM_DIR"

  # 从你的 fork 浅克隆（已包含 ai_browser 模块）
  echo "  克隆主仓库 (shallow, 约 3-5GB) ..."
  git clone --depth 1 --no-tags \
    https://ghproxy.net/https://github.com/trillionmonster/chromium.git src

  # 如果 ghproxy 不通，自动切换
  if [ $? -ne 0 ]; then
    echo "  ghproxy 不通，尝试直连 ..."
    rm -rf src
    git clone --depth 1 --no-tags \
      https://github.com/trillionmonster/chromium.git src
  fi

  echo "  ✓ 主仓库克隆完成"

  # 创建 gclient 配置
  cat > .gclient << 'GCEOF'
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {},
  },
]
GCEOF

  # 拉取第三方依赖（v8, skia, angle 等）
  echo ""
  echo "  拉取第三方依赖 (gclient sync, 这一步最耗时) ..."
  echo "  预计 30-60 分钟，取决于网速"
  echo ""
  cd src
  gclient sync --no-history --shallow -j16 --force
  echo "  ✓ 依赖拉取完成"
fi

# --- Step 3: 应用集成补丁 ---
echo ""
echo "[3/5] 应用集成补丁 ..."
cd "$CHROMIUM_DIR/src"

if [ -f "chrome/browser/ai_browser/apply_patches.py" ]; then
  python3 chrome/browser/ai_browser/apply_patches.py
  echo "  ✓ 补丁完成"
else
  echo "  ✗ apply_patches.py 未找到！"
  echo "    请确认 chrome/browser/ai_browser/ 目录存在"
  exit 1
fi

# --- Step 4: 配置构建 ---
echo ""
echo "[4/5] 配置构建 ..."
cd "$CHROMIUM_DIR/src"

ARCH=$(uname -m)
if [ "$ARCH" = "arm64" ]; then
  CPU="arm64"
else
  CPU="x64"
fi
echo "  架构: $ARCH → target_cpu=$CPU"

gn gen out/Release --args="
  is_debug = false
  is_official_build = false
  target_cpu = \"$CPU\"
  symbol_level = 0
  enable_nacl = false
  blink_symbol_level = 0
  v8_symbol_level = 0
"
echo "  ✓ 构建配置完成"

# --- Step 5: 编译 ---
echo ""
echo "[5/5] 开始编译 ..."
echo "  这一步需要 2-6 小时，取决于机器性能"
echo ""
echo "  执行: autoninja -C out/Release chrome"
echo ""

autoninja -C out/Release chrome

echo ""
echo "============================================"
echo "  编译完成!"
echo ""
echo "  运行: open $CHROMIUM_DIR/src/out/Release/Chromium.app"
echo "  地址栏输入: chrome://ai-settings"
echo "============================================"
