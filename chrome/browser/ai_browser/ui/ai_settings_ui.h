// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_UI_AI_SETTINGS_UI_H_
#define CHROME_BROWSER_AI_BROWSER_UI_AI_SETTINGS_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace ai_browser {

// WebUI controller for chrome://ai-settings.
// Provides configuration interface for the AI Browser service.
class AISettingsUI : public content::WebUIController {
 public:
  explicit AISettingsUI(content::WebUI* web_ui);
  ~AISettingsUI() override;

  AISettingsUI(const AISettingsUI&) = delete;
  AISettingsUI& operator=(const AISettingsUI&) = delete;
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_UI_AI_SETTINGS_UI_H_
