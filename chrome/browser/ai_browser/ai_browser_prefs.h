// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_AI_BROWSER_PREFS_H_
#define CHROME_BROWSER_AI_BROWSER_AI_BROWSER_PREFS_H_

class PrefRegistrySimple;

namespace ai_browser {

// Pref paths
extern const char kLLMApiKey[];
extern const char kLLMModel[];
extern const char kLLMEndpoint[];
extern const char kLLMMaxTokens[];
extern const char kOCREndpoint[];
extern const char kOCRLang[];
extern const char kMaxConcurrentTabs[];
extern const char kPageLoadTimeout[];
extern const char kStealthModeEnabled[];
extern const char kAutoScrollEnabled[];
extern const char kApiServerPort[];
extern const char kApiServerEnabled[];

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_AI_BROWSER_PREFS_H_
