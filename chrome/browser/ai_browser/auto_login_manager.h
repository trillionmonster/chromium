// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_BROWSER_AUTO_LOGIN_MANAGER_H_
#define CHROME_BROWSER_AI_BROWSER_AUTO_LOGIN_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace ai_browser {

class AIClient;
class SessionPersistence;

// Information about a detected login form on a page.
struct LoginFormInfo {
  std::string username_selector;
  std::string password_selector;
  std::string submit_selector;
  std::string form_action;
  bool has_captcha = false;
};

using LoginCheckCallback = base::OnceCallback<void(bool needs_login)>;
using LoginCallback = base::OnceCallback<void(bool success, std::string error)>;

// Manages automatic login for websites. Detects login forms, fills
// user-provided credentials, handles CAPTCHAs via AI vision, and persists
// sessions. This operates on the user's own stored credentials — analogous
// to a password manager with auto-fill capability.
class AutoLoginManager {
 public:
  AutoLoginManager(SessionPersistence* session_mgr, AIClient* ai_client);
  ~AutoLoginManager();

  AutoLoginManager(const AutoLoginManager&) = delete;
  AutoLoginManager& operator=(const AutoLoginManager&) = delete;

  // Check whether the current page is a login page.
  void CheckNeedsLogin(content::WebContents* web_contents,
                       LoginCheckCallback callback);

  // Attempt full login flow for |domain| using stored credentials.
  void PerformLogin(content::WebContents* web_contents,
                    const std::string& domain,
                    LoginCallback callback);

 private:
  // Inject detect_login.js and parse the result.
  void DetectLoginForm(content::RenderFrameHost* frame,
                       base::OnceCallback<void(std::optional<LoginFormInfo>)>
                           callback);

  // Fill username and password fields, then click submit.
  void FillAndSubmit(content::RenderFrameHost* frame,
                     const LoginFormInfo& form,
                     const std::string& username,
                     const std::string& password,
                     base::OnceCallback<void(bool)> callback);

  // If a CAPTCHA is detected, screenshot it and ask AI to solve.
  void HandleCaptcha(content::WebContents* web_contents,
                     base::OnceCallback<void(std::string answer)> callback);

  // Called after login form submission to verify success.
  void VerifyLoginSuccess(content::WebContents* web_contents,
                          const std::string& original_url,
                          LoginCallback callback);

  // Type text into a field character-by-character with small random delays,
  // similar to how a real user types.
  void TypeWithDelay(content::RenderFrameHost* frame,
                     const std::string& selector,
                     const std::string& text,
                     size_t char_index,
                     base::OnceClosure done_callback);

  raw_ptr<SessionPersistence> session_mgr_;
  raw_ptr<AIClient> ai_client_;

  base::WeakPtrFactory<AutoLoginManager> weak_factory_{this};
};

}  // namespace ai_browser

#endif  // CHROME_BROWSER_AI_BROWSER_AUTO_LOGIN_MANAGER_H_
