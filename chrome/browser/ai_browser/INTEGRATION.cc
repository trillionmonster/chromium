// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ==========================================================================
// CHROMIUM INTEGRATION PATCHES
// ==========================================================================
// This file documents the exact changes needed in existing Chromium source
// files to integrate the ai_browser module. Apply these changes to a full
// Chromium checkout (src/).
//
// These are NOT standalone files — they are patch descriptions for files
// that exist in the upstream Chromium tree.
// ==========================================================================

// --------------------------------------------------------------------------
// 1. chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc
// --------------------------------------------------------------------------
// Add include at the top (with other WebUI includes):
//
//   #include "chrome/browser/ai_browser/ui/ai_settings_ui.h"
//
// In ChromeWebUIControllerFactory::CreateWebUIControllerForURL(), add
// inside the URL matching block (alongside other chrome:// handlers):
//
//   if (url.host_piece() == "ai-settings") {
//     return std::make_unique<ai_browser::AISettingsUI>(web_ui);
//   }
//

// --------------------------------------------------------------------------
// 2. chrome/browser/prefs/browser_prefs.cc
// --------------------------------------------------------------------------
// Add include:
//
//   #include "chrome/browser/ai_browser/ai_browser_prefs.h"
//
// In RegisterProfilePrefs(), add:
//
//   ai_browser::RegisterProfilePrefs(registry);
//

// --------------------------------------------------------------------------
// 3. chrome/browser/chrome_content_browser_client.cc
// --------------------------------------------------------------------------
// Add include:
//
//   #include "chrome/browser/ai_browser/ai_browser_service.h"
//
// In ChromeContentBrowserClient::BrowserURLHandlerCreated() or an
// appropriate initialization point, ensure AIBrowserService is created
// for the profile. Alternatively, register it via a BrowserContextKeyedService
// factory (preferred approach):
//
//   // Option A: Register as KeyedService factory
//   // Create chrome/browser/ai_browser/ai_browser_service_factory.h/cc
//   // and register in chrome_browser_main_extra_parts.cc
//
//   // Option B: Direct initialization in profile creation
//   // In ProfileManager::DoFinalInitForServices():
//   //   ai_browser::AIBrowserService::GetOrCreate(profile);
//

// --------------------------------------------------------------------------
// 4. chrome/browser/BUILD.gn
// --------------------------------------------------------------------------
// In the main chrome_browser source_set deps, add:
//
//   deps += [
//     "//chrome/browser/ai_browser",
//   ]
//

// --------------------------------------------------------------------------
// 5. chrome/browser/ui/BUILD.gn
// --------------------------------------------------------------------------
// If ai_browser/ui is a separate build target, add:
//
//   deps += [
//     "//chrome/browser/ai_browser/ui",
//   ]
//

// --------------------------------------------------------------------------
// 6. chrome/app/generated_resources.grd
// --------------------------------------------------------------------------
// Add resource include for WebUI HTML:
//
//   <include name="IDR_AI_SETTINGS_HTML"
//            file="../browser/ai_browser/ui/resources/ai_settings.html"
//            type="BINDATA" />
//   <include name="IDR_AI_SETTINGS_CSS"
//            file="../browser/ai_browser/ui/resources/ai_settings.css"
//            type="BINDATA" />
//   <include name="IDR_AI_SETTINGS_JS"
//            file="../browser/ai_browser/ui/resources/ai_settings.js"
//            type="BINDATA" />
//
// Or create a dedicated GRD file (see ai_settings_resources.grd below).
//

// --------------------------------------------------------------------------
// 7. chrome/browser/resources/BUILD.gn  (alternative: WebUI data source)
// --------------------------------------------------------------------------
// Modern Chromium WebUI uses webui_resources rather than GRD. An alternative
// approach is to use generate_grd + build_webui targets:
//
//   import("//ui/webui/resources/tools/build_webui.gni")
//
//   build_webui("ai_settings") {
//     grd_prefix = "ai_settings"
//     non_web_component_files = [
//       "ai_settings.html",
//       "ai_settings.css",
//       "ai_settings.js",
//     ]
//   }
//

// --------------------------------------------------------------------------
// 8. chrome/browser/about_flags.cc (optional)
// --------------------------------------------------------------------------
// To gate the feature behind a flag:
//
//   {"ai-browser",
//    "AI Browser",
//    "Enables AI Browser features including intelligent page reading, "
//    "auto-login, and search summarization.",
//    kOsDesktop,
//    FEATURE_VALUE_TYPE(features::kAIBrowser)},
//

// --------------------------------------------------------------------------
// 9. chrome/common/webui_url_constants.h
// --------------------------------------------------------------------------
// Add URL constant:
//
//   inline constexpr char kChromeUIAISettingsHost[] = "ai-settings";
//   inline constexpr char kChromeUIAISettingsURL[] = "chrome://ai-settings/";
//
