// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_SESSION_PERSISTENCE_H_
#define CHROME_BROWSER_AI_BROWSER_SESSION_PERSISTENCE_H_

#include <optional>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/values.h"

class PrefService;

namespace ai_browser {

// Manages persistence of browser sessions (cookies) and user credentials
// across browser restarts. Credentials are stored encrypted in the user's
// profile preferences.
class SessionPersistence {
 public:
  explicit SessionPersistence(PrefService* prefs);
  ~SessionPersistence();

  SessionPersistence(const SessionPersistence&) = delete;
  SessionPersistence& operator=(const SessionPersistence&) = delete;

  // Cookie/session management.
  void SaveSession(const std::string& domain,
                   const std::string& cookies_json);
  std::optional<std::string> LoadSession(const std::string& domain);
  void ClearSession(const std::string& domain);

  // Credential management.
  void StoreCredentials(const std::string& domain,
                        const std::string& username,
                        const std::string& password);
  std::optional<std::pair<std::string, std::string>> GetCredentials(
      const std::string& domain);
  void ClearCredentials(const std::string& domain);

  // Get list of all domains with stored credentials.
  std::vector<std::string> GetStoredDomains();

 private:
  // Simple obfuscation for credential storage. In production this should
  // use OS keychain or crypto::Encryptor with a hardware-derived key.
  std::string ObfuscateString(const std::string& input);
  std::string DeobfuscateString(const std::string& input);

  raw_ptr<PrefService> prefs_;
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_SESSION_PERSISTENCE_H_
