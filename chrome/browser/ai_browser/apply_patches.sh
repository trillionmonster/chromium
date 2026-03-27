#!/bin/bash
# =============================================================================
# apply_patches.sh — 自动将 ai_browser 集成到 Chromium 源码树
# =============================================================================
# 在完整的 Chromium src/ 目录中运行此脚本
# 用法: cd ~/chromium/src && bash chrome/browser/ai_browser/apply_patches.sh
# =============================================================================

set -e

CHROMIUM_SRC="$(pwd)"

echo "=== AI Browser Integration Patcher ==="
echo "Working in: $CHROMIUM_SRC"

# Verify we're in the right directory.
if [ ! -f "$CHROMIUM_SRC/chrome/browser/BUILD.gn" ]; then
  echo "ERROR: Run this script from the Chromium src/ root directory."
  echo "  cd ~/chromium/src && bash chrome/browser/ai_browser/apply_patches.sh"
  exit 1
fi

if [ ! -d "$CHROMIUM_SRC/chrome/browser/ai_browser" ]; then
  echo "ERROR: chrome/browser/ai_browser/ not found."
  echo "  Copy the ai_browser directory first."
  exit 1
fi

echo ""
echo "--- Patch 1: chrome/common/webui_url_constants.h ---"
URL_CONSTANTS="$CHROMIUM_SRC/chrome/common/webui_url_constants.h"
if grep -q "kChromeUIAISettingsHost" "$URL_CONSTANTS" 2>/dev/null; then
  echo "  Already patched, skipping."
else
  # Find the last inline constexpr line and append after it.
  # We add our constants near the end of the file, before the final #endif.
  sed -i.bak '/^#endif.*CHROME_COMMON_WEBUI_URL_CONSTANTS_H_/i \
\
// AI Browser settings page.\
inline constexpr char kChromeUIAISettingsHost[] = "ai-settings";\
inline constexpr char kChromeUIAISettingsURL[] = "chrome://ai-settings/";\
' "$URL_CONSTANTS"
  echo "  Patched: added kChromeUIAISettingsHost/URL constants."
fi

echo ""
echo "--- Patch 2: chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc ---"
FACTORY="$CHROMIUM_SRC/chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc"
if grep -q "ai_settings_ui.h" "$FACTORY" 2>/dev/null; then
  echo "  Already patched, skipping."
else
  # Add include at the top include block.
  sed -i.bak '/#include "chrome\/browser\/ui\/webui/a \
#include "chrome/browser/ai_browser/ui/ai_settings_ui.h"' "$FACTORY"

  # Add URL handler in CreateWebUIControllerForURL.
  # Look for a pattern like "if (url.host_piece() ==" and add our handler before the final else/return.
  sed -i.bak '/WebUIFactoryFunction GetWebUIFactoryFunction/,/^}/ {
    /return nullptr;/i \
  if (url.host_piece() == "ai-settings") {\
    return &NewWebUI<ai_browser::AISettingsUI>;\
  }
  }' "$FACTORY"
  echo "  Patched: registered chrome://ai-settings handler."
fi

echo ""
echo "--- Patch 3: chrome/browser/prefs/browser_prefs.cc ---"
BROWSER_PREFS="$CHROMIUM_SRC/chrome/browser/prefs/browser_prefs.cc"
if grep -q "ai_browser_prefs.h" "$BROWSER_PREFS" 2>/dev/null; then
  echo "  Already patched, skipping."
else
  # Add include.
  sed -i.bak '/#include "chrome\/browser\/prefetch/i \
#include "chrome/browser/ai_browser/ai_browser_prefs.h"' "$BROWSER_PREFS"

  # Add registration call in RegisterProfilePrefs.
  # Look for the function and add our call.
  sed -i.bak '/void RegisterProfilePrefs/,/^}/ {
    /^}$/i \
  ai_browser::RegisterProfilePrefs(registry);\
  // Register dict prefs used by SessionPersistence.\
  registry->RegisterDictionaryPref("ai_browser.sessions");\
  registry->RegisterDictionaryPref("ai_browser.credentials");
  }' "$BROWSER_PREFS"
  echo "  Patched: registered ai_browser profile prefs."
fi

echo ""
echo "--- Patch 4: chrome/browser/BUILD.gn ---"
BROWSER_BUILD="$CHROMIUM_SRC/chrome/browser/BUILD.gn"
if grep -q "ai_browser" "$BROWSER_BUILD" 2>/dev/null; then
  echo "  Already patched, skipping."
else
  # Add deps in the main source_set.
  # Find the first "deps = [" block and add our dependency.
  sed -i.bak '0,/deps = \[/{/deps = \[/a \
    "//chrome/browser/ai_browser",\
    "//chrome/browser/ai_browser:api",\
    "//chrome/browser/ai_browser:ui",
  }' "$BROWSER_BUILD"
  echo "  Patched: added ai_browser deps."
fi

echo ""
echo "=== All patches applied! ==="
echo ""
echo "Next steps:"
echo "  1. Review the .bak backup files if needed"
echo "  2. Run: gn gen out/Release --args='is_debug=false target_os=\"mac\" target_cpu=\"arm64\"'"
echo "  3. Run: autoninja -C out/Release chrome"
echo "  4. Run: open out/Release/Chromium.app"
echo "  5. Navigate to: chrome://ai-settings"
echo ""
echo "To run unit tests:"
echo "  autoninja -C out/Release ai_browser_unittests"
echo "  ./out/Release/ai_browser_unittests"
