// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/auto_login_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ai_browser/ai_client.h"
#include "chrome/browser/ai_browser/session_persistence.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace ai_browser {

namespace {

// The detect_login.js script is injected to find login forms.
// It looks for <form> elements containing input[type=password], identifies
// the associated username field and submit button, and returns structured
// JSON describing what it found.
constexpr char kDetectLoginScript[] = R"js(
(function() {
  var results = {hasLoginForm: false, forms: []};
  var forms = document.querySelectorAll('form');
  for (var i = 0; i < forms.length; i++) {
    var form = forms[i];
    var pwdFields = form.querySelectorAll('input[type="password"]');
    if (pwdFields.length === 0) continue;

    var pwdField = pwdFields[0];
    var pwdSelector = '';
    if (pwdField.id) {
      pwdSelector = '#' + CSS.escape(pwdField.id);
    } else if (pwdField.name) {
      pwdSelector = 'input[name="' + CSS.escape(pwdField.name) + '"]';
    } else {
      pwdSelector = 'input[type="password"]';
    }

    // Find username: look for text/email input before the password field.
    var allInputs = form.querySelectorAll(
        'input[type="text"], input[type="email"], input:not([type])');
    var userField = null;
    for (var j = 0; j < allInputs.length; j++) {
      var inp = allInputs[j];
      if (inp.compareDocumentPosition(pwdField) &
          Node.DOCUMENT_POSITION_FOLLOWING) {
        userField = inp;
      }
    }

    var userSelector = '';
    if (userField) {
      if (userField.id) {
        userSelector = '#' + CSS.escape(userField.id);
      } else if (userField.name) {
        userSelector = 'input[name="' + CSS.escape(userField.name) + '"]';
      } else {
        userSelector = 'input[type="text"], input[type="email"]';
      }
    }

    // Find submit button.
    var submitBtn = form.querySelector(
        'button[type="submit"], input[type="submit"], button:not([type])');
    var submitSelector = '';
    if (submitBtn) {
      if (submitBtn.id) {
        submitSelector = '#' + CSS.escape(submitBtn.id);
      } else {
        submitSelector = 'button[type="submit"], input[type="submit"]';
      }
    }

    // Check for CAPTCHA elements.
    var hasCaptcha = !!(form.querySelector(
        '[class*="captcha"], [id*="captcha"], iframe[src*="captcha"], ' +
        'iframe[src*="recaptcha"], [class*="recaptcha"]'));

    results.hasLoginForm = true;
    results.forms.push({
      usernameSelector: userSelector,
      passwordSelector: pwdSelector,
      submitSelector: submitSelector,
      formAction: form.action || '',
      hasCaptcha: hasCaptcha
    });
  }

  // Also check for standalone password fields not in a form.
  if (!results.hasLoginForm) {
    var standalonePasswords = document.querySelectorAll(
        'input[type="password"]');
    if (standalonePasswords.length > 0) {
      results.hasLoginForm = true;
      results.forms.push({
        usernameSelector: '',
        passwordSelector: 'input[type="password"]',
        submitSelector: '',
        formAction: '',
        hasCaptcha: false
      });
    }
  }

  return JSON.stringify(results);
})();
)js";

LoginFormInfo ParseLoginFormInfo(const base::Value::Dict& dict) {
  LoginFormInfo info;
  if (const std::string* val = dict.FindString("usernameSelector")) {
    info.username_selector = *val;
  }
  if (const std::string* val = dict.FindString("passwordSelector")) {
    info.password_selector = *val;
  }
  if (const std::string* val = dict.FindString("submitSelector")) {
    info.submit_selector = *val;
  }
  if (const std::string* val = dict.FindString("formAction")) {
    info.form_action = *val;
  }
  info.has_captcha = dict.FindBool("hasCaptcha").value_or(false);
  return info;
}

}  // namespace

AutoLoginManager::AutoLoginManager(SessionPersistence* session_mgr,
                                   AIClient* ai_client)
    : session_mgr_(session_mgr), ai_client_(ai_client) {}

AutoLoginManager::~AutoLoginManager() = default;

void AutoLoginManager::CheckNeedsLogin(content::WebContents* web_contents,
                                       LoginCheckCallback callback) {
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  if (!frame) {
    std::move(callback).Run(false);
    return;
  }

  DetectLoginForm(
      frame,
      base::BindOnce(
          [](LoginCheckCallback cb, std::optional<LoginFormInfo> form_info) {
            std::move(cb).Run(form_info.has_value());
          },
          std::move(callback)));
}

void AutoLoginManager::PerformLogin(content::WebContents* web_contents,
                                    const std::string& domain,
                                    LoginCallback callback) {
  // 1. Get stored credentials.
  auto creds = session_mgr_->GetCredentials(domain);
  if (!creds.has_value()) {
    std::move(callback).Run(false, "No stored credentials for " + domain);
    return;
  }

  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  if (!frame) {
    std::move(callback).Run(false, "No main frame available");
    return;
  }

  const std::string& username = creds->first;
  const std::string& password = creds->second;
  std::string original_url = web_contents->GetLastCommittedURL().spec();

  // 2. Detect login form.
  DetectLoginForm(
      frame,
      base::BindOnce(
          &AutoLoginManager::OnLoginFormDetected, weak_factory_.GetWeakPtr(),
          web_contents, domain, username, password, original_url,
          std::move(callback)));
}

void AutoLoginManager::DetectLoginForm(
    content::RenderFrameHost* frame,
    base::OnceCallback<void(std::optional<LoginFormInfo>)> callback) {
  frame->ExecuteJavaScript(
      base::UTF8ToUTF16(kDetectLoginScript),
      base::BindOnce(
          [](base::OnceCallback<void(std::optional<LoginFormInfo>)> cb,
             base::Value result) {
            if (!result.is_string()) {
              std::move(cb).Run(std::nullopt);
              return;
            }
            auto parsed = base::JSONReader::Read(result.GetString());
            if (!parsed || !parsed->is_dict()) {
              std::move(cb).Run(std::nullopt);
              return;
            }
            const base::Value::Dict& dict = parsed->GetDict();
            if (!dict.FindBool("hasLoginForm").value_or(false)) {
              std::move(cb).Run(std::nullopt);
              return;
            }
            const base::Value::List* forms = dict.FindList("forms");
            if (!forms || forms->empty()) {
              std::move(cb).Run(std::nullopt);
              return;
            }
            const base::Value::Dict* first_form = (*forms)[0].GetIfDict();
            if (!first_form) {
              std::move(cb).Run(std::nullopt);
              return;
            }
            std::move(cb).Run(ParseLoginFormInfo(*first_form));
          },
          std::move(callback)));
}

void AutoLoginManager::FillAndSubmit(
    content::RenderFrameHost* frame,
    const LoginFormInfo& form,
    const std::string& username,
    const std::string& password,
    base::OnceCallback<void(bool)> callback) {
  // Type username with human-like delays.
  if (!form.username_selector.empty()) {
    // Focus the username field first.
    std::string focus_js = base::StringPrintf(
        "document.querySelector('%s').focus();",
        form.username_selector.c_str());
    frame->ExecuteJavaScript(base::UTF8ToUTF16(focus_js), base::DoNothing());

    TypeWithDelay(
        frame, form.username_selector, username, 0,
        base::BindOnce(
            &AutoLoginManager::OnUsernameFilled, weak_factory_.GetWeakPtr(),
            frame, form, password, std::move(callback)));
  } else {
    // No username field, just type password.
    OnUsernameFilled(frame, form, password, std::move(callback));
  }
}

void AutoLoginManager::TypeWithDelay(content::RenderFrameHost* frame,
                                     const std::string& selector,
                                     const std::string& text,
                                     size_t char_index,
                                     base::OnceClosure done_callback) {
  if (char_index >= text.size()) {
    std::move(done_callback).Run();
    return;
  }

  // Type one character via dispatching an input event.
  std::string current_value = text.substr(0, char_index + 1);
  std::string type_js = base::StringPrintf(
      R"js(
      (function() {
        var el = document.querySelector('%s');
        if (!el) return;
        el.value = '%s';
        el.dispatchEvent(new Event('input', {bubbles: true}));
      })();
      )js",
      selector.c_str(), current_value.c_str());
  frame->ExecuteJavaScript(base::UTF8ToUTF16(type_js), base::DoNothing());

  // Random delay between 50-200ms per keystroke.
  int delay_ms = base::RandInt(50, 200);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AutoLoginManager::TypeWithDelay,
                     weak_factory_.GetWeakPtr(), frame, selector, text,
                     char_index + 1, std::move(done_callback)),
      base::Milliseconds(delay_ms));
}

void AutoLoginManager::HandleCaptcha(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::string answer)> callback) {
  // Screenshot the page and ask AI to solve the CAPTCHA.
  content::RenderWidgetHostView* view =
      web_contents->GetRenderWidgetHostView();
  if (!view) {
    std::move(callback).Run(std::string());
    return;
  }

  view->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindOnce(
          [](base::WeakPtr<AutoLoginManager> self,
             base::OnceCallback<void(std::string)> cb,
             const SkBitmap& bitmap) {
            if (!self || bitmap.drawsNothing()) {
              std::move(cb).Run(std::string());
              return;
            }
            // Encode to PNG.
            std::vector<uint8_t> png_data;
            if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false,
                                                     &png_data)) {
              std::move(cb).Run(std::string());
              return;
            }
            self->ai_client_->SolveCaptcha(png_data, std::move(cb));
          },
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AutoLoginManager::VerifyLoginSuccess(content::WebContents* web_contents,
                                          const std::string& original_url,
                                          LoginCallback callback) {
  // Simple heuristic: if the URL changed away from the login page,
  // or if there's no longer a password field, login likely succeeded.
  std::string current_url = web_contents->GetLastCommittedURL().spec();
  if (current_url != original_url) {
    // URL changed — likely redirected after successful login.
    std::move(callback).Run(true, std::string());
    return;
  }

  // Check if login form is still present.
  CheckNeedsLogin(
      web_contents,
      base::BindOnce(
          [](LoginCallback cb, bool still_on_login) {
            if (still_on_login) {
              std::move(cb).Run(false,
                                "Login form still present after submission");
            } else {
              std::move(cb).Run(true, std::string());
            }
          },
          std::move(callback)));
}

// Private helper called after username is filled.
void AutoLoginManager::OnUsernameFilled(
    content::RenderFrameHost* frame,
    const LoginFormInfo& form,
    const std::string& password,
    base::OnceCallback<void(bool)> callback) {
  // Focus password field and type password.
  if (!form.password_selector.empty()) {
    std::string focus_js = base::StringPrintf(
        "document.querySelector('%s').focus();",
        form.password_selector.c_str());
    frame->ExecuteJavaScript(base::UTF8ToUTF16(focus_js), base::DoNothing());

    TypeWithDelay(
        frame, form.password_selector, password, 0,
        base::BindOnce(
            &AutoLoginManager::OnPasswordFilled, weak_factory_.GetWeakPtr(),
            frame, form, std::move(callback)));
  } else {
    std::move(callback).Run(false);
  }
}

void AutoLoginManager::OnPasswordFilled(
    content::RenderFrameHost* frame,
    const LoginFormInfo& form,
    base::OnceCallback<void(bool)> callback) {
  // Small delay before clicking submit.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](content::RenderFrameHost* frame, LoginFormInfo form,
             base::OnceCallback<void(bool)> cb) {
            std::string submit_js;
            if (!form.submit_selector.empty()) {
              submit_js = base::StringPrintf(
                  "document.querySelector('%s').click();",
                  form.submit_selector.c_str());
            } else {
              // Fallback: submit the form containing the password field.
              submit_js = base::StringPrintf(
                  R"js(
                  var pwd = document.querySelector('%s');
                  if (pwd && pwd.form) { pwd.form.submit(); }
                  )js",
                  form.password_selector.c_str());
            }
            frame->ExecuteJavaScript(base::UTF8ToUTF16(submit_js),
                                     base::DoNothing());
            std::move(cb).Run(true);
          },
          frame, form, std::move(callback)),
      base::Milliseconds(base::RandInt(200, 500)));
}

// This method is referenced in PerformLogin but defined here to avoid
// forward-declaration issues. It handles the result of form detection
// during the login flow.
void AutoLoginManager::OnLoginFormDetected(
    content::WebContents* web_contents,
    const std::string& domain,
    const std::string& username,
    const std::string& password,
    const std::string& original_url,
    LoginCallback callback,
    std::optional<LoginFormInfo> form_info) {
  if (!form_info.has_value()) {
    std::move(callback).Run(false, "Could not detect login form");
    return;
  }

  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  if (!frame) {
    std::move(callback).Run(false, "No main frame");
    return;
  }

  const LoginFormInfo& form = form_info.value();

  // Handle CAPTCHA if present.
  if (form.has_captcha) {
    HandleCaptcha(
        web_contents,
        base::BindOnce(
            [](base::WeakPtr<AutoLoginManager> self,
               content::RenderFrameHost* frame, LoginFormInfo form,
               std::string username, std::string password,
               content::WebContents* wc, std::string orig_url,
               LoginCallback cb, std::string captcha_answer) {
              if (!self) return;
              // Type CAPTCHA answer if we got one.
              if (!captcha_answer.empty()) {
                std::string captcha_js = base::StringPrintf(
                    R"js(
                    var captchaInput = document.querySelector(
                        'input[name*="captcha"], input[id*="captcha"]');
                    if (captchaInput) { captchaInput.value = '%s'; }
                    )js",
                    captcha_answer.c_str());
                frame->ExecuteJavaScript(base::UTF8ToUTF16(captcha_js),
                                         base::DoNothing());
              }
              // Now fill and submit.
              self->FillAndSubmit(
                  frame, form, username, password,
                  base::BindOnce(
                      [](base::WeakPtr<AutoLoginManager> self2,
                         content::WebContents* wc2, std::string orig_url2,
                         std::string domain2, LoginCallback cb2,
                         bool submitted) {
                        if (!self2 || !submitted) {
                          std::move(cb2).Run(false, "Failed to submit form");
                          return;
                        }
                        // Wait a bit for navigation, then verify.
                        base::SingleThreadTaskRunner::GetCurrentDefault()
                            ->PostDelayedTask(
                                FROM_HERE,
                                base::BindOnce(
                                    &AutoLoginManager::VerifyLoginSuccess,
                                    self2, wc2, orig_url2, std::move(cb2)),
                                base::Seconds(3));
                      },
                      self, wc, orig_url, domain, std::move(cb)));
            },
            weak_factory_.GetWeakPtr(), frame, form, username, password,
            web_contents, original_url, std::move(callback)));
    return;
  }

  // No CAPTCHA — fill and submit directly.
  FillAndSubmit(
      frame, form, username, password,
      base::BindOnce(
          [](base::WeakPtr<AutoLoginManager> self,
             content::WebContents* wc, std::string orig_url,
             std::string domain, LoginCallback cb, bool submitted) {
            if (!self || !submitted) {
              std::move(cb).Run(false, "Failed to submit form");
              return;
            }
            base::SingleThreadTaskRunner::GetCurrentDefault()
                ->PostDelayedTask(
                    FROM_HERE,
                    base::BindOnce(&AutoLoginManager::VerifyLoginSuccess,
                                   self, wc, orig_url, std::move(cb)),
                    base::Seconds(3));
          },
          weak_factory_.GetWeakPtr(), web_contents, original_url, domain,
          std::move(callback)));
}

}  // namespace ai_browser
