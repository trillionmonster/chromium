// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ai_browser_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ai_browser {

class AIBrowserPrefsTest : public testing::Test {
 protected:
  void SetUp() override {
    RegisterProfilePrefs(prefs_.registry());
  }

  TestingPrefServiceSimple prefs_;
};

// Verify all pref keys are registered and have expected defaults.

TEST_F(AIBrowserPrefsTest, LLMApiKeyDefaultsToEmpty) {
  EXPECT_EQ(prefs_.GetString(kLLMApiKey), "");
}

TEST_F(AIBrowserPrefsTest, LLMModelDefaultsToClaudeSonnet) {
  EXPECT_EQ(prefs_.GetString(kLLMModel), "claude-sonnet-4-20250514");
}

TEST_F(AIBrowserPrefsTest, LLMEndpointDefaultsToAnthropicAPI) {
  EXPECT_EQ(prefs_.GetString(kLLMEndpoint),
            "https://api.anthropic.com/v1/messages");
}

TEST_F(AIBrowserPrefsTest, LLMMaxTokensDefaultsTo4096) {
  EXPECT_EQ(prefs_.GetInteger(kLLMMaxTokens), 4096);
}

TEST_F(AIBrowserPrefsTest, OCREndpointDefaultsToLocalhost) {
  EXPECT_EQ(prefs_.GetString(kOCREndpoint), "http://localhost:8866");
}

TEST_F(AIBrowserPrefsTest, OCRLangDefaultsToChinese) {
  EXPECT_EQ(prefs_.GetString(kOCRLang), "ch");
}

TEST_F(AIBrowserPrefsTest, MaxConcurrentTabsDefaultsTo5) {
  EXPECT_EQ(prefs_.GetInteger(kMaxConcurrentTabs), 5);
}

TEST_F(AIBrowserPrefsTest, PageLoadTimeoutDefaultsTo30) {
  EXPECT_EQ(prefs_.GetInteger(kPageLoadTimeout), 30);
}

TEST_F(AIBrowserPrefsTest, StealthModeDefaultsToTrue) {
  EXPECT_TRUE(prefs_.GetBoolean(kStealthModeEnabled));
}

TEST_F(AIBrowserPrefsTest, AutoScrollDefaultsToTrue) {
  EXPECT_TRUE(prefs_.GetBoolean(kAutoScrollEnabled));
}

TEST_F(AIBrowserPrefsTest, ApiServerPortDefaultsTo9333) {
  EXPECT_EQ(prefs_.GetInteger(kApiServerPort), 9333);
}

TEST_F(AIBrowserPrefsTest, ApiServerEnabledDefaultsToTrue) {
  EXPECT_TRUE(prefs_.GetBoolean(kApiServerEnabled));
}

// Verify prefs can be read/written correctly.

TEST_F(AIBrowserPrefsTest, SetAndGetStringPref) {
  prefs_.SetString(kLLMApiKey, "sk-test-key-123");
  EXPECT_EQ(prefs_.GetString(kLLMApiKey), "sk-test-key-123");
}

TEST_F(AIBrowserPrefsTest, SetAndGetIntegerPref) {
  prefs_.SetInteger(kLLMMaxTokens, 8192);
  EXPECT_EQ(prefs_.GetInteger(kLLMMaxTokens), 8192);
}

TEST_F(AIBrowserPrefsTest, SetAndGetBooleanPref) {
  prefs_.SetBoolean(kStealthModeEnabled, false);
  EXPECT_FALSE(prefs_.GetBoolean(kStealthModeEnabled));
}

TEST_F(AIBrowserPrefsTest, OverwritePrefRetainsNewValue) {
  prefs_.SetString(kLLMModel, "claude-3-5-sonnet-20241022");
  prefs_.SetString(kLLMModel, "claude-opus-4-20250514");
  EXPECT_EQ(prefs_.GetString(kLLMModel), "claude-opus-4-20250514");
}

TEST_F(AIBrowserPrefsTest, AllPrefsAreIndependent) {
  prefs_.SetString(kLLMApiKey, "key1");
  prefs_.SetString(kLLMEndpoint, "http://custom-endpoint");
  prefs_.SetInteger(kMaxConcurrentTabs, 10);

  EXPECT_EQ(prefs_.GetString(kLLMApiKey), "key1");
  EXPECT_EQ(prefs_.GetString(kLLMEndpoint), "http://custom-endpoint");
  EXPECT_EQ(prefs_.GetInteger(kMaxConcurrentTabs), 10);
  // Other prefs unchanged.
  EXPECT_EQ(prefs_.GetString(kLLMModel), "claude-sonnet-4-20250514");
  EXPECT_EQ(prefs_.GetInteger(kPageLoadTimeout), 30);
}

}  // namespace ai_browser
