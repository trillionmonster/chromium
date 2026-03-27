// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai_browser/ai_browser_tab_helper.h"

#include "chrome/browser/ai_browser/ai_browser_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace ai_browser {

AIBrowserTabHelper::AIBrowserTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<AIBrowserTabHelper>(*web_contents) {}

AIBrowserTabHelper::~AIBrowserTabHelper() = default;

void AIBrowserTabHelper::AnalyzeCurrentPage(
    const std::string& question,
    base::OnceCallback<void(std::string)> callback) {
  // TODO: Get AIBrowserService from the profile's KeyedServiceFactory
  // and call AnalyzePage with the current URL.
  GURL url = web_contents()->GetLastCommittedURL();
  if (!url.is_valid()) {
    std::move(callback).Run("Error: No valid URL in current tab");
    return;
  }
  // Placeholder — in full implementation this would go through
  // AIBrowserServiceFactory to get the service instance.
  std::move(callback).Run("Analysis pending for: " + url.spec());
}

void AIBrowserTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  // Could trigger automatic analysis here based on user preferences.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AIBrowserTabHelper);

}  // namespace ai_browser
