// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/session_persistence.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/ai_browser/ai_browser_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ai_browser {

class SessionPersistenceTest : public testing::Test {
 protected:
  void SetUp() override {
    // Register the main prefs.
    RegisterProfilePrefs(prefs_.registry());
    // Register the dict prefs used internally by SessionPersistence.
    prefs_.registry()->RegisterDictionaryPref("ai_browser.sessions");
    prefs_.registry()->RegisterDictionaryPref("ai_browser.credentials");
  }

  TestingPrefServiceSimple prefs_;
};

// === Session (Cookie) Tests ===

TEST_F(SessionPersistenceTest, LoadSessionReturnsNulloptWhenNotStored) {
  SessionPersistence sp(&prefs_);
  EXPECT_EQ(sp.LoadSession("example.com"), std::nullopt);
}

TEST_F(SessionPersistenceTest, SaveAndLoadSession) {
  SessionPersistence sp(&prefs_);
  sp.SaveSession("example.com", R"({"cookie":"abc123"})");

  auto loaded = sp.LoadSession("example.com");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(*loaded, R"({"cookie":"abc123"})");
}

TEST_F(SessionPersistenceTest, SaveSessionOverwritesExisting) {
  SessionPersistence sp(&prefs_);
  sp.SaveSession("example.com", "old_cookies");
  sp.SaveSession("example.com", "new_cookies");

  auto loaded = sp.LoadSession("example.com");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(*loaded, "new_cookies");
}

TEST_F(SessionPersistenceTest, ClearSessionRemovesEntry) {
  SessionPersistence sp(&prefs_);
  sp.SaveSession("example.com", "cookies");
  sp.ClearSession("example.com");

  EXPECT_EQ(sp.LoadSession("example.com"), std::nullopt);
}

TEST_F(SessionPersistenceTest, ClearSessionDoesNotAffectOtherDomains) {
  SessionPersistence sp(&prefs_);
  sp.SaveSession("a.com", "cookies_a");
  sp.SaveSession("b.com", "cookies_b");
  sp.ClearSession("a.com");

  EXPECT_EQ(sp.LoadSession("a.com"), std::nullopt);
  ASSERT_TRUE(sp.LoadSession("b.com").has_value());
  EXPECT_EQ(*sp.LoadSession("b.com"), "cookies_b");
}

TEST_F(SessionPersistenceTest, MultipleDomainsSessions) {
  SessionPersistence sp(&prefs_);
  sp.SaveSession("a.com", "1");
  sp.SaveSession("b.com", "2");
  sp.SaveSession("c.com", "3");

  EXPECT_EQ(*sp.LoadSession("a.com"), "1");
  EXPECT_EQ(*sp.LoadSession("b.com"), "2");
  EXPECT_EQ(*sp.LoadSession("c.com"), "3");
}

TEST_F(SessionPersistenceTest, ClearNonexistentSessionIsNoOp) {
  SessionPersistence sp(&prefs_);
  // Should not crash.
  sp.ClearSession("nonexistent.com");
  EXPECT_EQ(sp.LoadSession("nonexistent.com"), std::nullopt);
}

// === Credential Tests ===

TEST_F(SessionPersistenceTest, GetCredentialsReturnsNulloptWhenNotStored) {
  SessionPersistence sp(&prefs_);
  EXPECT_EQ(sp.GetCredentials("example.com"), std::nullopt);
}

TEST_F(SessionPersistenceTest, StoreAndGetCredentials) {
  SessionPersistence sp(&prefs_);
  sp.StoreCredentials("example.com", "user@test.com", "p@ssw0rd!");

  auto creds = sp.GetCredentials("example.com");
  ASSERT_TRUE(creds.has_value());
  EXPECT_EQ(creds->first, "user@test.com");
  EXPECT_EQ(creds->second, "p@ssw0rd!");
}

TEST_F(SessionPersistenceTest, CredentialsAreObfuscatedInStorage) {
  SessionPersistence sp(&prefs_);
  sp.StoreCredentials("example.com", "admin", "secret");

  // Read the raw stored value — it should NOT be plaintext.
  const base::Value::Dict& raw =
      prefs_.GetDict("ai_browser.credentials");
  const base::Value::Dict* cred = raw.FindDict("example.com");
  ASSERT_NE(cred, nullptr);

  const std::string* stored_user = cred->FindString("username");
  const std::string* stored_pass = cred->FindString("password");
  ASSERT_NE(stored_user, nullptr);
  ASSERT_NE(stored_pass, nullptr);

  // Stored values must differ from plaintext.
  EXPECT_NE(*stored_user, "admin");
  EXPECT_NE(*stored_pass, "secret");
}

TEST_F(SessionPersistenceTest, ObfuscationRoundtrip) {
  SessionPersistence sp(&prefs_);

  // Test with various string types.
  std::vector<std::pair<std::string, std::string>> test_cases = {
      {"user", "pass"},
      {"", ""},  // empty strings
      {"user@special!#$%", "p@ss_w0rd!&*()"},  // special chars
      {"用户名", "密码"},  // CJK characters
      {std::string(1000, 'a'), std::string(1000, 'b')},  // long strings
  };

  for (size_t i = 0; i < test_cases.size(); ++i) {
    const auto& [user, pass] = test_cases[i];
    std::string domain = "domain" + std::to_string(i) + ".com";
    sp.StoreCredentials(domain, user, pass);

    auto retrieved = sp.GetCredentials(domain);
    ASSERT_TRUE(retrieved.has_value())
        << "Failed for domain: " << domain;
    EXPECT_EQ(retrieved->first, user)
        << "Username mismatch for domain: " << domain;
    EXPECT_EQ(retrieved->second, pass)
        << "Password mismatch for domain: " << domain;
  }
}

TEST_F(SessionPersistenceTest, StoreCredentialsOverwritesExisting) {
  SessionPersistence sp(&prefs_);
  sp.StoreCredentials("example.com", "old_user", "old_pass");
  sp.StoreCredentials("example.com", "new_user", "new_pass");

  auto creds = sp.GetCredentials("example.com");
  ASSERT_TRUE(creds.has_value());
  EXPECT_EQ(creds->first, "new_user");
  EXPECT_EQ(creds->second, "new_pass");
}

TEST_F(SessionPersistenceTest, ClearCredentialsRemovesEntry) {
  SessionPersistence sp(&prefs_);
  sp.StoreCredentials("example.com", "user", "pass");
  sp.ClearCredentials("example.com");

  EXPECT_EQ(sp.GetCredentials("example.com"), std::nullopt);
}

TEST_F(SessionPersistenceTest, ClearCredentialsDoesNotAffectOtherDomains) {
  SessionPersistence sp(&prefs_);
  sp.StoreCredentials("a.com", "u1", "p1");
  sp.StoreCredentials("b.com", "u2", "p2");
  sp.ClearCredentials("a.com");

  EXPECT_EQ(sp.GetCredentials("a.com"), std::nullopt);
  auto creds = sp.GetCredentials("b.com");
  ASSERT_TRUE(creds.has_value());
  EXPECT_EQ(creds->first, "u2");
}

TEST_F(SessionPersistenceTest, ClearNonexistentCredentialsIsNoOp) {
  SessionPersistence sp(&prefs_);
  sp.ClearCredentials("nonexistent.com");
  EXPECT_EQ(sp.GetCredentials("nonexistent.com"), std::nullopt);
}

// === GetStoredDomains Tests ===

TEST_F(SessionPersistenceTest, GetStoredDomainsEmptyWhenNone) {
  SessionPersistence sp(&prefs_);
  auto domains = sp.GetStoredDomains();
  EXPECT_TRUE(domains.empty());
}

TEST_F(SessionPersistenceTest, GetStoredDomainsReturnsSavedDomains) {
  SessionPersistence sp(&prefs_);
  sp.StoreCredentials("a.com", "u1", "p1");
  sp.StoreCredentials("b.com", "u2", "p2");
  sp.StoreCredentials("c.com", "u3", "p3");

  auto domains = sp.GetStoredDomains();
  EXPECT_EQ(domains.size(), 3u);

  // Sort for stable comparison.
  std::sort(domains.begin(), domains.end());
  EXPECT_EQ(domains[0], "a.com");
  EXPECT_EQ(domains[1], "b.com");
  EXPECT_EQ(domains[2], "c.com");
}

TEST_F(SessionPersistenceTest, GetStoredDomainsUpdatesAfterClear) {
  SessionPersistence sp(&prefs_);
  sp.StoreCredentials("a.com", "u1", "p1");
  sp.StoreCredentials("b.com", "u2", "p2");
  sp.ClearCredentials("a.com");

  auto domains = sp.GetStoredDomains();
  EXPECT_EQ(domains.size(), 1u);
  EXPECT_EQ(domains[0], "b.com");
}

// === Cross-instance Persistence ===

TEST_F(SessionPersistenceTest, DataSurvivesNewInstance) {
  {
    SessionPersistence sp(&prefs_);
    sp.SaveSession("example.com", "cookies");
    sp.StoreCredentials("example.com", "user", "pass");
  }  // sp destroyed

  // Create a new instance backed by the same prefs.
  SessionPersistence sp2(&prefs_);
  EXPECT_EQ(*sp2.LoadSession("example.com"), "cookies");
  auto creds = sp2.GetCredentials("example.com");
  ASSERT_TRUE(creds.has_value());
  EXPECT_EQ(creds->first, "user");
  EXPECT_EQ(creds->second, "pass");
}

// === Session and Credential Independence ===

TEST_F(SessionPersistenceTest, SessionsAndCredentialsAreIndependent) {
  SessionPersistence sp(&prefs_);
  sp.SaveSession("example.com", "session_data");
  sp.StoreCredentials("example.com", "user", "pass");

  // Clearing session should not affect credentials.
  sp.ClearSession("example.com");
  EXPECT_EQ(sp.LoadSession("example.com"), std::nullopt);
  auto creds = sp.GetCredentials("example.com");
  ASSERT_TRUE(creds.has_value());
  EXPECT_EQ(creds->first, "user");

  // Clearing credentials should not affect sessions (already cleared, but
  // test with a new entry).
  sp.SaveSession("example.com", "new_session");
  sp.ClearCredentials("example.com");
  EXPECT_EQ(sp.GetCredentials("example.com"), std::nullopt);
  ASSERT_TRUE(sp.LoadSession("example.com").has_value());
}

}  // namespace ai_browser
