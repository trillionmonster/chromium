#!/usr/bin/env python3
"""
apply_patches.py — 将 ai_browser 模块集成到 Chromium 源码树
在完整的 Chromium src/ 目录中运行:
  cd ~/chromium/src && python3 chrome/browser/ai_browser/apply_patches.py
"""

import os
import sys
import re
import shutil

SRC = os.getcwd()

def check_env():
    if not os.path.isfile(os.path.join(SRC, 'chrome/browser/BUILD.gn')):
        sys.exit('ERROR: 请在 Chromium src/ 根目录运行此脚本')
    if not os.path.isdir(os.path.join(SRC, 'chrome/browser/ai_browser')):
        sys.exit('ERROR: chrome/browser/ai_browser/ 不存在，请先复制模块文件')

def backup_and_read(path):
    full = os.path.join(SRC, path)
    bak = full + '.ai_backup'
    if not os.path.exists(bak):
        shutil.copy2(full, bak)
    with open(full, 'r') as f:
        return f.read()

def write_file(path, content):
    with open(os.path.join(SRC, path), 'w') as f:
        f.write(content)

def patch_url_constants():
    """Patch 1: 添加 chrome://ai-settings URL 常量"""
    path = 'chrome/common/webui_url_constants.h'
    content = backup_and_read(path)
    marker = 'kChromeUIAISettingsHost'
    if marker in content:
        print(f'  [跳过] {path} 已包含 {marker}')
        return
    insert = '''
// AI Browser settings page.
inline constexpr char kChromeUIAISettingsHost[] = "ai-settings";
inline constexpr char kChromeUIAISettingsURL[] = "chrome://ai-settings/";

'''
    # 插入到文件末尾 #endif 之前
    content = content.rstrip()
    lines = content.split('\n')
    for i in range(len(lines) - 1, -1, -1):
        if lines[i].strip().startswith('#endif'):
            lines.insert(i, insert)
            break
    write_file(path, '\n'.join(lines) + '\n')
    print(f'  [完成] {path}')

def patch_controller_factory():
    """Patch 2: 注册 chrome://ai-settings WebUI 控制器"""
    path = 'chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc'
    content = backup_and_read(path)
    marker = 'ai_settings_ui.h'
    if marker in content:
        print(f'  [跳过] {path} 已包含 {marker}')
        return

    # 1) 添加 #include
    include_line = '#include "chrome/browser/ai_browser/ui/ai_settings_ui.h"\n'
    # 找到第一个 #include "chrome/browser 行后插入
    pos = content.find('#include "chrome/browser/')
    if pos >= 0:
        eol = content.index('\n', pos)
        content = content[:eol+1] + include_line + content[eol+1:]

    # 2) 在 GetWebUIFactoryFunction 中 "return nullptr;" 前插入路由
    handler_code = '''  if (url.host_piece() == "ai-settings")
    return &NewWebUI<ai_browser::AISettingsUI>;
'''
    # 找 GetWebUIFactoryFunction 函数体中的 return nullptr
    fn_match = re.search(r'GetWebUIFactoryFunction[^{]*\{', content)
    if fn_match:
        search_start = fn_match.end()
        nullptr_pos = content.find('return nullptr;', search_start)
        if nullptr_pos >= 0:
            content = content[:nullptr_pos] + handler_code + '\n  ' + content[nullptr_pos:]

    write_file(path, content)
    print(f'  [完成] {path}')

def patch_browser_prefs():
    """Patch 3: 注册 ai_browser 偏好设置"""
    path = 'chrome/browser/prefs/browser_prefs.cc'
    content = backup_and_read(path)
    marker = 'ai_browser_prefs.h'
    if marker in content:
        print(f'  [跳过] {path} 已包含 {marker}')
        return

    # 1) 添加 #include (在其他 chrome/browser include 附近)
    include_line = '#include "chrome/browser/ai_browser/ai_browser_prefs.h"\n'
    pos = content.find('#include "chrome/browser/')
    if pos >= 0:
        content = content[:pos] + include_line + content[pos:]

    # 2) 在 RegisterProfilePrefs 函数结尾 } 前插入调用
    reg_code = '''
  // AI Browser preferences.
  ai_browser::RegisterProfilePrefs(registry);
  registry->RegisterDictionaryPref("ai_browser.sessions");
  registry->RegisterDictionaryPref("ai_browser.credentials");
'''
    # 找 RegisterProfilePrefs 函数
    fn_match = re.search(r'void\s+RegisterProfilePrefs\s*\([^)]*\)\s*\{', content)
    if fn_match:
        # 找到对应的函数结束 }, 用简单的大括号计数
        start = fn_match.end()
        depth = 1
        i = start
        while i < len(content) and depth > 0:
            if content[i] == '{':
                depth += 1
            elif content[i] == '}':
                depth -= 1
            i += 1
        # i 现在指向函数结束 } 的下一个字符
        closing_brace = i - 1
        content = content[:closing_brace] + reg_code + content[closing_brace:]

    write_file(path, content)
    print(f'  [完成] {path}')

def patch_browser_build_gn():
    """Patch 4: 在 chrome/browser/BUILD.gn 中添加 ai_browser 依赖"""
    path = 'chrome/browser/BUILD.gn'
    content = backup_and_read(path)
    marker = '//chrome/browser/ai_browser'
    if marker in content:
        print(f'  [跳过] {path} 已包含 ai_browser 依赖')
        return

    dep_lines = '    "//chrome/browser/ai_browser",\n    "//chrome/browser/ai_browser:api",\n    "//chrome/browser/ai_browser:ui",\n'

    # 找第一个 source_set 或 static_library 的 deps = [ 块
    match = re.search(r'(source_set|static_library)\s*\(\s*"[^"]*"\s*\)\s*\{', content)
    if match:
        deps_pos = content.find('deps = [', match.end())
        if deps_pos >= 0:
            insert_pos = deps_pos + len('deps = [') + 1  # 跳过换行
            content = content[:insert_pos] + dep_lines + content[insert_pos:]

    write_file(path, content)
    print(f'  [完成] {path}')

def main():
    print('=' * 60)
    print('AI Browser — Chromium 集成补丁')
    print('=' * 60)
    print(f'源码目录: {SRC}')
    print()

    check_env()

    print('[1/4] URL 常量...')
    patch_url_constants()

    print('[2/4] WebUI 控制器工厂...')
    patch_controller_factory()

    print('[3/4] 偏好设置注册...')
    patch_browser_prefs()

    print('[4/4] BUILD.gn 依赖...')
    patch_browser_build_gn()

    print()
    print('=' * 60)
    print('补丁完成! 每个修改的文件都有 .ai_backup 备份')
    print()
    print('下一步:')
    print('  # 检查你的 Mac 芯片类型:')
    print('  uname -m')
    print()
    print('  # Apple Silicon (M1/M2/M3/M4):')
    print("  gn gen out/Release --args='is_debug=false target_cpu=\"arm64\" symbol_level=0 enable_nacl=false'")
    print()
    print('  # Intel Mac:')
    print("  gn gen out/Release --args='is_debug=false target_cpu=\"x64\" symbol_level=0 enable_nacl=false'")
    print()
    print('  # 编译:')
    print('  autoninja -C out/Release chrome')
    print()
    print('  # 运行:')
    print('  open out/Release/Chromium.app')
    print('  # 地址栏输入: chrome://ai-settings')
    print('=' * 60)

if __name__ == '__main__':
    main()
