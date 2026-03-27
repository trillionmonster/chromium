// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ui/ai_settings_ui.h"

#include "chrome/browser/ai_browser/ui/ai_settings_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/ai_settings_resources.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace ai_browser {

namespace {

void CreateAndAddDataSource(content::WebUI* web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(), "ai-settings");

  // Register resources from the GRD file.
  source->SetDefaultResource(IDR_AI_SETTINGS_HTML);
  source->AddResourcePath("ai_settings.css", IDR_AI_SETTINGS_CSS);
  source->AddResourcePath("ai_settings.js", IDR_AI_SETTINGS_JS);

  // Allow the page to make network requests (for connection testing).
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src 'self' http://localhost:* https://api.anthropic.com;");

  // Allow inline styles for the toggle switches and dynamic styling.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline';");
}

}  // namespace

AISettingsUI::AISettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<AISettingsHandler>());
  CreateAndAddDataSource(web_ui);
}

AISettingsUI::~AISettingsUI() = default;

}  // namespace ai_browser
