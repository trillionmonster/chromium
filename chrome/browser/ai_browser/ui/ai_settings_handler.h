// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_UI_AI_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_AI_BROWSER_UI_AI_SETTINGS_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ai_browser {

// Handles JavaScript → C++ messages for the chrome://ai-settings page.
class AISettingsHandler : public content::WebUIMessageHandler {
 public:
  AISettingsHandler();
  ~AISettingsHandler() override;

  AISettingsHandler(const AISettingsHandler&) = delete;
  AISettingsHandler& operator=(const AISettingsHandler&) = delete;

  // WebUIMessageHandler
  void RegisterMessages() override;

 private:
  // Message handlers called from JavaScript.
  void HandleGetConfig(const base::Value::List& args);
  void HandleSaveConfig(const base::Value::List& args);
  void HandleTestLLMConnection(const base::Value::List& args);
  void HandleTestOCRConnection(const base::Value::List& args);
  void HandleGetCredentials(const base::Value::List& args);
  void HandleSaveCredential(const base::Value::List& args);
  void HandleDeleteCredential(const base::Value::List& args);

  // Callback from AI client test.
  void OnLLMTestResult(const std::string& callback_id,
                       bool success,
                       std::string message);
  void OnOCRTestResult(const std::string& callback_id,
                       bool success,
                       std::string message);

  base::WeakPtrFactory<AISettingsHandler> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_UI_AI_SETTINGS_HANDLER_H_
