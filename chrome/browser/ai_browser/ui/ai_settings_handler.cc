// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ui/ai_settings_handler.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/ai_browser/ai_browser_prefs.h"
#include "chrome/browser/ai_browser/ai_client.h"
#include "chrome/browser/ai_browser/session_persistence.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui.h"

namespace ai_browser {

AISettingsHandler::AISettingsHandler() = default;
AISettingsHandler::~AISettingsHandler() = default;

void AISettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getConfig",
      base::BindRepeating(&AISettingsHandler::HandleGetConfig,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "saveConfig",
      base::BindRepeating(&AISettingsHandler::HandleSaveConfig,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "testLLMConnection",
      base::BindRepeating(&AISettingsHandler::HandleTestLLMConnection,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "testOCRConnection",
      base::BindRepeating(&AISettingsHandler::HandleTestOCRConnection,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCredentials",
      base::BindRepeating(&AISettingsHandler::HandleGetCredentials,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "saveCredential",
      base::BindRepeating(&AISettingsHandler::HandleSaveCredential,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "deleteCredential",
      base::BindRepeating(&AISettingsHandler::HandleDeleteCredential,
                          base::Unretained(this)));
}

void AISettingsHandler::HandleGetConfig(const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  base::Value::Dict config;
  config.Set("llm_api_key", prefs->GetString(kLLMApiKey));
  config.Set("llm_model", prefs->GetString(kLLMModel));
  config.Set("llm_endpoint", prefs->GetString(kLLMEndpoint));
  config.Set("llm_max_tokens", prefs->GetInteger(kLLMMaxTokens));
  config.Set("ocr_endpoint", prefs->GetString(kOCREndpoint));
  config.Set("ocr_lang", prefs->GetString(kOCRLang));
  config.Set("max_concurrent_tabs", prefs->GetInteger(kMaxConcurrentTabs));
  config.Set("page_load_timeout", prefs->GetInteger(kPageLoadTimeout));
  config.Set("stealth_mode", prefs->GetBoolean(kStealthModeEnabled));
  config.Set("auto_scroll", prefs->GetBoolean(kAutoScrollEnabled));
  config.Set("api_server_port", prefs->GetInteger(kApiServerPort));
  config.Set("api_server_enabled", prefs->GetBoolean(kApiServerEnabled));

  AllowJavascript();
  FireWebUIListener("config-loaded", config);
}

void AISettingsHandler::HandleSaveConfig(const base::Value::List& args) {
  if (args.empty() || !args[0].is_dict()) return;
  const base::Value::Dict& config = args[0].GetDict();

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  if (auto* v = config.FindString("llm_api_key"))
    prefs->SetString(kLLMApiKey, *v);
  if (auto* v = config.FindString("llm_model"))
    prefs->SetString(kLLMModel, *v);
  if (auto* v = config.FindString("llm_endpoint"))
    prefs->SetString(kLLMEndpoint, *v);
  if (auto i = config.FindInt("llm_max_tokens"))
    prefs->SetInteger(kLLMMaxTokens, *i);
  if (auto* v = config.FindString("ocr_endpoint"))
    prefs->SetString(kOCREndpoint, *v);
  if (auto* v = config.FindString("ocr_lang"))
    prefs->SetString(kOCRLang, *v);
  if (auto i = config.FindInt("max_concurrent_tabs"))
    prefs->SetInteger(kMaxConcurrentTabs, *i);
  if (auto i = config.FindInt("page_load_timeout"))
    prefs->SetInteger(kPageLoadTimeout, *i);
  if (auto b = config.FindBool("stealth_mode"))
    prefs->SetBoolean(kStealthModeEnabled, *b);
  if (auto b = config.FindBool("auto_scroll"))
    prefs->SetBoolean(kAutoScrollEnabled, *b);
  if (auto i = config.FindInt("api_server_port"))
    prefs->SetInteger(kApiServerPort, *i);
  if (auto b = config.FindBool("api_server_enabled"))
    prefs->SetBoolean(kApiServerEnabled, *b);

  AllowJavascript();
  FireWebUIListener("config-saved", base::Value(true));
}

void AISettingsHandler::HandleTestLLMConnection(
    const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  auto url_loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  auto client = std::make_unique<AIClient>(prefs, url_loader_factory);
  // Note: The client will be destroyed when the callback fires,
  // but SimpleURLLoader prevents that via prevent-destroy semantics.
  AIClient* client_ptr = client.get();
  client_ptr->TestLLMConnection(base::BindOnce(
      &AISettingsHandler::OnLLMTestResult, weak_factory_.GetWeakPtr(),
      "llm-test-result"));
}

void AISettingsHandler::HandleTestOCRConnection(
    const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  auto url_loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  auto client = std::make_unique<AIClient>(prefs, url_loader_factory);
  AIClient* client_ptr = client.get();
  client_ptr->TestOCRConnection(base::BindOnce(
      &AISettingsHandler::OnOCRTestResult, weak_factory_.GetWeakPtr(),
      "ocr-test-result"));
}

void AISettingsHandler::OnLLMTestResult(const std::string& callback_id,
                                        bool success,
                                        std::string message) {
  AllowJavascript();
  base::Value::Dict result;
  result.Set("success", success);
  result.Set("message", message);
  FireWebUIListener(callback_id, result);
}

void AISettingsHandler::OnOCRTestResult(const std::string& callback_id,
                                        bool success,
                                        std::string message) {
  AllowJavascript();
  base::Value::Dict result;
  result.Set("success", success);
  result.Set("message", message);
  FireWebUIListener(callback_id, result);
}

void AISettingsHandler::HandleGetCredentials(const base::Value::List& args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  SessionPersistence sp(profile->GetPrefs());
  auto domains = sp.GetStoredDomains();

  base::Value::List cred_list;
  for (const auto& domain : domains) {
    auto creds = sp.GetCredentials(domain);
    if (creds) {
      base::Value::Dict item;
      item.Set("domain", domain);
      item.Set("username", creds->first);
      // Don't send password to frontend.
      item.Set("has_password", true);
      cred_list.Append(std::move(item));
    }
  }

  AllowJavascript();
  FireWebUIListener("credentials-loaded", cred_list);
}

void AISettingsHandler::HandleSaveCredential(const base::Value::List& args) {
  if (args.size() < 3) return;
  const std::string* domain = args[0].GetIfString();
  const std::string* username = args[1].GetIfString();
  const std::string* password = args[2].GetIfString();
  if (!domain || !username || !password) return;

  Profile* profile = Profile::FromWebUI(web_ui());
  SessionPersistence sp(profile->GetPrefs());
  sp.StoreCredentials(*domain, *username, *password);

  AllowJavascript();
  FireWebUIListener("credential-saved", base::Value(true));
}

void AISettingsHandler::HandleDeleteCredential(const base::Value::List& args) {
  if (args.empty()) return;
  const std::string* domain = args[0].GetIfString();
  if (!domain) return;

  Profile* profile = Profile::FromWebUI(web_ui());
  SessionPersistence sp(profile->GetPrefs());
  sp.ClearCredentials(*domain);

  AllowJavascript();
  FireWebUIListener("credential-deleted", base::Value(true));
}

}  // namespace ai_browser
