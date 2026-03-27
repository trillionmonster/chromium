// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/session_persistence.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ai_browser {

namespace {

const char kSessionsPref[] = "ai_browser.sessions";
const char kCredentialsPref[] = "ai_browser.credentials";

// Simple XOR-based obfuscation key. This is NOT cryptographic security —
// it's basic obfuscation to prevent casual reading of stored credentials.
// A production implementation should use OS keychain (Keyring on Linux,
// Credential Manager on Windows, Keychain on macOS).
const char kObfuscationKey[] = "ai_browser_credential_key_2024";

}  // namespace

SessionPersistence::SessionPersistence(PrefService* prefs) : prefs_(prefs) {}

SessionPersistence::~SessionPersistence() = default;

void SessionPersistence::SaveSession(const std::string& domain,
                                     const std::string& cookies_json) {
  ScopedDictPrefUpdate update(prefs_, kSessionsPref);
  update->Set(domain, cookies_json);
}

std::optional<std::string> SessionPersistence::LoadSession(
    const std::string& domain) {
  const base::Value::Dict& sessions = prefs_->GetDict(kSessionsPref);
  const std::string* value = sessions.FindString(domain);
  if (!value) {
    return std::nullopt;
  }
  return *value;
}

void SessionPersistence::ClearSession(const std::string& domain) {
  ScopedDictPrefUpdate update(prefs_, kSessionsPref);
  update->Remove(domain);
}

void SessionPersistence::StoreCredentials(const std::string& domain,
                                          const std::string& username,
                                          const std::string& password) {
  base::Value::Dict cred;
  cred.Set("username", ObfuscateString(username));
  cred.Set("password", ObfuscateString(password));

  ScopedDictPrefUpdate update(prefs_, kCredentialsPref);
  update->Set(domain, std::move(cred));
}

std::optional<std::pair<std::string, std::string>>
SessionPersistence::GetCredentials(const std::string& domain) {
  const base::Value::Dict& credentials = prefs_->GetDict(kCredentialsPref);
  const base::Value::Dict* cred = credentials.FindDict(domain);
  if (!cred) {
    return std::nullopt;
  }

  const std::string* username_enc = cred->FindString("username");
  const std::string* password_enc = cred->FindString("password");
  if (!username_enc || !password_enc) {
    return std::nullopt;
  }

  return std::make_pair(DeobfuscateString(*username_enc),
                        DeobfuscateString(*password_enc));
}

void SessionPersistence::ClearCredentials(const std::string& domain) {
  ScopedDictPrefUpdate update(prefs_, kCredentialsPref);
  update->Remove(domain);
}

std::vector<std::string> SessionPersistence::GetStoredDomains() {
  std::vector<std::string> domains;
  const base::Value::Dict& credentials = prefs_->GetDict(kCredentialsPref);
  for (const auto [key, value] : credentials) {
    domains.push_back(key);
  }
  return domains;
}

std::string SessionPersistence::ObfuscateString(const std::string& input) {
  std::string result = input;
  for (size_t i = 0; i < result.size(); ++i) {
    result[i] ^= kObfuscationKey[i % (sizeof(kObfuscationKey) - 1)];
  }
  return base::Base64Encode(result);
}

std::string SessionPersistence::DeobfuscateString(const std::string& input) {
  std::optional<std::vector<uint8_t>> decoded = base::Base64Decode(input);
  if (!decoded.has_value()) {
    return std::string();
  }
  std::string result(decoded->begin(), decoded->end());
  for (size_t i = 0; i < result.size(); ++i) {
    result[i] ^= kObfuscationKey[i % (sizeof(kObfuscationKey) - 1)];
  }
  return result;
}

}  // namespace ai_browser
