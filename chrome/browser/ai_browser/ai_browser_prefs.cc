// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ai_browser_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace ai_browser {

const char kLLMApiKey[] = "ai_browser.llm.api_key";
const char kLLMModel[] = "ai_browser.llm.model";
const char kLLMEndpoint[] = "ai_browser.llm.endpoint";
const char kLLMMaxTokens[] = "ai_browser.llm.max_tokens";
const char kOCREndpoint[] = "ai_browser.ocr.endpoint";
const char kOCRLang[] = "ai_browser.ocr.lang";
const char kMaxConcurrentTabs[] = "ai_browser.max_concurrent_tabs";
const char kPageLoadTimeout[] = "ai_browser.page_load_timeout";
const char kStealthModeEnabled[] = "ai_browser.stealth_mode";
const char kAutoScrollEnabled[] = "ai_browser.auto_scroll";
const char kApiServerPort[] = "ai_browser.api_server_port";
const char kApiServerEnabled[] = "ai_browser.api_server_enabled";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // LLM configuration
  registry->RegisterStringPref(kLLMApiKey, std::string());
  registry->RegisterStringPref(kLLMModel, "claude-sonnet-4-20250514");
  registry->RegisterStringPref(kLLMEndpoint,
                               "https://api.anthropic.com/v1/messages");
  registry->RegisterIntegerPref(kLLMMaxTokens, 4096);

  // OCR configuration
  registry->RegisterStringPref(kOCREndpoint, "http://localhost:8866");
  registry->RegisterStringPref(kOCRLang, "ch");

  // Automation configuration
  registry->RegisterIntegerPref(kMaxConcurrentTabs, 5);
  registry->RegisterIntegerPref(kPageLoadTimeout, 30);
  registry->RegisterBooleanPref(kStealthModeEnabled, true);
  registry->RegisterBooleanPref(kAutoScrollEnabled, true);

  // API server configuration
  registry->RegisterIntegerPref(kApiServerPort, 9333);
  registry->RegisterBooleanPref(kApiServerEnabled, true);
}

}  // namespace ai_browser
