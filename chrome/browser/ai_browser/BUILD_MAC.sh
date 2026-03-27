#!/bin/bash
# =============================================================================
# AI Browser — macOS 编译指南
# =============================================================================
# 在你的 Mac 上执行以下步骤来编译 AI Browser
#
# 前置条件:
#   - macOS 12+ (建议 13+)
#   - Xcode 15+ (从 App Store 安装)
#   - 至少 100GB 可用磁盘空间
#   - 16GB+ 内存 (建议 32GB)
# =============================================================================

set -e

echo "=== Step 1: 安装 depot_tools ==="
# depot_tools 包含 gclient, gn, autoninja 等 Chromium 构建工具
if [ ! -d "$HOME/depot_tools" ]; then
  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools
fi
export PATH="$HOME/depot_tools:$PATH"
# 建议加到 ~/.zshrc:
# echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.zshrc

echo "=== Step 2: 获取 Chromium 源码 ==="
mkdir -p ~/chromium && cd ~/chromium
# fetch 会下载完整源码 (~30GB), 需要较长时间
fetch --nohooks chromium

echo "=== Step 3: 安装依赖和运行 hooks ==="
cd ~/chromium/src
./build/install-build-deps.sh  # Linux only; Mac 只需 Xcode
gclient runhooks

echo "=== Step 4: 复制 ai_browser 模块 ==="
# 将我们写的代码复制到 Chromium 源码树中
# 假设你已经将 ai_browser 目录传输到了 Mac
cp -r /path/to/ai_browser ~/chromium/src/chrome/browser/ai_browser

echo "=== Step 5: 应用集成补丁 ==="
echo "请手动编辑以下文件 (参见 INTEGRATION.cc):"
echo ""
echo "  1. chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc"
echo "     → 添加 #include 和 chrome://ai-settings 路由"
echo ""
echo "  2. chrome/browser/prefs/browser_prefs.cc"
echo "     → 添加 ai_browser::RegisterProfilePrefs(registry) 调用"
echo ""
echo "  3. chrome/common/webui_url_constants.h"
echo "     → 添加 kChromeUIAISettingsHost 常量"
echo ""
echo "  4. chrome/browser/BUILD.gn"
echo "     → 添加 deps 中的 //chrome/browser/ai_browser"
echo ""
echo "详细修改内容见: chrome/browser/ai_browser/INTEGRATION.cc"

echo "=== Step 6: 生成构建配置 (Release for macOS) ==="
cd ~/chromium/src

gn gen out/Release --args='
  is_debug = false
  is_official_build = false
  target_os = "mac"
  target_cpu = "arm64"
  symbol_level = 0
  enable_nacl = false
  blink_symbol_level = 0
  v8_symbol_level = 0
  use_thin_lto = false
'
# 如果是 Intel Mac, 把 target_cpu 改为 "x64"

echo "=== Step 7: 编译 ==="
# 完整编译 Chrome 大约需要 2-6 小时 (取决于机器)
# -j 参数会被 autoninja 自动设置
autoninja -C out/Release chrome

echo "=== Step 8: 运行 ==="
# 编译完成后, .app 包在:
open out/Release/Chromium.app
# 然后在地址栏输入 chrome://ai-settings 配置 AI 服务

echo "=== Step 9: 运行单元测试 ==="
autoninja -C out/Release ai_browser_unittests
./out/Release/ai_browser_unittests

echo "=== 完成! ==="
